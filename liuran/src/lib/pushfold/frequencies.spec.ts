import { describe, it, expect } from 'vitest';
import { combosAt, computeFrequencies, N_HANDS, TOTAL_COMBOS, GRID_SIZE } from './frequencies';

describe('combosAt', () => {
	it('returns 6 combos for pairs (diagonal)', () => {
		expect(combosAt(0, 0)).toBe(6);
		expect(combosAt(7, 7)).toBe(6);
	});

	it('returns 4 combos for suited hands (upper triangle)', () => {
		expect(combosAt(0, 1)).toBe(4);
		expect(combosAt(2, 12)).toBe(4);
	});

	it('returns 12 combos for offsuit hands (lower triangle)', () => {
		expect(combosAt(1, 0)).toBe(12);
		expect(combosAt(12, 2)).toBe(12);
	});

	it('accounts for all 1326 combos across the grid', () => {
		let total = 0;
		for (let i = 0; i < GRID_SIZE; i++) {
			for (let j = 0; j < GRID_SIZE; j++) {
				total += combosAt(i, j);
			}
		}
		expect(total).toBe(TOTAL_COMBOS);
	});
});

describe('computeFrequencies', () => {
	it('reports zero action when nothing is played', () => {
		expect(computeFrequencies(Array(N_HANDS).fill(0), Array(N_HANDS).fill(0))).toEqual({
			push: 0,
			buFold: 1,
			call: 0,
			bbFold: 1
		});
	});

	it('reports full action when every hand is played', () => {
		const { push, buFold, call, bbFold } = computeFrequencies(
			Array(N_HANDS).fill(1),
			Array(N_HANDS).fill(1)
		);
		expect(push).toBeCloseTo(1, 10);
		expect(buFold).toBeCloseTo(0, 10);
		expect(call).toBeCloseTo(1, 10);
		expect(bbFold).toBeCloseTo(0, 10);
	});

	it('weights a single hand by its combo count', () => {
		// Only the first hand (index 0, a pair = 6 combos) is pushed on the button.
		const bu = Array(N_HANDS).fill(0);
		bu[0] = 1;

		const { push, buFold } = computeFrequencies(bu, Array(N_HANDS).fill(0));
		expect(push).toBeCloseTo(6 / TOTAL_COMBOS, 10);
		expect(buFold).toBeCloseTo(1 - 6 / TOTAL_COMBOS, 10);
	});

	it('reads BU and BB from their independent strategy arrays', () => {
		// Button pushes hand 0 (pair, 6 combos); big blind calls hand 1 (suited, 4 combos).
		const bu = Array(N_HANDS).fill(0);
		bu[0] = 1;
		const bb = Array(N_HANDS).fill(0);
		bb[1] = 1;

		const { push, call } = computeFrequencies(bu, bb);
		expect(push).toBeCloseTo(6 / TOTAL_COMBOS, 10);
		expect(call).toBeCloseTo(4 / TOTAL_COMBOS, 10);
	});

	it('handles fractional (mixed) strategies', () => {
		// Half-pushing every hand should yield half the full-push frequency.
		expect(computeFrequencies(Array(N_HANDS).fill(0.5), Array(N_HANDS).fill(0)).push).toBeCloseTo(
			0.5,
			10
		);
	});
});
