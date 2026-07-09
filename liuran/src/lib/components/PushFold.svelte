<script lang="ts">
	import { HANDS } from '$lib/generated/hands';
	import { computeFrequencies } from '$lib/pushfold/frequencies';
	import '$lib/styles/pushfold.css';

	type SolverModule = typeof import('$lib/pkg/headsup/pushfold_headsup');
	type Solver = InstanceType<SolverModule['HeadsUpSolver']>;

	const N_ITER = 1000;

	// The solver is backed by a WASM module that instantiates asynchronously.
	// Loading it through a dynamic import (resolved only once the module is ready)
	// keeps this component out of the static module graph that has to await the
	// WASM, which is what broke client-side navigation in dev, and avoids running
	// the solver during static prerendering. The solver builds its lookup tables
	// once on construction, so we keep a single instance and reuse it per solve.
	let solver = $state<Solver | null>(null);

	$effect(() => {
		let instance: Solver | null = null;
		import('$lib/pkg/headsup/pushfold_headsup').then((m) => {
			instance = new m.HeadsUpSolver();
			solver = instance;
		});
		return () => {
			instance?.free();
			solver = null;
		};
	});

	let stack = $state(5.0);
	let sb = $state(0.5);
	let ante = $state(0.125);

	// Light client-side mirror of the solver's own validation (see validate() in
	// solver.rs). Rust remains authoritative, but checking here gives immediate
	// feedback and keeps us from firing the solver with junk. Empty number inputs
	// bind to null in Svelte; otherwise-invalid ones to NaN.
	let validationError = $derived.by(() => {
		if ([stack, sb, ante].some((v) => v == null || !Number.isFinite(v)))
			return 'Enter numeric values for stack, SB, and ante.';
		if (stack <= 0) return 'Stack must be positive.';
		if (sb < 0 || ante < 0) return 'SB and ante must be non-negative.';
		if (sb > stack) return 'SB cannot exceed the stack.';
		return null;
	});

	type Solution = {
		/** Button push frequency per infoset (169 entries, row-major grid). */
		buPush: Float32Array;
		/** Big blind call frequency per infoset (169 entries, row-major grid). */
		bbCall: Float32Array;
		/** Nash gap of the returned pair, in BB per deal. */
		exploitability: number;
	};

	let solution = $derived.by<Solution | null>(() => {
		if (!solver || validationError) return null;
		try {
			// The returned Strategies is a WASM object whose getters copy on each
			// access; read each field once into a plain JS value, then free the
			// object so its WASM memory isn't held until GC.
			const result = solver.solve(stack, sb, ante, N_ITER);
			const solution = {
				buPush: result.bu_push,
				bbCall: result.bb_call,
				exploitability: result.exploitability
			};
			result.free();
			return solution;
		} catch (e) {
			// The Rust layer rejected something the UI check missed; degrade
			// gracefully rather than crash the component.
			console.error('solver rejected inputs', e);
			return null;
		}
	});

	let selected = $state('bu');

	let frequencies = $derived.by(() => {
		if (!solution) return null;
		const { push, buFold, call, bbFold } = computeFrequencies(solution.buPush, solution.bbCall);
		const formatter = new Intl.NumberFormat('en-US', { maximumSignificantDigits: 4 });
		// Percentages below 0.01% are visual noise; clamp them to a flat 0.000%.
		const formatPct = (value: number) => (value < 0.01 ? '0.000' : formatter.format(value));
		return [
			formatPct(push * 100),
			formatPct(buFold * 100),
			formatPct(call * 100),
			formatPct(bbFold * 100)
		];
	});

	// Exploitability is the Nash gap in BB per deal; scale to the standard
	// BB/100 (big blinds won per 100 hands) win-rate unit poker players read.
	let exploitability = $derived.by(() => {
		if (!solution) return null;
		const formatter = new Intl.NumberFormat('en-US', { maximumSignificantDigits: 3 });
		const value = solution.exploitability * 100;
		// Below 0.000001 bb/100 the number is effectively zero; show a flat value.
		return value < 0.000001 ? '0.000000' : formatter.format(value);
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
				Stack (BB):
				<input bind:value={stack} class="config-input" type="number" step="0.5" min="0.5" />
			</div>
			<div class="configs input-container">
				SB (BB): <input bind:value={sb} class="config-input" type="number" step="0.1" min="0" />
			</div>
			<div class="configs input-container">
				Ante (BB):<input
					bind:value={ante}
					class="config-input"
					type="number"
					step="0.005"
					min="0"
				/>
			</div>
		</div>
		<div class="configs">
			<button onclick={() => reset()}>Default</button>
		</div>
	</div>

	{#if validationError}
		<div class="loading" role="alert">{validationError}</div>
	{:else if solution}
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
										? solution.buPush[rowIndex * 13 + colIndex]
										: solution.bbCall[rowIndex * 13 + colIndex]};"
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

			{#if exploitability}
				<div class="configs">
					Exp:
					<span class="exploitability-summary">{exploitability} bb/100</span>
				</div>
			{/if}
		</div>
	{/if}
</div>
