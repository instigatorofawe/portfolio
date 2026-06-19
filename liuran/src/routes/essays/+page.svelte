<script lang="ts">
	import EssaysTable from '$lib/components/EssaysTable.svelte';
	import type { PageData } from './$types';
	let { data }: { data: PageData } = $props();
</script>

<svelte:head>
	<title>Ran Liu - Essays</title>
</svelte:head>

<main>
	<h1>Essays</h1>
	<EssaysTable essays={data.essays} />
</main>

<style>
	main {
		font-family: var(--font-serif);
	}
</style>
