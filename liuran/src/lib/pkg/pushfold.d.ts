/* tslint:disable */
/* eslint-disable */

/**
 * Push/fold solver handle. Constructing it builds the (constant) equity and
 * matchup lookup tables once; reuse the same handle to solve at different
 * stakes rather than paying that setup per solve.
 */
export class PushFoldSolver {
    free(): void;
    [Symbol.dispose](): void;
    constructor();
    /**
     * Runs CFR+ for `iterations` and returns the averaged strategies, or
     * throws a [`SolverError`] into JS if the stakes are invalid.
     */
    solve(stack: number, sb: number, ante: number, iterations: number): Strategies;
}

/**
 * Why the solver rejected its inputs. Annotated for wasm-bindgen so it crosses
 * into JS as a numeric discriminant (see `bindings.rs`); the human-readable
 * message for each variant lives on the consumer.
 */
export enum SolverError {
    /**
     * stack, SB, or ante was NaN or infinite.
     */
    NonFiniteInput = 0,
    /**
     * stack was zero or negative.
     */
    StackNotPositive = 1,
    /**
     * SB or ante was negative.
     */
    NegativeBlindOrAnte = 2,
    /**
     * SB was larger than the stack.
     */
    SmallBlindExceedsStack = 3,
    /**
     * iterations was zero.
     */
    ZeroIterations = 4,
}

/**
 * Converged strategies from a solve. `bu_push` and `bb_call` are each 169
 * frequencies in `[0, 1]`, row-major over the 13x13 hand grid (the button's
 * push frequency and the big blind's call frequency respectively); wasm-bindgen
 * surfaces the getters to JS as `Float32Array`s.
 */
export class Strategies {
    private constructor();
    free(): void;
    [Symbol.dispose](): void;
    /**
     * Big blind call frequency per infoset (169 entries).
     */
    readonly bb_call: Float32Array;
    /**
     * Button push frequency per infoset (169 entries).
     */
    readonly bu_push: Float32Array;
    /**
     * Nash gap of the returned strategy pair, in big-blind units per deal.
     */
    readonly exploitability: number;
}
