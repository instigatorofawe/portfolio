//! Primitives and lookup tables shared by the push/fold solver crates.
//!
//! Holds the game-agnostic constants and reference data (grid dimensions,
//! infoset count, the hand-label grid) plus every generated lookup table. Each
//! game's tables live in their own directory module (`generated::headsup`,
//! `generated::threeway`); the solvers themselves live in their own crates
//! (`pushfold-headsup`, ...).

pub mod constants;
pub mod display;
pub mod error;
pub mod generated;

#[cfg(test)]
pub use constants::F32_EPSILON;
pub use constants::{N_INFOSETS, N_RANKS};
#[cfg(feature = "hands")]
pub use display::{format_grid, hands};
pub use error::SolverError;
