import { expect, test } from 'vitest';
import { render } from 'vitest-browser-svelte';
import { page } from 'vitest/browser';
import SocialLinks from './SocialLinks.svelte';

test('renders a labelled link for each social profile', async () => {
	render(SocialLinks);

	await expect.element(page.getByLabelText('LinkedIn')).toBeInTheDocument();
	await expect.element(page.getByLabelText('GitHub')).toBeInTheDocument();
	await expect.element(page.getByLabelText('Google Scholar')).toBeInTheDocument();
});

test('points each link at the correct external profile and opens it safely', async () => {
	render(SocialLinks);

	const linkedin = page.getByLabelText('LinkedIn');
	await expect.element(linkedin).toHaveAttribute('href', 'https://www.linkedin.com/in/rliu14/');
	await expect.element(linkedin).toHaveAttribute('target', '_blank');
	await expect.element(linkedin).toHaveAttribute('rel', 'noopener noreferrer');
});
