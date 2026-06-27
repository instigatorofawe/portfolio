<script lang="ts">
	import Publications from '$lib/components/Publications.svelte';
	import type { PageData } from './$types';
	let { data }: { data: PageData } = $props();
</script>

<svelte:head>
	<title>Ran Liu - Publications</title>
</svelte:head>

<main>
	<h1>Selected publications</h1>
	<p>
		For a complete list, see
		<a href="https://scholar.google.com/citations?user=bshgBtkAAAAJ&hl=en">Google Scholar</a>.
	</p>
	<p style="text-align: right;">(*) denotes equal contribution</p>
	<Publications publications={data.publications} />
</main>
