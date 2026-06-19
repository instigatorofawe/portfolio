<script lang="ts">
	import GoogleScholar from './icons/GoogleScholar.svelte';
	import LinkedIn from './icons/LinkedIn.svelte';
	import GitHub from './icons/GitHub.svelte';

	type Social = {
		label: string;
		href: string;
		icon: typeof GoogleScholar;
		/** Brand color shown on hover for single-color icons. */
		brand?: string;
		/** Multi-color icons are desaturated until hovered. */
		multicolor?: boolean;
	};

	const socials: Social[] = [
		{
			label: 'LinkedIn',
			href: 'https://www.linkedin.com/in/rliu14/',
			icon: LinkedIn,
			brand: '#0a66c2'
		},
		{ label: 'GitHub', href: 'https://github.com/instigatorofawe', icon: GitHub, brand: '#181717' },
		{
			label: 'Google Scholar',
			href: 'https://scholar.google.com/citations?user=bshgBtkAAAAJ&hl=en',
			icon: GoogleScholar,
			multicolor: true
		}
	];
</script>

<div class="socials">
	{#each socials as { label, href, icon: Icon, brand, multicolor } (href)}
		<!-- eslint-disable svelte/no-navigation-without-resolve -- external social profile URL -->
		<a
			{href}
			target="_blank"
			rel="noopener noreferrer"
			aria-label={label}
			class:multicolor
			style={brand ? `--brand: ${brand}` : undefined}
		>
			<Icon />
		</a>
		<!-- eslint-enable svelte/no-navigation-without-resolve -->
	{/each}
</div>

<style>
	.socials {
		display: flex;
		flex-direction: row;
		gap: var(--space-3);
		margin: var(--space-2) 0;
	}

	.socials a {
		color: var(--muted);
		display: inline-flex;
	}

	.socials a:hover {
		color: var(--brand);
	}

	.socials :global(svg) {
		width: 22px;
		height: 22px;
		transition:
			color 0.15s ease,
			filter 0.15s ease;
	}

	/* Multi-color icons are gray by default and reveal their brand colors on hover. */
	.socials a.multicolor :global(svg) {
		filter: grayscale(1);
	}

	.socials a.multicolor:hover :global(svg) {
		filter: none;
	}
</style>
