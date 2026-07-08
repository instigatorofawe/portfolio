// Aggregate per-hand push/fold (BU) and call/fold (BB) frequencies from a
// solver strategy. The solver returns two parallel arrays of 169 entries — the
// button's push frequency and the big blind's call frequency — each indexed by
// position in the 13x13 hand grid.
//
// Each grid cell maps to a different number of concrete two-card combos: pairs
// (the diagonal) have 6, suited hands (upper triangle) have 4, and offsuit
// hands (lower triangle) have 12. Weighting by combo count and normalizing by
// the 1326 total combos turns the per-hand strategy into an overall frequency.

export const N_HANDS = 169;
export const GRID_SIZE = 13;
export const TOTAL_COMBOS = 1326;

/** Number of concrete combos represented by a hand at grid position (i, j). */
export function combosAt(i: number, j: number): number {
	if (i === j) return 6; // pair
	if (i < j) return 4; // suited
	return 12; // offsuit
}

export type Frequencies = {
	/** Fraction of combos the button pushes (0–1). */
	push: number;
	/** Fraction of combos the button folds (0–1). */
	buFold: number;
	/** Fraction of combos the big blind calls (0–1). */
	call: number;
	/** Fraction of combos the big blind folds (0–1). */
	bbFold: number;
};

/**
 * Collapse a solver's push (BU) and call (BB) strategies into overall BU/BB
 * action frequencies, each expressed as a fraction in [0, 1]. `buPush` and
 * `bbCall` are the two 169-entry arrays returned by the solver.
 */
export function computeFrequencies(
	buPush: ArrayLike<number>,
	bbCall: ArrayLike<number>
): Frequencies {
	let push = 0;
	let call = 0;

	for (let index = 0; index < N_HANDS; index++) {
		const combos = combosAt(Math.floor(index / GRID_SIZE), index % GRID_SIZE);
		push += (buPush[index] * combos) / TOTAL_COMBOS;
		call += (bbCall[index] * combos) / TOTAL_COMBOS;
	}

	return { push, buFold: 1 - push, call, bbFold: 1 - call };
}
