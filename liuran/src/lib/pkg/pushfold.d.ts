/* tslint:disable */
/* eslint-disable */

export class PushFoldSolver {
    free(): void;
    [Symbol.dispose](): void;
    constructor();
    /**
     * Runs CFR+ (regret clamping, alternating updates, linear averaging)
     * and returns the averaged strategies with their exploitability.
     */
    solve(stack: number, sb: number, ante: number, iterations: number): Strategies;
}

/**
 * Input validation errors
 */
export enum SolverError {
    NonFiniteInput = 0,
    StackNotPositive = 1,
    NegativeBlindOrAnte = 2,
    SmallBlindExceedsStack = 3,
    ZeroIterations = 4,
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
