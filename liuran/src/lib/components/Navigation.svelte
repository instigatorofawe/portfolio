<script lang="ts">
	import { page } from '$app/state';
	import { resolve } from '$app/paths';
	import { navItems } from '$lib/config/navigation';

	let { active = page.url.pathname } = $props();
</script>

<nav>
	<div>
		{#each navItems as item (item.href)}
			<a
				href={resolve(item.href)}
				class:active={active === item.href}
				aria-current={active === item.href ? 'page' : undefined}
			>
				{item.label}
			</a>
		{/each}
	</div>
</nav>

<style>
	nav {
		display: flex;
		width: 100%;
		height: var(--nav-height);
		font-family: var(--font-mono);
		border-bottom: 1px solid var(--hairline);
	}

	nav div {
		margin: auto auto;
		padding: 0 var(--space-3);
	}

	nav a {
		color: var(--muted);
		text-decoration: none;
		font-size: 1rem;
	}

	nav a + a::before {
		content: '|';
		display: inline-block;
		margin: 0 var(--space-3);
		color: var(--muted);
	}

	nav a.active {
		color: var(--ink);
	}

	nav a:hover {
		color: var(--link);
		text-decoration: underline;
	}
</style>
