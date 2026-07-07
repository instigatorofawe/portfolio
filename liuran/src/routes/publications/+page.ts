import type { PageLoad } from './$types';
import { parse } from 'yaml';
import publications from '$lib/assets/publications.yml?raw';
import { sanitizeHtml } from '$lib/sanitize';
import type { Publication } from '$lib/components/Publications.svelte';

export const load: PageLoad = async () => {
	// Author lists carry inline HTML (e.g. <b>…</b>) that Publications renders
	// via {@html}; sanitize them here so the component only ever sees clean markup.
	const items = parse(publications) as Publication[];
	return {
		publications: items.map((item) => ({ ...item, authors: sanitizeHtml(item.authors) }))
	};
};
