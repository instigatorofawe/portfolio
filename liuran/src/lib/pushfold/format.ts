// Display formatting shared by every solver UI (heads-up, three-way). Both
// solvers collapse their output into the same two display strings — an
// action-frequency percentage and an exploitability figure in bb/100 — with
// identical rounding and small-value clamping, kept here so the two stay in
// step.

const pctFormatter = new Intl.NumberFormat('en-US', { maximumSignificantDigits: 4 });

/**
 * Format an action frequency, already expressed as a percentage (0–100), for
 * display. Percentages below 0.01% are visual noise and clamp to a flat
 * "0.000".
 */
export function formatPct(value: number): string {
	return value < 0.01 ? '0.000' : pctFormatter.format(value);
}

const exploitabilityFormatter = new Intl.NumberFormat('en-US', { maximumSignificantDigits: 3 });

/**
 * Format a solver's exploitability — the Nash/NashConv gap in BB per deal — as
 * the standard bb/100 (big blinds won per 100 hands) win-rate string poker
 * players read. Below 0.000001 bb/100 the value is effectively zero and shows
 * a flat "0.000000".
 */
export function formatExploitability(exploitability: number): string {
	const value = exploitability * 100;
	return value < 0.000001 ? '0.000000' : exploitabilityFormatter.format(value);
}
