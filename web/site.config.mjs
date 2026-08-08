/**
 * Where this build thinks it is being published.
 *
 * Two things have to agree about the base path: Astro, which rewrites the URLs
 * it emits, and scripts/nest-base.mjs, which moves the built files to match. If
 * they ever disagree the site still builds and every link 404s, so they read
 * the answer from here rather than each reaching for process.env themselves.
 *
 * This value also has to match the Worker routes in wrangler.jsonc and the row
 * for this project in fobiat.dev's src/data/projects.js.
 *
 * Production is fobiat.dev/openbpm. Override to host elsewhere:
 *   SITE_URL=https://example.com BASE_PATH= npm run build   # domain root
 */

export const SITE_URL = process.env.SITE_URL ?? 'https://fobiat.dev';

/** Leading slash, no trailing slash. Empty string means "served at the root". */
export const BASE_PATH = process.env.BASE_PATH ?? '/openbpm';

/** Astro wants `undefined` rather than `''` for a root-hosted site. */
export const ASTRO_BASE = BASE_PATH.replace(/\/+$/, '') || undefined;

/** Bare path segment — `openbpm` — as used for directory names. */
export const BASE_SEGMENT = BASE_PATH.replace(/^\/+|\/+$/g, '');

/** Used to turn links this site cannot resolve into absolute GitHub URLs. */
export const GITHUB_REPO = 'fobiat/openBPM';

/** Directory, relative to the repo root, that sync-docs.mjs publishes from. */
export const DOCS_DIR_NAME = 'docs';

/**
 * Directories under the docs directory copied verbatim into the built site,
 * and the URL each is served at, for non-Markdown files a document links to.
 * Nothing here needs one yet.
 */
export const ASSET_MOUNTS = [];

/**
 * Paths under docs/ that must never be published, as path prefixes relative to
 * docs/. sync-docs.mjs otherwise publishes everything it finds, so anything
 * unfinished or not for public reading has to be named here.
 */
export const DOCS_EXCLUDE = [];
