import { execSync } from 'node:child_process';
import { defineConfig } from 'vitest/config';
import { playwright } from '@vitest/browser-playwright';
import adapter from '@sveltejs/adapter-static';
import wasm from 'vite-plugin-wasm';
import { sveltekit } from '@sveltejs/kit/vite';

// Latest commit date (strict ISO 8601), baked in at build time. Falls back to
// the current time if git history isn't available (e.g. a shallow checkout).
let lastCommitDate: string;
try {
	lastCommitDate = execSync('git log -1 --format=%cI').toString().trim();
} catch {
	lastCommitDate = new Date().toISOString();
}

export default defineConfig({
	define: {
		__LAST_COMMIT_DATE__: JSON.stringify(lastCommitDate)
	},
	plugins: [
		sveltekit({
			compilerOptions: {
				// Force runes mode for the project, except for libraries. Can be removed in svelte 6.
				runes: ({ filename }) =>
					filename.split(/[/\\]/).includes('node_modules') ? undefined : true
			},

			// adapter-static prerenders the whole app to a static site (SSG).
			// See https://svelte.dev/docs/kit/adapter-static for configuration options.
			adapter: adapter()
		}),
		wasm()
	],
	test: {
		expect: { requireAssertions: true },
		projects: [
			{
				extends: './vite.config.ts',
				test: {
					name: 'client',
					browser: {
						enabled: true,
						provider: playwright(),
						instances: [{ browser: 'chromium', headless: true }]
					},
					include: ['src/**/*.svelte.{test,spec}.{js,ts}'],
					exclude: ['src/lib/server/**']
				}
			},

			{
				extends: './vite.config.ts',
				test: {
					name: 'server',
					environment: 'node',
					include: ['src/**/*.{test,spec}.{js,ts}'],
					exclude: ['src/**/*.svelte.{test,spec}.{js,ts}']
				}
			}
		]
	}
});
