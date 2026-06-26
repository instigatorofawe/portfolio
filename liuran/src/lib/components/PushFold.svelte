<script lang="ts">
	import { HANDS } from '$lib/generated/hands';

	type SolveFn = (typeof import('$lib/pkg/pushfold'))['solve_push_fold'];

	const N_ITER = 150;
	const TOTAL_COMBOS = 1326;
	const N_HANDS = 169;
	const BREAKPOINT_MOBILE = 1024;

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
		let strategy_bu = 0;
		let strategy_bb = 0;
		for (let index = 0; index < 169; index++) {
			let i = Math.floor(index / 13);
			let j = index % 13;

			let combos;
			if (i == j) {
				combos = 6;
			} else if (i < j) {
				combos = 4;
			} else {
				combos = 12;
			}

			strategy_bu += (strategy[index] * combos) / TOTAL_COMBOS;
			strategy_bb += (strategy[index + N_HANDS] * combos) / TOTAL_COMBOS;
		}
		let formatter = new Intl.NumberFormat('en-US', { maximumSignificantDigits: 4 });
		let push = formatter.format(strategy_bu * 100);
		let bu_fold = formatter.format((1 - strategy_bu) * 100);
		let call = formatter.format(strategy_bb * 100);
		let bb_fold = formatter.format((1 - strategy_bb) * 100);

		return [push, bu_fold, call, bb_fold];
	});

	let innerWidth = $state(0);
	let cellHeight = $derived(innerWidth <= BREAKPOINT_MOBILE ? 20 : 50);

	function reset() {
		stack = 5.0;
		sb = 0.5;
		ante = 0.125;
	}

	function select(id: string) {
		selected = id;
	}
</script>

<div style="display: flex;">
	<div class="configs-wrapper" style="margin: 15px auto;">
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
		<div style="width: 50px; border: 1px solid; margin-right: -1px; margin-bottom: -1px;">
			<div
				tabindex="0"
				class:bu-selected={selected == 'bu'}
				role="button"
				onkeypress={(e) => e.key === 'Enter' && select('bu')}
				onclick={() => select('bu')}
				class="selector"
			>
				<div style="margin: auto;">BU</div>
			</div>
			<div
				tabindex="0"
				class:bb-selected={selected == 'bb'}
				role="button"
				onkeypress={(e) => e.key === 'Enter' && select('bb')}
				onclick={() => select('bb')}
				class="selector"
			>
				<div style="margin: auto;">BB</div>
			</div>
		</div>

		<div>
			{#each [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12] as rowIndex (rowIndex)}
				<div class="row">
					{#each [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12] as colIndex (colIndex)}
						<div class="cell">
							<div class="strategy-indicator">
								{#if selected == 'bu'}
									<div
										class="bet"
										style="height: calc({cellHeight}px * {strategy[rowIndex * 13 + colIndex]});"
									></div>
									<div
										class="fold"
										style="height: calc({cellHeight}px * {1 - strategy[rowIndex * 13 + colIndex]})"
									></div>
								{:else}
									<div
										class="call"
										style="height: calc({cellHeight}px * {strategy[
											N_HANDS + rowIndex * 13 + colIndex
										]});"
									></div>
									<div
										class="fold"
										style="height: calc({cellHeight}px * {1 -
											strategy[N_HANDS + rowIndex * 13 + colIndex]})"
									></div>
								{/if}
							</div>
							<div style="margin: auto;">
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

<svelte:window bind:innerWidth />

<style>
	:root {
		font-family: 'Lato';
	}

	.wrapper {
		display: flex;
		flex-direction: row;
	}

	.loading {
		display: flex;
		place-items: center;
		justify-content: center;
		min-height: 324.5px;
		color: var(--muted);
		font-size: 12pt;
	}

	.row {
		display: flex;
	}

	.cell {
		width: 51px;
		height: 51px;
		border: 1px solid;
		margin-right: -1px;
		margin-bottom: -1px;
		display: flex;
		font-size: 16px;
		position: relative;
	}

	.cell:hover {
		cursor: pointer;
		background-color: rgba(0, 0, 0, 0.2);
	}

	.strategy-indicator {
		position: absolute;
	}

	.bet {
		position: relative;
		width: 49px;
		z-index: -1;
	}

	.call {
		position: relative;
		width: 49px;
		z-index: -1;
	}

	.fold {
		position: relative;
		width: 49px;
		z-index: -1;
	}

	button {
		border-radius: 8px;
		border: 1px solid transparent;
		padding: 0.6em 1.2em;
		margin: 5px;
		font-size: 12pt;
		font-weight: 500;
		font-family: inherit;
		cursor: pointer;
		transition: border-color 0.2s;
	}

	button:hover {
		background-color: #dfdfdf;
	}

	.selector {
		height: 324.5px;
		display: flex;
		background-color: #efefef;
	}

	.selector:hover {
		cursor: pointer;
		background-color: #dfdfdf;
	}

	.bu-selected,
	.bu-selected:hover,
	.bet,
	.push-summary {
		background-color: rgb(233, 150, 122);
	}

	.bb-selected,
	.bb-selected:hover,
	.call,
	.call-summary {
		background-color: rgb(143, 188, 139);
	}

	.fold-summary,
	.fold {
		background-color: rgb(109, 162, 192);
	}

	.push-summary,
	.call-summary,
	.fold-summary {
		padding: 5px 10px 5px 10px;
		border-radius: 5px;
		margin: 5px;
	}

	.configs {
		display: flex;
		place-items: center;
		font-size: 12pt;
		margin: auto;
	}

	.config-input {
		margin: 5px;
		width: 55px;
	}

	.configs-wrapper {
		display: flex;
		flex-direction: row;
		margin: 15px 0;
		place-items: center;
	}

	@media only screen and (max-width: 1024px) {
		button {
			font-size: 9pt;
		}
		.cell {
			width: 21px;
			height: 21px;
			font-size: 8px;
		}

		.bet,
		.fold,
		.call {
			width: 19px;
		}

		.selector {
			height: 129.5px;
		}

		.configs {
			font-size: 12px;
		}

		.configs-wrapper {
			flex-direction: column;
		}

		.input-container {
			margin: 0;
			align-self: end;
		}
	}
</style>
