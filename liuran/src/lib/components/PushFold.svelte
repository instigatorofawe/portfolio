<script lang="ts">
	import { computeFrequencies } from '$lib/pushfold/frequencies';
	import { formatPct, formatExploitability } from '$lib/pushfold/format';
	import StrategyGrid from '$lib/components/StrategyGrid.svelte';
	import '$lib/styles/pushfold.css';
	import '$lib/styles/headsup.css';

	type SolverModule = typeof import('$lib/pkg/headsup/pushfold_headsup');
	type Solver = InstanceType<SolverModule['HeadsUpSolver']>;

	const N_ITER = 1000;
	// The heads-up solve is fast enough (a few ms) to run inline on the main
	// thread, unlike three-way, which needs a worker. A short debounce — just a
	// few times a single solve — collapses a burst of keystrokes into one solve
	// without any perceptible lag between the last edit and the result.
	const DEBOUNCE_MS = 30;

	// The solver is backed by a WASM module that instantiates asynchronously.
	// Loading it through a dynamic import (resolved only once the module is ready)
	// keeps this component out of the static module graph that has to await the
	// WASM, which is what broke client-side navigation in dev, and avoids running
	// the solver during static prerendering. The solver builds its lookup tables
	// once on construction, so we keep a single instance and reuse it per solve.
	let solver = $state<Solver | null>(null);

	$effect(() => {
		// If the component unmounts before the import resolves, the cleanup
		// below has already run; `cancelled` stops the late resolution from
		// constructing a solver whose WASM memory nothing would ever free.
		let cancelled = false;
		let instance: Solver | null = null;
		import('$lib/pkg/headsup/pushfold_headsup').then((m) => {
			if (cancelled) return;
			instance = new m.HeadsUpSolver();
			solver = instance;
		});
		return () => {
			cancelled = true;
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

	// The last completed solve. Kept in place while a newer debounced solve is
	// pending so the grid doesn't blank out between keystrokes; the template
	// hides it behind the validation branch whenever the inputs are invalid.
	let solution = $state<Solution | null>(null);
	// Message from the last solve the Rust layer rejected — an edge the client
	// validation missed. Shown alongside the controls and cleared on the next
	// successful solve, mirroring the three-way solver.
	let solveError = $state<string | null>(null);

	type SolveParams = { stack: number; sb: number; ante: number };

	function solve({ stack, sb, ante }: SolveParams) {
		if (!solver) return;
		try {
			// The returned Strategies is a WASM object whose getters copy on each
			// access; read each field once into a plain JS value, then free the
			// object so its WASM memory isn't held until GC.
			const result = solver.solve(stack, sb, ante, N_ITER);
			const next = {
				buPush: result.bu_push,
				bbCall: result.bb_call,
				exploitability: result.exploitability
			};
			result.free();
			solution = next;
			solveError = null;
		} catch (e) {
			// The Rust layer rejected something the UI check missed; surface it
			// rather than silently blank the grid. The previous solution stays on
			// screen so the user keeps a reference point.
			console.error('solver rejected inputs', e);
			// The Rust wrapper throws a real Error whose message is the
			// SolverError Display string; show that, not "Error: …".
			solveError = e instanceof Error ? e.message : String(e);
		}
	}

	// Whether the first solve has run. The initial solve fires immediately so the
	// grid appears as soon as the solver is ready; only later edits are debounced,
	// so rapid typing doesn't fire the synchronous solve on every keystroke.
	let hasSolved = false;

	// Re-solve when the solver becomes ready or a valid input changes.
	$effect(() => {
		const params = { stack, sb, ante };
		if (!solver || validationError) return;
		if (!hasSolved) {
			hasSolved = true;
			solve(params);
			return;
		}
		const timer = setTimeout(() => solve(params), DEBOUNCE_MS);
		return () => clearTimeout(timer);
	});

	let selected = $state('bu');

	let frequencies = $derived.by(() => {
		if (!solution) return null;
		const { push, buFold, call, bbFold } = computeFrequencies(solution.buPush, solution.bbCall);
		return [
			formatPct(push * 100),
			formatPct(buFold * 100),
			formatPct(call * 100),
			formatPct(bbFold * 100)
		];
	});

	let exploitability = $derived.by(() =>
		solution ? formatExploitability(solution.exploitability) : null
	);

	function reset() {
		stack = 5.0;
		sb = 0.5;
		ante = 0.125;
	}

	function select(id: string) {
		selected = id;
	}
</script>

<div class="pushfold headsup">
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
			{#if solveError}
				<span class="solve-error" role="alert">{solveError}</span>
			{/if}
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

			<StrategyGrid
				strategy={selected == 'bu' ? solution.buPush : solution.bbCall}
				action={selected == 'bu' ? 'push' : 'call'}
			/>
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
