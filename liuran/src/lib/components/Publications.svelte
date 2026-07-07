<script lang="ts">
	export type Publication = {
		title: string;
		authors: string;
		journal: string;
		year: number;
		doi?: string;
		git?: string;
	};

	let { publications }: { publications: Publication[] } = $props();
</script>

<ul class="publications">
	{#each publications as { title, authors, journal, year, doi, git } (title)}
		<li>
			<p class="title">
				{#if doi}
					<a href="https://doi.org/{doi}" target="_blank" rel="noopener noreferrer">{title}</a>
				{:else}
					{title}
				{/if}
			</p>
			<!-- eslint-disable-next-line svelte/no-at-html-tags -- authors are sanitized with DOMPurify in the publications loader -->
			<p class="authors">{@html authors}</p>
			<p class="meta">
				<span class="journal">{journal}</span>, {year}
				{#if git}
					&middot;
					<!-- eslint-disable-next-line svelte/no-navigation-without-resolve -- external repository URL -->
					<a href={git} target="_blank" rel="noopener noreferrer">code</a>
				{/if}
			</p>
		</li>
	{/each}
</ul>

<style>
	.publications {
		list-style: none;
		margin: 0;
		padding: 0;
		display: flex;
		flex-direction: column;
		gap: var(--space-6);
	}

	.title {
		font-family: var(--font-serif);
		font-weight: 600;
		text-align: left;
		margin: 0 0 var(--space-1);
	}

	.authors {
		text-align: left;
		margin: 0 0 var(--space-1);
	}

	:global .authors b {
		color: var(--accent);
	}

	.meta {
		color: var(--muted);
		font-size: var(--text-sm);
		text-align: left;
		margin: 0;
	}

	.journal {
		font-style: italic;
	}
</style>
