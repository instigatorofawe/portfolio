import { defineConfig } from '@playwright/test';

const PORT = 4173;

export default defineConfig({
	testDir: 'e2e',
	testMatch: '**/*.e2e.{ts,js}',
	// Fail the run if a `test.only` is accidentally committed.
	forbidOnly: !!process.env.CI,
	use: { baseURL: `http://localhost:${PORT}` },
	webServer: {
		command: 'npm run build && npm run preview',
		port: PORT,
		// Locally, reuse an already-running `preview` server instead of rebuilding.
		reuseExistingServer: !process.env.CI
	}
});
