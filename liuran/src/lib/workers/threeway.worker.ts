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
	| {
			id: number;
			ok: true;
			buPush: Float32Array;
			sbCall: Float32Array;
			sbPush: Float32Array;
			bbCallBoth: Float32Array;
			bbCallBu: Float32Array;
			bbCallSb: Float32Array;
			exploitability: number;
	  }
	| { id: number; ok: false; error: string };

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

workerSelf.onmessage = (event) => {
	const { id, stackBu, stackSb, stackBb, sb, ante, iterations } = event.data;
	solver ??= new ThreewaySolver();

	try {
		const result = solver.solve(stackBu, stackSb, stackBb, sb, ante, iterations);
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
			{ id, ok: true, buPush, sbCall, sbPush, bbCallBoth, bbCallBu, bbCallSb, exploitability },
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
		workerSelf.postMessage({ id, ok: false, error: String(e) }, []);
	}
};
