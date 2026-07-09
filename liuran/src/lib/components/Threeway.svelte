<script lang="ts">
	import { computeFrequency } from '$lib/pushfold/frequencies';
	import { formatPct, formatExploitability } from '$lib/pushfold/format';
	import StrategyGrid from '$lib/components/StrategyGrid.svelte';
	import '$lib/styles/pushfold.css';
	import '$lib/styles/threeway.css';

	const N_ITER = 1000;
	// Solves settle within a few hundred ms of the last edit rather than
	// firing on every keystroke.
	const DEBOUNCE_MS = 300;

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

	// Three-way CFR+ is heavy enough (O(N^3) per iteration, 1000 iterations)
	// to freeze the page for several seconds if solved inline; it runs in a
	// dedicated worker instead, so the main thread stays responsive while it
	// churns. `solution` holds the last completed solve and is left in place
	// while a newer one is in flight, so the grid doesn't blank out on every
	// keystroke — only `solving` flips to show that a fresher answer is on
	// the way.
	let solution = $state<Solution | null>(null);
	let solving = $state(false);
	// Fraction of the in-flight solve's iterations completed, driven by the
	// Rust solver's own progress callback (see solve() in solver.rs) rather
	// than a guessed timer, so it tracks actual CFR+ progress.
	let solveProgress = $state(0);
	let solveError = $state<string | null>(null);

	let worker: Worker | null = null;
	// Bumped per dispatched request; a response is discarded unless it
	// matches the current value, which covers the (normally impossible) case
	// of a message from a worker whose termination hasn't taken effect yet.
	let latestRequestId = 0;

	type SolveParams = {
		stackBu: number;
		stackSb: number;
		stackBb: number;
		sb: number;
		ante: number;
	};

	// The wasm solve is one long synchronous call inside the worker, with no
	// point at which it checks for cancellation — so a superseded solve can't
	// be told to stop, only killed. Each dispatch therefore terminates
	// whatever worker is currently running (idle or not) and starts a fresh
	// one, instead of reusing one long-lived worker and letting stale
	// requests queue up behind it.
	function dispatchSolve(params: SolveParams) {
		worker?.terminate();

		const id = ++latestRequestId;
		const w = new Worker(new URL('../workers/threeway.worker.ts', import.meta.url), {
			type: 'module'
		});
		w.onmessage = (event: MessageEvent<import('$lib/workers/threeway.worker').SolveResponse>) => {
			const data = event.data;
			if (data.type === 'ready') {
				// The wasm import has resolved; only now is it safe to post the
				// solve request without racing the worker's module still
				// loading (a bare postMessage right after `new Worker()` can be
				// dropped before that import settles).
				w.postMessage({ id, ...params, iterations: N_ITER });
				return;
			}
			if (data.id !== latestRequestId) return;
			switch (data.type) {
				case 'progress':
					solveProgress = data.fraction;
					break;
				case 'done':
					solving = false;
					solution = {
						buPush: data.buPush,
						sbCall: data.sbCall,
						sbPush: data.sbPush,
						bbCallBoth: data.bbCallBoth,
						bbCallBu: data.bbCallBu,
						bbCallSb: data.bbCallSb,
						exploitability: data.exploitability
					};
					solveError = null;
					break;
				case 'error':
					// The Rust layer rejected something the UI check missed;
					// degrade gracefully rather than leave `solving` stuck.
					console.error('solver rejected inputs', data.error);
					solving = false;
					solveError = data.error;
					break;
			}
		};

		worker = w;
		solving = true;
		solveProgress = 0;
	}

	$effect(() => {
		return () => worker?.terminate();
	});

	$effect(() => {
		const params = { stackBu, stackSb, stackBb, sb, ante };
		if (validationError) return;
		const timer = setTimeout(() => dispatchSolve(params), DEBOUNCE_MS);
		return () => clearTimeout(timer);
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
		const act = computeFrequency(selectedStrategy) * 100;
		return { act: formatPct(act), fold: formatPct(100 - act) };
	});

	let exploitability = $derived.by(() =>
		solution ? formatExploitability(solution.exploitability) : null
	);

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

<div class="pushfold threeway">
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
			{#if solving}
				<span class="solving-indicator">Solving… {Math.round(solveProgress * 100)}%</span>
			{:else if solveError}
				<span class="solving-indicator" role="alert">{solveError}</span>
			{/if}
		</div>
	</div>

	{#if validationError}
		<div class="loading" role="alert">{validationError}</div>
	{:else if solution && selectedStrategy}
		<div class="wrapper" class:solving>
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

			<StrategyGrid strategy={selectedStrategy} action={DECISIONS[selected].action} />
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
