/// Number of ranks, and the side length of the 13x13 hand grid.
pub const N_RANKS: usize = 13;
/// Number of infosets
pub const N_INFOSETS: usize = N_RANKS * N_RANKS;

/// Tolerance of floating point equality
#[cfg(test)]
pub const F32_EPSILON: f32 = 1e-5;
