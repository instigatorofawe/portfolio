<script lang="ts">
	import { HANDS } from '$lib/generated/hands';
	import { GRID_SIZE } from '$lib/pushfold/frequencies';

	// Renders the 13x13 push/fold grid shared by every solver UI (heads-up,
	// three-way, …). Markup is unstyled here on purpose: pushfold.css and
	// threeway.css both target the generic .row/.cell/.strategy-indicator
	// class names as plain (non-scoped) stylesheets namespaced under their
	// own root class, so this component just needs to render those class
	// names inside whichever ancestor supplies the styling.
	let {
		strategy,
		action
	}: {
		/** 169-entry per-hand frequency array, row-major over the 13x13 grid. */
		strategy: Float32Array;
		/** Which action the filled portion of each bar represents. */
		action: 'push' | 'call';
	} = $props();

	const indices = Array.from({ length: GRID_SIZE }, (_, i) => i);
</script>

<div>
	{#each indices as rowIndex (rowIndex)}
		<div class="row">
			{#each indices as colIndex (colIndex)}
				<div class="cell">
					<div
						class="strategy-indicator"
						style="--fill: {strategy[rowIndex * GRID_SIZE + colIndex]};"
					>
						{#if action === 'push'}
							<div class="bet"></div>
						{:else}
							<div class="call"></div>
						{/if}
						<div class="fold"></div>
					</div>
					<div class="cell-label">
						{HANDS[rowIndex][colIndex]}
					</div>
				</div>
			{/each}
		</div>
	{/each}
</div>
