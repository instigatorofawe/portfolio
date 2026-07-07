import type { PageLoad, EntryGenerator } from './$types';
import { error } from '@sveltejs/kit';
import essays from '$lib/assets/essays.yml?raw';
import { sanitizeHtml } from '$lib/sanitize';
import type { Essay } from '$lib/components/EssaysTable.svelte';
import { marked } from 'marked';
import { parse } from 'yaml';

export const load: PageLoad = async ({ fetch, params }) => {
	const directory: Essay[] = parse(essays);
	const essay = directory.find((entry) => entry.id === params.id);
	if (!essay) {
		error(404, 'Essay not found');
	}

	const response = await fetch('/essays/' + essay.md + '?raw');
	if (!response.ok) {
		// A bad `md:` path in essays.yml would otherwise render the fetch's
		// error-page HTML as essay content; surface it as an error instead
		// (failing the prerender at build time rather than shipping the page).
		error(response.status, 'Essay content unavailable');
	}

	return {
		title: sanitizeHtml(essay.title),
		content: sanitizeHtml(await marked.parse(await response.text()))
	};
};

export const entries: EntryGenerator = () => {
	const directory: Essay[] = parse(essays);
	return directory.map((entry) => ({ id: entry.id }));
};
