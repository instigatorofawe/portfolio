<script lang="ts">
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';

	export type Essay = {
		title: string;
		date: string;
		id: string;
		md: string;
	};

	let { essays }: { essays: Essay[] } = $props();

	let query = $state('');

	function match(title: string, query: string): boolean {
		if (query.length == 0) {
			return true;
		}
		return title.toLowerCase().includes(query.toLowerCase());
	}
</script>

<table class="essaysTable">
	<thead>
		<tr>
			<td class="searchBox" colspan="2">
				<label
					>Filter by title: <input bind:value={query} style="margin-left: var(--space-2)" /></label
				>
			</td>
		</tr>
		<tr>
			<th class="essayTableCell">Title</th>
			<th class="essayTableCell" style="text-align: right;">Date</th>
		</tr>
	</thead>

	<tbody>
		{#each essays as essay (essay.id)}
			{#if match(essay.title, query)}
				<tr class="essayTableRow" onclick={() => goto(resolve('/essays/[id]', { id: essay.id }))}>
					<!-- eslint-disable-next-line svelte/no-at-html-tags -- essay titles are author-authored trusted content -->
					<td class="essayTableCell">{@html essay.title} </td>
					<td class="essayTableCell" style="text-align: right;">
						{new Date(essay.date).toLocaleDateString()}
					</td>
				</tr>
			{/if}
		{/each}
	</tbody>
</table>

<style>
	.essaysTable {
		border-collapse: collapse;
		text-align: left;
		max-width: var(--full-container-width);
		width: var(--full-container-width);
	}

	.essaysTable td,
	th {
		padding-top: var(--space-2);
		padding-bottom: var(--space-3);
		font-size: var(--text-base);
	}

	.essaysTable th {
		font-size: var(--text-lg);
	}

	.essayTableRow:hover {
		background-color: var(--hairline);
		cursor: pointer;
	}

	.essayTableCell:not(:last-child) {
		padding-right: var(--space-4);
	}

	.searchBox {
		text-align: right;
	}

	@media only screen and (max-width: 1024px) {
		.essaysTable {
			width: calc(100vw - 2rem);
		}
	}
</style>
