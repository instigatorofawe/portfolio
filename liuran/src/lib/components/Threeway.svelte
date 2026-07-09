<script lang="ts">
	import { HANDS } from '$lib/generated/hands';
	import { computeFrequency } from '$lib/pushfold/frequencies';
	import '$lib/styles/threeway.css';

	type SolverModule = typeof import('$lib/pkg/threeway/pushfold_threeway');
	type Solver = InstanceType<SolverModule['ThreewaySolver']>;

	const N_ITER = 1000;

	// Same loading pattern as PushFold.svelte: the solver is backed by a WASM
	// module that instantiates asynchronously, so it's loaded through a
	// dynamic import to keep this component out of the static module graph
	// and out of prerendering. The solver builds its lookup tables once on
	// construction, so we keep a single instance and reuse it per solve.
	let solver = $state<Solver | null>(null);

	$effect(() => {
		let instance: Solver | null = null;
		import('$lib/pkg/threeway/pushfold_threeway').then((m) => {
			instance = new m.ThreewaySolver();
			solver = instance;
		});
		return () => {
			instance?.free();
			solver = null;
		};
	});

	let stackBu = $state(5.0);
	let stackSb = $state(5.0);
	let stackBb = $state(5.0);
	let sb = $state(0.5);
	let ante = $state(0.125);

	// Light client-side mirror of the solver's own validation (see
	// validate_inputs() in solver.rs). Rust remains authoritative, but
	// checking here gives immediate feedback and keeps us from firing the
	// solver with junk. Empty number inputs bind to null in Svelte; otherwise
	// -invalid ones to NaN.
	let validationError = $derived.by(() => {
		if ([stackBu, stackSb, stackBb, sb, ante].some((v) => v == null || !Number.isFinite(v)))
			return 'Enter numeric values for all stacks, the small blind, and the ante.';
		if (stackBu <= 0 || stackSb <= 0 || stackBb <= 0) return 'Stacks must be positive.';
		if (sb < 0 || ante < 0) return 'Small blind and ante must be non-negative.';
		if (sb > stackSb) return 'Small blind cannot exceed the SB stack.';
		if (stackBb < 1.0) return 'BB stack must be at least 1bb.';
		return null;
	});

	type Solution = {
		buPush: Float32Array;
		sbCall: Float32Array;
		sbPush: Float32Array;
		bbCallBoth: Float32Array;
		bbCallBu: Float32Array;
		bbCallSb: Float32Array;
		exploitability: number;
	};

	let solution = $derived.by<Solution | null>(() => {
		if (!solver || validationError) return null;
		try {
			// The returned Strategies is a WASM object whose getters copy on each
			// access; read each field once into a plain JS value, then free the
			// object so its WASM memory isn't held until GC.
			const result = solver.solve(stackBu, stackSb, stackBb, sb, ante, N_ITER);
			const solution = {
				buPush: result.bu_push,
				sbCall: result.sb_call,
				sbPush: result.sb_push,
				bbCallBoth: result.bb_call_both,
				bbCallBu: result.bb_call_bu,
				bbCallSb: result.bb_call_sb,
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

	type DecisionKey = 'bu' | 'sbCall' | 'sbPush' | 'bbBoth' | 'bbBu' | 'bbSb';

	const DECISIONS: Record<DecisionKey, { label: string; action: 'push' | 'call' }> = {
		bu: { label: 'BU', action: 'push' },
		sbCall: { label: 'SB call', action: 'call' },
		sbPush: { label: 'SB push', action: 'push' },
		bbBoth: { label: 'BB call both', action: 'call' },
		bbBu: { label: 'BB call BU', action: 'call' },
		bbSb: { label: 'BB call SB', action: 'call' }
	};

	let selected = $state<DecisionKey>('bu');

	let selectedStrategy = $derived.by(() => {
		if (!solution) return null;
		switch (selected) {
			case 'bu':
				return solution.buPush;
			case 'sbCall':
				return solution.sbCall;
			case 'sbPush':
				return solution.sbPush;
			case 'bbBoth':
				return solution.bbCallBoth;
			case 'bbBu':
				return solution.bbCallBu;
			case 'bbSb':
				return solution.bbCallSb;
		}
	});

	let frequency = $derived.by(() => {
		if (!selectedStrategy) return null;
		const formatter = new Intl.NumberFormat('en-US', { maximumSignificantDigits: 4 });
		// Percentages below 0.01% are visual noise; clamp them to a flat 0.000%.
		const formatPct = (value: number) => (value < 0.01 ? '0.000' : formatter.format(value));
		const act = computeFrequency(selectedStrategy) * 100;
		return { act: formatPct(act), fold: formatPct(100 - act) };
	});

	// Exploitability is the NashConv gap in BB per deal; scale to the standard
	// BB/100 (big blinds won per 100 hands) win-rate unit poker players read.
	let exploitability = $derived.by(() => {
		if (!solution) return null;
		const formatter = new Intl.NumberFormat('en-US', { maximumSignificantDigits: 3 });
		const value = solution.exploitability * 100;
		// Below 0.000001 bb/100 the number is effectively zero; show a flat value.
		return value < 0.000001 ? '0.000000' : formatter.format(value);
	});

	function reset() {
		stackBu = 5.0;
		stackSb = 5.0;
		stackBb = 5.0;
		sb = 0.5;
		ante = 0.125;
	}

	function select(key: DecisionKey) {
		selected = key;
	}
</script>

{#snippet node(key: DecisionKey)}
	<button
		type="button"
		class="node node-{DECISIONS[key].action}"
		class:node-selected={selected === key}
		onclick={() => select(key)}
	>
		{DECISIONS[key].label}
	</button>
{/snippet}

<div class="threeway">
	<div class="controls">
		<div class="configs-wrapper configs-inputs">
			<div class="configs input-container">
				BU stack (BB):
				<input bind:value={stackBu} class="config-input" type="number" step="0.5" min="0.5" />
			</div>
			<div class="configs input-container">
				SB stack (BB):
				<input bind:value={stackSb} class="config-input" type="number" step="0.5" min="0.5" />
			</div>
			<div class="configs input-container">
				BB stack (BB):
				<input bind:value={stackBb} class="config-input" type="number" step="0.5" min="0.5" />
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
	{:else if solution && selectedStrategy}
		<div class="wrapper">
			<nav class="tree">
				<ul>
					<li>
						{@render node('bu')}
						<ul>
							<li class="branch-label">Push</li>
							<ul>
								<li>
									{@render node('sbCall')}
									<ul>
										<li class="branch-label">Call</li>
										<ul>
											<li>{@render node('bbBoth')}</li>
										</ul>
										<li class="branch-label">Fold</li>
										<ul>
											<li>{@render node('bbBu')}</li>
										</ul>
									</ul>
								</li>
							</ul>
							<li class="branch-label">Fold</li>
							<ul>
								<li>
									{@render node('sbPush')}
									<ul>
										<li class="branch-label">Push</li>
										<ul>
											<li>{@render node('bbSb')}</li>
										</ul>
										<li class="branch-label">Fold</li>
										<li class="terminal">Hand ends</li>
									</ul>
								</li>
							</ul>
						</ul>
					</li>
				</ul>
			</nav>

			<div>
				{#each [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12] as rowIndex (rowIndex)}
					<div class="row">
						{#each [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12] as colIndex (colIndex)}
							<div class="cell">
								<div
									class="strategy-indicator"
									style="--fill: {selectedStrategy[rowIndex * 13 + colIndex]};"
								>
									{#if DECISIONS[selected].action === 'push'}
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

	{#if frequency}
		<div class="configs-wrapper">
			<div class="configs">
				{DECISIONS[selected].label}:
				<span class={DECISIONS[selected].action === 'push' ? 'push-summary' : 'call-summary'}>
					{DECISIONS[selected].action}
					{frequency.act}%
				</span>
				<span class="fold-summary">fold {frequency.fold}%</span>
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
