// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import sitemap from '@astrojs/sitemap';

// Where the site is published is shared with the post-build nesting step, so
// it lives in one module both import rather than two copies of the same
// `process.env` default. See site.config.mjs.
import { SITE_URL, ASTRO_BASE } from './site.config.mjs';

export default defineConfig({
  site: SITE_URL,
  base: ASTRO_BASE,
  trailingSlash: 'ignore',
  integrations: [
    sitemap(),
    starlight({
      title: 'openBPM',
      description:
        'A tap-tempo BPM counter and beatmatch assistant for vinyl DJing on an ESP32.',
      social: [
        {
          icon: 'github',
          label: 'GitHub',
          href: 'https://github.com/fobiat/openBPM',
        },
      ],
      // Generated from docs/ by scripts/sync-docs.mjs, so the sidebar follows
      // the repo's own directory structure rather than a second list here that
      // would drift out of sync with it.
      sidebar: [
        { label: 'Documentation', items: [{ autogenerate: { directory: 'docs' } }] },
      ],
      pagination: false,
    }),
  ],
});
