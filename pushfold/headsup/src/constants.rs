use nalgebra::DMatrix;
use pushfold_shared::generated;

// Grid dimensions and the hand-label grid are game-agnostic and live in the
// shared crate; re-export them so `use crate::constants::*` keeps working.
pub use pushfold_shared::{N_INFOSETS, N_RANKS};

/// Tolerance of floating point equality
#[cfg(test)]
pub const F32_EPSILON: f32 = 1e-5;

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
            let equity = generated::headsup_equity::EQUITIES[k] as f32 / u16::MAX as f32;
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
            let count = generated::headsup_matchup::MATCHUPS[k] as f32;
            result[(i, j)] = count;
            result[(j, i)] = count;
            k += 1;
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn valid_equities() {
        let equity_table = equities();

        assert_eq!(equity_table.shape(), (N_INFOSETS, N_INFOSETS));

        for i in 0..N_INFOSETS {
            // Check all diagonal entries equal to 0.5
            assert_eq!(equity_table[(i, i)], 0.5);
            for j in (i + 1)..N_INFOSETS {
                // Check all off-diagonal entries are complementary
                assert!(equity_table[(i, j)] >= 0.0);
                assert!(equity_table[(i, j)] <= 1.0);
                assert!(equity_table[(j, i)] >= 0.0);
                assert!(equity_table[(j, i)] <= 1.0);
                assert!(
                    f32::abs(1.0 - (equity_table[(i, j)] + equity_table[(j, i)])) < F32_EPSILON
                );
            }
        }
    }

    #[test]
    fn valid_matchups() {
        let matchup_table = matchups();

        assert_eq!(matchup_table.shape(), (N_INFOSETS, N_INFOSETS));

        // Check that total matchups are equivalent to 52 choose 2, 2 = 52 * 51 * 50 * 49 / 4
        let mut total: u32 = 0;
        for i in 0..N_INFOSETS {
            for j in 0..N_INFOSETS {
                total += matchup_table[(i, j)] as u32;
            }
        }

        assert_eq!(total, 52 * 51 * 50 * 49 / 4);
    }
}
