/* tslint:disable */
/* eslint-disable */

/**
 * Why the solver rejected its inputs. Exported as a `#[wasm_bindgen]` enum, so
 * it crosses into JS as a discriminant the caller can compare against
 * `SolverError.*` rather than parsing a free-text string. The human-readable
 * message for each variant lives on the consumer (see PushFold.svelte).
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
     * iter was zero.
     */
    ZeroIterations = 4,
}

export function solve_push_fold(stack: number, sb: number, ante: number, iter: number): Float32Array;
