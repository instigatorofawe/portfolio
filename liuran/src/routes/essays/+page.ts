import type { PageLoad } from './$types';
import { parse } from 'yaml';
import essays from '$lib/assets/essays.yml?raw';
import { sanitizeHtml } from '$lib/sanitize';
import type { Essay } from '$lib/components/EssaysTable.svelte';

export const load: PageLoad = () => {
	// Titles carry inline HTML (e.g. <i>…</i>) that EssaysTable renders via
	// {@html}; sanitize them here so the component only ever sees clean markup.
	const directory: Essay[] = parse(essays);
	return {
		essays: directory.map((essay) => ({ ...essay, title: sanitizeHtml(essay.title) }))
	};
};
