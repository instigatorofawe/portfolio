<script lang="ts">
	import '$lib/styles/tokens.css';
	import '$lib/styles/base.css';
	import favicon from '$lib/assets/favicon.png';
	import Navigation from '$lib/components/Navigation.svelte';
	import Footer from '$lib/components/Footer.svelte';

	let { children } = $props();
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
</svelte:head>

<div class="page-container">
	<Navigation />
	<div class="content">
		{@render children()}
	</div>
	<Footer />
</div>

<style>
	:global(body) {
		display: flex;
		flex-direction: column;
		margin: 0;
		background-color: var(--bg);
	}

	:global(*),
	:global(*::before),
	:global(*::after) {
		box-sizing: border-box;
	}

	.page-container {
		display: flex;
		flex-direction: column;
		min-height: calc(100vh);
	}

	.content {
		margin: auto;
		padding: 1rem;
	}
</style>
