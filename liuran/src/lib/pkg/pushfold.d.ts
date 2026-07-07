/* tslint:disable */
/* eslint-disable */

/**
 * Push/fold solver over the 169 canonical infosets.
 *
 * All per-iteration work is four 169x169 matrix-vector products against two
 * constant matrices: the matchup weights `W` (card removal baked into deal
 * counts) and the weight-fused call payouts `B = W .* pc`. The full
 * matchup-level (169^2) probability and EV matrices are never materialized;
 * every quantity CFR needs is a contraction of those matrices with the
 * players' strategy vectors.
 *
 * Sign convention: all payoffs are from the button's perspective; the big
 * blind maximizes their negation.
 */
export class PushFoldSolver {
    free(): void;
    [Symbol.dispose](): void;
    /**
     * Initializes the environment. The equity and matchup tables are
     * constant for the lifetime of the solver; everything stake-dependent
     * is rebuilt per solve.
     */
    constructor();
    /**
     * Runs CFR+ (regret clamping, alternating updates, linear averaging)
     * and returns the averaged strategies with their exploitability.
     */
    solve(stack: number, sb: number, ante: number, iterations: number): Strategies;
}

/**
 * Why the solver rejected its inputs. Annotated for wasm-bindgen so it crosses
 * into JS as a numeric discriminant; the human-readable message for each
 * variant lives on the consumer.
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
 * Averaged (Nash-converging) strategies, indexed by canonical infoset
 * (row-major over the 13x13 hand grid). Values are the push frequency for
 * the button and the call frequency for the big blind.
 *
 * wasm-bindgen surfaces this to JS as an opaque handle: the `bu_push` and
 * `bb_call` getters flatten the nalgebra vectors into `Float32Array`-shaped
 * `Vec<f32>`, so the `DVector` fields are `skip`ped (they can't cross into JS
 * directly) and re-exposed through those getters.
 */
export class Strategies {
    private constructor();
    free(): void;
    [Symbol.dispose](): void;
    /**
     * Nash gap of the averaged strategy pair, in big blinds per deal (sum of
     * both players' best-response improvements). A plain scalar, so
     * wasm-bindgen exposes it directly; `readonly` keeps it getter-only.
     */
    readonly exploitability: number;
    /**
     * Big blind call frequency per infoset (169 entries).
     */
    readonly bb_call: Float32Array;
    /**
     * Button push frequency per infoset (169 entries).
     */
    readonly bu_push: Float32Array;
}
