#!/usr/bin/env node
/**
 * Relocate the built site into its BASE_PATH subdirectory.
 *
 * Astro's `base` option rewrites the URLs it *emits* (every href becomes
 * `/rivet/...`) but it does not nest what it *writes* — `dist/` still holds
 * `index.html` and `docs/` at its top level. That split is fine for hosts that
 * strip the prefix before looking at disk, and wrong for ours: the site is an
 * assets-only Cloudflare Worker, so Cloudflare resolves the request path
 * directly against `dist/`. A request for `/rivet/docs/` would look for
 * `dist/rivet/docs/index.html` and 404 against a `dist/docs/` that is right
 * there.
 *
 * So after the build, move the whole tree down one level into `dist/<base>/`.
 * Emitted links and files then agree, and no Worker script has to run to
 * rewrite paths at request time.
 *
 * No-op when BASE_PATH is unset, which is how a root-hosted build stays
 * untouched.
 */

import { mkdir, rename, rm } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { BASE_SEGMENT as base } from '../site.config.mjs';

const WEB_DIR = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const DIST = join(WEB_DIR, 'dist');

if (!base) {
  console.log('[nest-base] BASE_PATH unset — leaving dist/ at the root.');
  process.exit(0);
}

if (!existsSync(DIST)) {
  console.error('[nest-base] no dist/ to nest — run the build first.');
  process.exit(1);
}

// Already nested, which happens when the script runs twice over one build.
// Bail rather than burying the site a second level down.
if (existsSync(join(DIST, base)) && !existsSync(join(DIST, 'index.html'))) {
  console.log(`[nest-base] dist/${base}/ already in place — nothing to do.`);
  process.exit(0);
}

// dist/ cannot be moved inside itself, so stage it beside dist/ first, then
// recreate dist/ and drop the staged tree in at dist/<base>/.
const STAGING = join(WEB_DIR, '.dist-nest-staging');
await rm(STAGING, { recursive: true, force: true });
await rename(DIST, STAGING);

const destination = join(DIST, base);
await mkdir(dirname(destination), { recursive: true });
await rename(STAGING, destination);

console.log(`[nest-base] site moved to dist/${base}/`);
