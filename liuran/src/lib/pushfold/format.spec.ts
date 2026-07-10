import { describe, expect, it } from 'vitest';
import { formatPct, formatExploitability } from './format';

describe('formatPct', () => {
	it('formats to at most 4 significant digits', () => {
		expect(formatPct(33.33333)).toBe('33.33');
		expect(formatPct(100)).toBe('100');
		expect(formatPct(7.5)).toBe('7.5');
	});

	it('clamps sub-0.01% values to a flat zero', () => {
		expect(formatPct(0.009)).toBe('0.000');
		expect(formatPct(0)).toBe('0.000');
	});
});

describe('formatExploitability', () => {
	it('converts bb/deal to bb/100 with 3 significant digits', () => {
		expect(formatExploitability(0.0012345)).toBe('0.123');
		expect(formatExploitability(0.05)).toBe('5');
	});

	it('clamps effectively-zero values to a flat zero', () => {
		expect(formatExploitability(1e-9)).toBe('0.000000');
	});
});
