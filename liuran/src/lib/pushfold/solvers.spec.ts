import { afterAll, describe, expect, it } from 'vitest';
import { HeadsUpSolver } from '../pkg/headsup/pushfold_headsup';
import { ThreewaySolver } from '../pkg/threeway/pushfold_threeway';

// Exercises the committed WASM through the same boundary the app uses. The
// error cases pin the contract the components' catch sites rely on: rejected
// inputs throw a real `Error` whose message is the shared SolverError Display
// string (see pushfold/shared/src/lib.rs) — not a bare variant number, which
// is what a `#[wasm_bindgen]` error enum would surface. The message
// assertions are exact on purpose: these strings render verbatim in the UI's
// error alerts, so a change to them is a copy change and should be conscious.

const N_HANDS = 169;

function solveError(run: () => unknown): Error {
	let thrown: unknown;
	try {
		run();
	} catch (e) {
		thrown = e;
	}
	expect(thrown).toBeInstanceOf(Error);
	return thrown as Error;
}

describe('HeadsUpSolver', () => {
	const solver = new HeadsUpSolver();
	afterAll(() => solver.free());

	it('solves valid inputs into 169-entry strategies', () => {
		const result = solver.solve(5.0, 0.5, 0.125, 100);
		const buPush = result.bu_push;
		const bbCall = result.bb_call;
		const exploitability = result.exploitability;
		result.free();

		expect(buPush).toHaveLength(N_HANDS);
		expect(bbCall).toHaveLength(N_HANDS);
		expect(Number.isFinite(exploitability)).toBe(true);
	});

	it.each([
		{ args: [NaN, 0.5, 0.125, 100], message: 'Inputs must be finite numbers.' },
		{ args: [-1, 0.5, 0.125, 100], message: 'Stacks must be positive.' },
		{ args: [5, -0.5, 0.125, 100], message: 'Small blind and ante must be non-negative.' },
		{ args: [5, 6, 0.125, 100], message: 'Small blind cannot exceed the stack.' },
		{ args: [5, 0.5, 0.125, 0], message: 'Iterations must be at least 1.' }
	])('rejects invalid inputs with a readable Error: "$message"', ({ args, message }) => {
		const [stack, sb, ante, iterations] = args;
		const error = solveError(() => solver.solve(stack, sb, ante, iterations));
		expect(error.message).toBe(message);
	});
});

describe('ThreewaySolver', () => {
	const solver = new ThreewaySolver();
	afterAll(() => solver.free());

	it('solves valid inputs into 169-entry strategies and reports progress', () => {
		const fractions: number[] = [];
		const result = solver.solve(5.0, 5.0, 5.0, 0.5, 0.125, 5, (f: number) => fractions.push(f));
		const buPush = result.bu_push;
		const bbCallBoth = result.bb_call_both;
		const exploitability = result.exploitability;
		result.free();

		expect(buPush).toHaveLength(N_HANDS);
		expect(bbCallBoth).toHaveLength(N_HANDS);
		expect(Number.isFinite(exploitability)).toBe(true);

		// Progress fractions arrive in order, within (0, 1], ending exactly at 1.
		expect(fractions.length).toBeGreaterThan(0);
		expect(fractions).toEqual([...fractions].sort((a, b) => a - b));
		expect(fractions.every((f) => f > 0 && f <= 1)).toBe(true);
		expect(fractions.at(-1)).toBe(1);
	});

	it('rejects a BB stack shorter than the big blind with a readable Error', () => {
		const error = solveError(() => solver.solve(5.0, 5.0, 0.5, 0.5, 0.125, 100, undefined));
		expect(error.message).toBe('BB stack must be at least 1 big blind.');
	});
});
