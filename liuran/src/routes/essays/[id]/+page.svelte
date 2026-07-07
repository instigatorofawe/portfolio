<script lang="ts">
	import type { PageData } from './$types';
	let { data }: { data: PageData } = $props();

	let plainTitle = $derived(data.title.replace(/<[^>]*>/g, ''));
</script>

<svelte:head>
	<title>Ran Liu - {plainTitle}</title>
</svelte:head>

<!-- eslint-disable-next-line svelte/no-at-html-tags -- rendered markdown is sanitized with DOMPurify in the loader -->
<main>{@html data.content}</main>

<style>
	main :global(p) {
		text-indent: var(--space-8);
	}

	main :global(ol),
	:global(ul) {
		line-height: 1.75;
	}
</style>
