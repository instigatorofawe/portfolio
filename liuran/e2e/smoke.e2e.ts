import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { expect, test } from '@playwright/test';
import { parse } from 'yaml';
import { navItems } from '../src/lib/config/navigation';

// End-to-end smoke tests: load the production build through `preview` and assert
// that each route renders and the WASM-backed solver runs. These are intentionally
// shallow — they catch a broken build, dead route, or non-loading WASM module,
// not detailed UI behavior (that lives in the component unit tests).

// `navItems` is the single source of truth for which pages live in the nav, so the
// route list is derived from it; this map just adds the expected title/heading per
// path. A new nav entry without a matching entry here fails fast (undefined title).
const pageMeta: Record<string, { title: string; heading: string }> = {
	'/': { title: 'Ran Liu', heading: 'About' },
	'/publications/': { title: 'Ran Liu - Publications', heading: 'Selected publications' },
	'/software/': { title: 'Ran Liu - Software', heading: 'Open-source contributions' },
	'/essays/': { title: 'Ran Liu - Essays', heading: 'Essays' }
};

const routes = navItems.map((item) => ({ path: item.href, ...pageMeta[item.href] }));

// essays.yml is the single source of truth for the essays directory (same file the
// app's loaders read), so the test stays in sync as essays are added or removed.
type Essay = { title: string; date: string; id: string; md: string };
const essays: Essay[] = parse(
	readFileSync(fileURLToPath(new URL('../src/lib/assets/essays.yml', import.meta.url)), 'utf8')
);

// Titles in essays.yml contain inline HTML (e.g. <i>…</i>); the rendered page text
// and document <title> use the tag-stripped form, matching the app's own logic.
const plainTitle = (title: string) => title.replace(/<[^>]*>/g, '');

test('home page renders', async ({ page }) => {
	await page.goto('/');
	await expect(page).toHaveTitle('Ran Liu');
	await expect(page.getByRole('heading', { name: 'About' })).toBeVisible();
});

test('primary navigation reaches every page', async ({ page }) => {
	await page.goto('/');
	const nav = page.getByRole('navigation');

	// Match nav links by href: the decorative "|" separator (a ::before pseudo)
	// gets folded into each link's accessible name, so name-based matching is brittle.
	for (const route of routes) {
		await nav.locator(`a[href="${route.path}"]`).click();
		await expect(page).toHaveURL(route.path);
		await expect(page).toHaveTitle(route.title);
		await expect(page.getByRole('heading', { name: route.heading })).toBeVisible();
	}
});

test('each route loads on direct navigation', async ({ page }) => {
	for (const route of routes) {
		await page.goto(route.path);
		await expect(page).toHaveTitle(route.title);
		await expect(page.getByRole('heading', { name: route.heading })).toBeVisible();
	}
});

test('every essay is listed on the essays page', async ({ page }) => {
	await page.goto('/essays/');

	const rows = page.locator('tr.essayTableRow');
	await expect(rows).toHaveCount(essays.length);
	for (const essay of essays) {
		await expect(rows.filter({ hasText: plainTitle(essay.title) })).toBeVisible();
	}
});

test('every essay is navigable from the list', async ({ page }) => {
	for (const essay of essays) {
		await page.goto('/essays/');
		// Each row's title cell holds the navigating <a href>; click the link rather
		// than the row, whose center may fall outside the link for shorter titles.
		await page
			.locator('tr.essayTableRow')
			.filter({ hasText: plainTitle(essay.title) })
			.getByRole('link')
			.click();

		await expect(page).toHaveURL(`/essays/${essay.id}/`);
		await expect(page).toHaveTitle(`Ran Liu - ${plainTitle(essay.title)}`);
		// The rendered markdown should produce some content, not an empty page.
		await expect(page.locator('main')).not.toBeEmpty();
	}
});

test('push/fold solver loads the WASM module and produces a solution', async ({ page }) => {
	await page.goto('/software/pushfold/');
	await expect(page).toHaveTitle('Ran Liu - Push/Fold Solver');

	// The solver instantiates asynchronously; once it resolves, the loading
	// placeholder is replaced by the strategy grid and frequency summaries.
	await expect(page.getByText('Loading solver…')).toBeHidden();
	await expect(page.getByText(/push \d/)).toBeVisible();
	await expect(page.getByText(/call \d/)).toBeVisible();
});
