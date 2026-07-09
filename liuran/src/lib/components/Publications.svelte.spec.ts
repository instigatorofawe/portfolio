import { expect, test } from 'vitest';
import { render } from 'vitest-browser-svelte';
import { page } from 'vitest/browser';
import Publications, { type Publication } from './Publications.svelte';

const withDoi: Publication = {
	title: 'A Paper With A DOI',
	authors: 'Ran Liu, Someone Else',
	journal: 'Journal of Testing',
	year: 2023,
	doi: '10.1000/example',
	git: 'https://github.com/instigatorofawe/example'
};

const bare: Publication = {
	title: 'A Paper Without A DOI',
	authors: 'Ran Liu',
	journal: 'Preprint Server',
	year: 2021
};

test('renders one entry per publication with its metadata', async () => {
	render(Publications, { publications: [withDoi, bare] });

	await expect.element(page.getByText('A Paper With A DOI')).toBeInTheDocument();
	await expect.element(page.getByText('Ran Liu, Someone Else')).toBeInTheDocument();
	await expect.element(page.getByText('Journal of Testing')).toBeInTheDocument();
	await expect.element(page.getByText('2023')).toBeInTheDocument();
});

test('links the title to its DOI and opens it safely', async () => {
	render(Publications, { publications: [withDoi] });

	const title = page.getByRole('link', { name: 'A Paper With A DOI' });
	await expect.element(title).toHaveAttribute('href', 'https://doi.org/10.1000/example');
	await expect.element(title).toHaveAttribute('target', '_blank');
	await expect.element(title).toHaveAttribute('rel', 'noopener noreferrer');
});

test('renders the title as plain text when there is no DOI', async () => {
	render(Publications, { publications: [bare] });

	await expect.element(page.getByText('A Paper Without A DOI')).toBeInTheDocument();
	await expect
		.element(page.getByRole('link', { name: 'A Paper Without A DOI' }))
		.not.toBeInTheDocument();
});

test('shows a code link only when a git URL is present', async () => {
	render(Publications, { publications: [withDoi, bare] });

	const codeLink = page.getByRole('link', { name: 'code' });
	await expect.element(codeLink).toHaveAttribute('href', withDoi.git!);
	// Only the DOI paper has a git URL, so there is exactly one code link.
	expect(codeLink.elements()).toHaveLength(1);
});
