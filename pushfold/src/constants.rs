use crate::generated;
use nalgebra::DMatrix;

/// Number of ranks, and the side length of the 13x13 hand grid.
pub const N_RANKS: usize = 13;
/// Number of infosets
pub const N_INFOSETS: usize = N_RANKS * N_RANKS;
/// Number of matchups between infosets
pub const N_MATCHUPS: usize = N_INFOSETS * N_INFOSETS;

/// Expands the generated equity table into the full `N_INFOSETS x N_INFOSETS`
/// matrix of `f32` equities.
///
/// Entry m[(i,j)] is the equity of infoset `i` against infoset `j`.
///
/// The generated table (see the C++ `EquityGenerator`) stores only the strict
/// upper triangle as fixed-point `u16`: the diagonal is a known 0.5 (a hand has
/// 50% equity against itself) and the lower triangle is the complement, since
/// heads-up all-in equity is zero-sum.
pub fn equities() -> DMatrix<f32> {
    let mut result: DMatrix<f32> = DMatrix::<f32>::zeros(N_INFOSETS, N_INFOSETS);
    let mut k = 0; // running index into the flattened upper triangle
    for i in 0..N_INFOSETS {
        result[i * N_INFOSETS + i] = 0.5;
        for j in (i + 1)..N_INFOSETS {
            let equity = generated::equity::EQUITIES[k] as f32 / u16::MAX as f32;
            result[(i, j)] = equity;
            result[(j, i)] = 1.0 - equity;
            k += 1;
        }
    }
    result
}

/// Expands the generated matchup table into the full, symmetric
/// `N_INFOSETS x N_INFOSETS` matrix of deal counts, widened to `f32`
///
/// The generated table (see the C++ `MatchupGenerator`) stores only the upper
/// triangle including the diagonal; the matrix is symmetric, so the lower
/// triangle mirrors it. Widening the `u8` counts to `f32` here lets the solver
/// read them directly.
pub fn matchups() -> DMatrix<f32> {
    let mut result: DMatrix<f32> = DMatrix::<f32>::zeros(N_INFOSETS, N_INFOSETS);
    let mut k = 0; // running index into the flattened upper triangle (incl. diagonal)
    for i in 0..N_INFOSETS {
        for j in i..N_INFOSETS {
            let count = generated::matchup::MATCHUPS[k] as f32;
            result[(i, j)] = count;
            result[(j, i)] = count;
            k += 1;
        }
    }
    result
}

/// Maps each infoset index to its label (e.g. `"AKs"`), flattening the generated
/// 13x13 hand grid row-major to match the infoset numbering the lookup tables
/// use (`i / N_RANKS` selects the row, `i % N_RANKS` the column).
#[cfg(test)]
pub fn hands() -> [&'static str; N_INFOSETS] {
    std::array::from_fn(|i| generated::hands::HANDS[i / N_RANKS][i % N_RANKS])
}
