//! Primitives and lookup tables shared by the push/fold solver crates.
//!
//! Holds the game-agnostic constants and reference data (grid dimensions,
//! infoset count, the hand-label grid) plus every generated lookup table. The
//! heads-up (pairwise) tables are `headsup_`-prefixed so the three-way solver,
//! which reuses the heads-up equities, can read them from here and add its own
//! `threeway_`-prefixed tables alongside without collision. The solvers
//! themselves live in their own crates (`pushfold-headsup`, ...).

pub mod generated;

/// Tolerance of floating point equality
#[cfg(test)]
pub const F32_EPSILON: f32 = 1e-5;

/// Number of ranks, and the side length of the 13x13 hand grid.
pub const N_RANKS: usize = 13;
/// Number of infosets
pub const N_INFOSETS: usize = N_RANKS * N_RANKS;

/// Maps each infoset index to its label (e.g. `"AKs"`), flattening the generated
/// 13x13 hand grid row-major to match the infoset numbering the lookup tables
/// use (`i / N_RANKS` selects the row, `i % N_RANKS` the column).
///
/// Behind the `hands` feature: it is reference data only the solver crates' tests
/// consume, so it stays out of release builds unless explicitly enabled.
#[cfg(feature = "hands")]
pub fn hands() -> [&'static str; N_INFOSETS] {
    std::array::from_fn(|i| generated::hands::HANDS[i / N_RANKS][i % N_RANKS])
}
