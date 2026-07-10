import type { PageLoad, EntryGenerator } from './$types';
import { error } from '@sveltejs/kit';
import essays from '$lib/assets/essays.yml?raw';
import { sanitizeHtml } from '$lib/sanitize';
import Essay from '$lib/components/EssaysTable.svelte';
import { marked } from 'marked';
import { parse } from 'yaml';

// Essay markdown is bundled from $lib/assets rather than fetched from /static;
// the glob keys are absolute module paths ending in the `md:` filename.
const essayFiles = import.meta.glob('/src/lib/assets/essays/*.md', {
	query: '?raw',
	import: 'default'
}) as Record<string, () => Promise<string>>;

export const load: PageLoad = async ({ params }) => {
	const directory: Essay[] = parse(essays);
	const essay = directory.find((entry) => entry.id === params.id);
	if (!essay) {
		error(404, 'Essay not found');
	}

	const loadRaw = essayFiles['/src/lib/assets/essays/' + essay.md];
	if (!loadRaw) {
		// A bad `md:` path in essays.yml would otherwise slip through; surface it
		// as an error so the prerender fails at build time rather than shipping a
		// broken page.
		error(404, 'Essay content unavailable');
	}

	return {
		title: sanitizeHtml(essay.title),
		content: sanitizeHtml(await marked.parse(await loadRaw()))
	};
};

export const entries: EntryGenerator = () => {
	const directory: Essay[] = parse(essays);
	return directory.map((entry) => ({ id: entry.id }));
};
