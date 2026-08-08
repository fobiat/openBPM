#!/usr/bin/env node
/**
 * Copy the repository's canonical docs into the Starlight content collection.
 *
 * `../docs/` is the source of truth and most of the audience reads it on
 * GitHub. Duplicating it into `src/content/` by hand would rot within a
 * release, so this script is the only thing that writes into
 * `src/content/docs/docs/`, that directory is gitignored, and the build runs
 * this first — the site cannot disagree with the repo.
 *
 * Unlike a hand-maintained manifest, this walks the docs tree and takes what it
 * finds, so a new document appears on the site without a second edit here. The
 * trade-off is that *excluding* something has to be deliberate: see EXCLUDE.
 *
 * What it does per file:
 *   1. Derives the slug from the path under docs/.
 *   2. Takes the title from the first H1, falling back to the filename.
 *   3. Strips that H1 — Starlight renders the title itself.
 *   4. Rewrites intra-repo .md links to site URLs, and anything it cannot map
 *      to an absolute GitHub URL, so no link 404s.
 */

import { readFile, writeFile, mkdir, rm, readdir } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { dirname, extname, join, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

import { ASTRO_BASE, GITHUB_REPO, DOCS_EXCLUDE } from '../site.config.mjs';

const WEB_DIR = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const REPO_ROOT = resolve(WEB_DIR, '..');
const DOCS_DIR = join(REPO_ROOT, 'docs');
const OUT_DIR = join(WEB_DIR, 'src/content/docs/docs');
const GITHUB_BLOB = `https://github.com/${GITHUB_REPO}/blob/main`;

const BASE = ASTRO_BASE ?? '';

/** Paths under docs/ that must never be published. Matched as path prefixes. */
const EXCLUDE = DOCS_EXCLUDE;

if (!existsSync(DOCS_DIR)) {
  console.error(`[sync-docs] no docs/ directory at ${DOCS_DIR}`);
  process.exit(1);
}

/** Every .md under docs/, as paths relative to docs/. */
async function collect(dir = DOCS_DIR) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = join(dir, entry.name);
    const rel = relative(DOCS_DIR, full);
    if (EXCLUDE.some((p) => rel === p || rel.startsWith(p + sep))) continue;
    if (entry.isDirectory()) out.push(...(await collect(full)));
    else if (extname(entry.name) === '.md') out.push(rel);
  }
  return out;
}

/** docs/adr/0001-native-backend.md -> adr/0001-native-backend */
const slugFor = (rel) =>
  rel
    .slice(0, -extname(rel).length)
    .split(sep)
    .map((s) => s.toLowerCase())
    .join('/')
    // A README inside a directory is that directory's index page.
    .replace(/\/readme$/, '');

const sources = await collect();
if (sources.length === 0) {
  console.error('[sync-docs] docs/ contained no publishable Markdown.');
  process.exit(1);
}

/** repo-relative path -> site slug, used to rewrite cross-document links. */
const slugBySource = new Map(sources.map((rel) => [rel, slugFor(rel)]));

// Two documents mapping to one slug would silently overwrite each other and
// leave the rewritten links pointing at whichever won. Fail instead.
const sourcesBySlug = new Map();
for (const [rel, slug] of slugBySource) {
  if (sourcesBySlug.has(slug)) {
    console.error(
      `[sync-docs] ${rel} and ${sourcesBySlug.get(slug)} both map to /docs/${slug}/.`,
    );
    process.exit(1);
  }
  sourcesBySlug.set(slug, rel);
}

const escapeYaml = (s) => s.replace(/"/g, '\\"');

/** First H1, else the filename turned into words. */
function titleOf(body, rel) {
  const h1 = body.match(/^#\s+(.+)$/m);
  if (h1) return h1[1].trim();
  const name = rel.split(sep).pop().replace(/\.md$/, '');
  return name.replace(/[-_]+/g, ' ').toLowerCase();
}

/** First non-heading, non-empty paragraph, trimmed to a meta description. */
function descriptionOf(body) {
  const lines = body.split('\n');
  for (const line of lines) {
    const t = line.trim();
    if (!t || t.startsWith('#') || t.startsWith('>') || t.startsWith('|')) continue;
    if (t.startsWith('```') || t.startsWith('<')) continue;
    const plain = t.replace(/[*_`\[\]]/g, '').replace(/\(([^)]*)\)/g, '');
    if (plain.length < 20) continue;
    return plain.length > 160 ? plain.slice(0, 157).trimEnd() + '…' : plain;
  }
  return undefined;
}

/**
 * Rewrite links. A link to another synced doc becomes a site URL; anything else
 * that points into the repo becomes an absolute GitHub URL rather than a 404.
 */
function rewriteLinks(body, rel) {
  return body.replace(/\]\(([^)]+)\)/g, (match, href) => {
    const raw = href.trim();
    if (/^(https?:|mailto:|#)/.test(raw)) return match;

    const [pathPart, hash = ''] = raw.split('#');
    const anchor = hash ? `#${hash}` : '';
    if (!pathPart) return match;

    // Resolve relative to the linking document's directory, inside docs/.
    const fromDir = dirname(rel);
    const target = pathPart.startsWith('/')
      ? relative(DOCS_DIR, join(REPO_ROOT, pathPart))
      : relative(DOCS_DIR, resolve(join(DOCS_DIR, fromDir), pathPart));

    const slug = slugBySource.get(target.split(sep).join(sep));
    if (slug !== undefined) return `](${BASE}/docs/${slug}/${anchor})`;

    // Not a synced doc — point at the repo so the link still resolves.
    const repoPath = pathPart.startsWith('/')
      ? pathPart.slice(1)
      : relative(REPO_ROOT, resolve(join(DOCS_DIR, fromDir), pathPart));
    return `](${GITHUB_BLOB}/${repoPath}${anchor})`;
  });
}

await rm(OUT_DIR, { recursive: true, force: true });

for (const rel of sources) {
  const body = await readFile(join(DOCS_DIR, rel), 'utf8');
  const title = titleOf(body, rel);
  const description = descriptionOf(body);

  const stripped = body.replace(/^#\s+.+$/m, '').replace(/^\s+/, '');
  const content = rewriteLinks(stripped, rel);

  const frontmatter = [
    '---',
    `title: "${escapeYaml(title)}"`,
    ...(description ? [`description: "${escapeYaml(description)}"`] : []),
    '---',
    '',
  ].join('\n');

  // Write at the slug, not the source path. Starlight derives a page's URL from
  // where the file lands, so writing anywhere else would put the page at a URL
  // the rewritten links above do not point to — `adr/README.md` is linked as
  // /docs/adr/ and has to be written as `adr.md` to actually be served there.
  const outPath = join(OUT_DIR, `${slugBySource.get(rel)}.md`);
  await mkdir(dirname(outPath), { recursive: true });
  await writeFile(outPath, frontmatter + content);
}

console.log(`[sync-docs] synced ${sources.length} pages from docs/`);
