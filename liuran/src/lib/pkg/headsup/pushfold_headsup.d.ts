/* tslint:disable */
/* eslint-disable */

export class HeadsUpSolver {
    free(): void;
    [Symbol.dispose](): void;
    constructor();
    /**
     * Runs CFR+ (regret clamping, alternating updates, linear averaging)
     * and returns the averaged strategies with their exploitability.
     *
     * Thin boundary wrapper: rejected inputs surface to JS as an `Error`
     * whose message is the `SolverError` `Display` string. The typed result
     * lives in `solve_inner`, which tests exercise natively (a `JsError` can
     * neither be constructed nor `Debug`-printed off-wasm).
     */
    solve(stack: number, sb: number, ante: number, iterations: number): Strategies;
}

/**
 * Solver result
 */
export class Strategies {
    private constructor();
    free(): void;
    [Symbol.dispose](): void;
    readonly exploitability: number;
    readonly bb_call: Float32Array;
    readonly bu_push: Float32Array;
}
