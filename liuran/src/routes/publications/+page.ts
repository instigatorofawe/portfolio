import type { PageLoad } from './$types';
import { parse } from 'yaml';
import publications from '$lib/assets/publications.yml?raw';
import type { Publication } from '$lib/components/Publications.svelte';

export const load: PageLoad = async () => {
	return {
		publications: parse(publications) as Publication[]
	};
};
