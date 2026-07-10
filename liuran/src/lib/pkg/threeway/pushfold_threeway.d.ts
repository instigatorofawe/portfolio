/* tslint:disable */
/* eslint-disable */

/**
 * Solver result: the six averaged strategies, one per decision point.
 */
export class Strategies {
    private constructor();
    free(): void;
    [Symbol.dispose](): void;
    readonly exploitability: number;
    readonly bb_call_both: Float32Array;
    readonly bb_call_bu: Float32Array;
    readonly bb_call_sb: Float32Array;
    readonly bu_push: Float32Array;
    readonly sb_call: Float32Array;
    readonly sb_push: Float32Array;
}

/**
 * Three-handed push/fold solver.
 *
 * The game: the button open-pushes or folds; the small blind reacts (call a
 * push, or open-push once the button folds); the big blind closes the action
 * at one of three histories. Two-player showdowns and the side pot of a
 * three-way all-in are priced with the heads-up equity table (the third
 * player's card removal is ignored); a three-way all-in's main pot uses the
 * generated three-way pot shares.
 *
 * Runs CFR+ exactly like the heads-up solver — clamped regrets, alternating
 * updates (BU, then SB, then BB, each against the freshest strategies),
 * linear averaging. With three players CFR carries no convergence guarantee
 * to a Nash equilibrium, so `solve` reports NashConv (the summed
 * best-response gaps) as `exploitability`: a small value still certifies the
 * profile is hard to exploit.
 */
export class ThreewaySolver {
    free(): void;
    [Symbol.dispose](): void;
    constructor();
    /**
     * Runs CFR+ (regret clamping, alternating updates, linear averaging)
     * and returns the averaged strategies with their NashConv gap.
     *
     * A solve is O(N^3) per iteration and can run for several seconds; when
     * `progress` is given, it's called with the fraction of iterations
     * complete (in (0, 1], reaching exactly 1 on the last iteration) so a
     * caller can render a progress indicator. Reported at most ~100 times
     * regardless of `iterations`, so the callback overhead stays negligible
     * next to the O(N^3) work it's interleaved with.
     *
     * Thin boundary wrapper: rejected inputs surface to JS as an `Error`
     * whose message is the `SolverError` `Display` string. The typed result
     * lives in `solve_inner`, which tests exercise natively (a `JsError` can
     * neither be constructed nor `Debug`-printed off-wasm).
     */
    solve(stack_bu: number, stack_sb: number, stack_bb: number, sb: number, ante: number, iterations: number, progress?: Function | null): Strategies;
}
