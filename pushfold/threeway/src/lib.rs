//! Three-handed (button / small blind / big blind) push/fold solver.
//!
//! Mirrors the heads-up crate's design — CFR+ over the 169-infoset hand grid,
//! driven entirely by precomputed lookup tables — but the extra player turns
//! the matchup and equity tables into 169^3 tensors (`ndarray`), and the game
//! tree grows to six binary decision points. Two-player showdowns (someone
//! folded, or the side pot between the two deeper stacks) are priced with the
//! shared heads-up equity table; three-way all-ins use the generated
//! three-way pot shares.

pub mod constants;
pub mod solver;
