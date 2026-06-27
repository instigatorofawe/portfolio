<script lang="ts">
	import { HANDS } from '$lib/generated/hands';
	import { computeFrequencies, N_HANDS } from '$lib/pushfold/frequencies';
	import '$lib/styles/pushfold.css';

	type SolveFn = (typeof import('$lib/pkg/pushfold'))['solve_push_fold'];

	const N_ITER = 150;

	// The solver is backed by a WASM module that instantiates asynchronously.
	// Loading it through a dynamic import (resolved only once the module is ready)
	// keeps this component out of the static module graph that has to await the
	// WASM, which is what broke client-side navigation in dev, and avoids running
	// the solver during static prerendering.
	let solve = $state<SolveFn | null>(null);

	$effect(() => {
		import('$lib/pkg/pushfold').then((m) => (solve = m.solve_push_fold));
	});

	let stack = $state(5.0);
	let sb = $state(0.5);
	let ante = $state(0.125);
	let strategy = $derived(solve ? solve(stack, sb, ante, N_ITER) : null);
	let selected = $state('bu');

	let frequencies = $derived.by(() => {
		if (!strategy) return null;
		const { push, buFold, call, bbFold } = computeFrequencies(strategy);
		const formatter = new Intl.NumberFormat('en-US', { maximumSignificantDigits: 4 });
		return [
			formatter.format(push * 100),
			formatter.format(buFold * 100),
			formatter.format(call * 100),
			formatter.format(bbFold * 100)
		];
	});

	function reset() {
		stack = 5.0;
		sb = 0.5;
		ante = 0.125;
	}

	function select(id: string) {
		selected = id;
	}
</script>

<div class="pushfold">
	<div class="controls">
		<div class="configs-wrapper configs-inputs">
			<div class="configs input-container">
				Stack (BB): <input bind:value={stack} class="config-input" type="number" step="0.5" />
			</div>
			<div class="configs input-container">
				SB (BB): <input bind:value={sb} class="config-input" type="number" step="0.1" />
			</div>
			<div class="configs input-container">
				Ante (BB):<input bind:value={ante} class="config-input" type="number" step="0.005" />
			</div>
		</div>
		<div class="configs">
			<button onclick={() => reset()}>Default</button>
		</div>
	</div>

	{#if strategy}
		<div class="wrapper">
			<div class="selector-col">
				<div
					tabindex="0"
					class:bu-selected={selected == 'bu'}
					role="button"
					onkeypress={(e) => e.key === 'Enter' && select('bu')}
					onclick={() => select('bu')}
					class="selector"
				>
					<div class="selector-label">BU</div>
				</div>
				<div
					tabindex="0"
					class:bb-selected={selected == 'bb'}
					role="button"
					onkeypress={(e) => e.key === 'Enter' && select('bb')}
					onclick={() => select('bb')}
					class="selector"
				>
					<div class="selector-label">BB</div>
				</div>
			</div>

			<div>
				{#each [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12] as rowIndex (rowIndex)}
					<div class="row">
						{#each [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12] as colIndex (colIndex)}
							<div class="cell">
								<div
									class="strategy-indicator"
									style="--fill: {selected == 'bu'
										? strategy[rowIndex * 13 + colIndex]
										: strategy[N_HANDS + rowIndex * 13 + colIndex]};"
								>
									{#if selected == 'bu'}
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
		</div>
	{:else}
		<div class="loading">Loading solver…</div>
	{/if}

	{#if frequencies}
		<div class="configs-wrapper">
			<div class="configs">
				BU:
				<span class="push-summary">push {frequencies[0]}%</span>
				<span class="fold-summary">fold {frequencies[1]}%</span>
			</div>

			<div class="configs">
				BB:
				<span class="call-summary">call {frequencies[2]}%</span>
				<span class="fold-summary">fold {frequencies[3]}%</span>
			</div>
		</div>
	{/if}
</div>
