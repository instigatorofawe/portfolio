// Runs the three-way solve off the main thread. Three-way CFR+ is O(N^3) per
// iteration (169^2 x 169 tensor contractions, several per iteration, 1000
// iterations), which takes long enough to noticeably freeze the page if run
// inline; a dedicated worker keeps input handling and layout responsive while
// it churns.
import { ThreewaySolver } from '$lib/pkg/threeway/pushfold_threeway';

export type SolveRequest = {
	id: number;
	stackBu: number;
	stackSb: number;
	stackBb: number;
	sb: number;
	ante: number;
	iterations: number;
};

export type SolveResponse =
	// A fresh worker is spun up per solve (see Threeway.svelte) so a stale
	// solve can be killed outright instead of queuing behind it; posted as
	// soon as the wasm import has resolved, so the caller knows it's safe to
	// post a SolveRequest without racing the module still loading.
	| { type: 'ready' }
	/** Fraction of iterations complete, in (0, 1]; reported at most ~100 times. */
	| { id: number; type: 'progress'; fraction: number }
	| {
			id: number;
			type: 'done';
			buPush: Float32Array;
			sbCall: Float32Array;
			sbPush: Float32Array;
			bbCallBoth: Float32Array;
			bbCallBu: Float32Array;
			bbCallSb: Float32Array;
			exploitability: number;
	  }
	| { id: number; type: 'error'; error: string };

// The DOM and webworker TS libs declare incompatible types for `self`, and
// this file lives in a project whose tsconfig is shared with DOM code; a
// narrow local type for the worker-global calls we actually use sidesteps
// that conflict instead of pulling in the webworker lib.
type WorkerGlobal = {
	onmessage: ((event: MessageEvent<SolveRequest>) => void) | null;
	postMessage(message: SolveResponse, transfer: Transferable[]): void;
};
const workerSelf = self as unknown as WorkerGlobal;

// One solver instance per worker; its lookup tables are built once on
// construction and reused across solves, same as the main-thread pattern in
// PushFold.svelte.
let solver: ThreewaySolver | null = null;

workerSelf.postMessage({ type: 'ready' }, []);

workerSelf.onmessage = (event) => {
	const { id, stackBu, stackSb, stackBb, sb, ante, iterations } = event.data;
	solver ??= new ThreewaySolver();

	try {
		const result = solver.solve(
			stackBu,
			stackSb,
			stackBb,
			sb,
			ante,
			iterations,
			(fraction: number) => workerSelf.postMessage({ id, type: 'progress', fraction }, [])
		);
		const buPush = result.bu_push;
		const sbCall = result.sb_call;
		const sbPush = result.sb_push;
		const bbCallBoth = result.bb_call_both;
		const bbCallBu = result.bb_call_bu;
		const bbCallSb = result.bb_call_sb;
		const exploitability = result.exploitability;
		result.free();

		// Each array is wasm-bindgen's own copy (a fresh ArrayBuffer, not a
		// view into wasm memory), so transferring it back is safe and free of
		// an extra structured-clone copy.
		workerSelf.postMessage(
			{ id, type: 'done', buPush, sbCall, sbPush, bbCallBoth, bbCallBu, bbCallSb, exploitability },
			[
				buPush.buffer,
				sbCall.buffer,
				sbPush.buffer,
				bbCallBoth.buffer,
				bbCallBu.buffer,
				bbCallSb.buffer
			]
		);
	} catch (e) {
		// The Rust layer rejected something the UI check missed; report it
		// rather than let the worker die silently.
		workerSelf.postMessage({ id, type: 'error', error: String(e) }, []);
	}
};
