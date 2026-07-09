import { expect, test } from 'vitest';
import { render } from 'vitest-browser-svelte';
import { page } from 'vitest/browser';
import EssaysTable, { type Essay } from './EssaysTable.svelte';

const essays: Essay[] = [
	{ title: 'On Poker', date: '2024-01-15', id: 'on-poker', md: 'on-poker.md' },
	{ title: 'On Chess', date: '2023-06-30', id: 'on-chess', md: 'on-chess.md' },
	{ title: 'Poker Redux', date: '2022-11-01', id: 'poker-redux', md: 'poker-redux.md' }
];

// Match the component's own formatting so the assertion is locale-independent.
const formatDate = (date: string) =>
	new Date(date).toLocaleDateString(undefined, { timeZone: 'UTC' });

test('renders a linked row per essay with its formatted date', async () => {
	render(EssaysTable, { essays });

	const link = page.getByRole('link', { name: 'On Poker' });
	await expect.element(link).toHaveAttribute('href', '/essays/on-poker');
	await expect.element(page.getByText(formatDate('2024-01-15'))).toBeInTheDocument();
});

test('filters rows by a case-insensitive title substring', async () => {
	render(EssaysTable, { essays });

	await page.getByRole('textbox').fill('poker');

	// Both "On Poker" and "Poker Redux" match; "On Chess" is filtered out.
	await expect.element(page.getByRole('link', { name: 'On Poker' })).toBeInTheDocument();
	await expect.element(page.getByRole('link', { name: 'Poker Redux' })).toBeInTheDocument();
	await expect.element(page.getByRole('link', { name: 'On Chess' })).not.toBeInTheDocument();
});

test('shows every essay again when the filter is cleared', async () => {
	render(EssaysTable, { essays });

	const filter = page.getByRole('textbox');
	await filter.fill('chess');
	await expect.element(page.getByRole('link', { name: 'On Poker' })).not.toBeInTheDocument();

	await filter.clear();
	await expect.element(page.getByRole('link', { name: 'On Poker' })).toBeInTheDocument();
	await expect.element(page.getByRole('link', { name: 'On Chess' })).toBeInTheDocument();
	await expect.element(page.getByRole('link', { name: 'Poker Redux' })).toBeInTheDocument();
});
