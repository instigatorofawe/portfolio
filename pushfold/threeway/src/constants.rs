use ndarray::{Array2, Array3};
use pushfold_shared::generated;

// Grid dimensions and the hand-label grid are game-agnostic and live in the
// shared crate; re-export them so `use crate::constants::*` keeps working.
pub use pushfold_shared::{N_INFOSETS, N_RANKS};

/// Tolerance of floating point equality
#[cfg(test)]
pub const F32_EPSILON: f32 = 1e-5;

/// Walks the flattened three-way tables in storage order, handing the closure
/// each flat index alongside its infoset triple `low <= mid <= high`.
///
/// The generated tables have one slot per unordered infoset multiset, ranked
/// colexicographically (see the C++ `ColexIndex`). That ranking visits slots
/// 0, 1, 2, ... exactly when `high` is the outer loop and `low` the inner, so
/// a running counter recovers every slot's triple without porting the
/// combinatorial unranking.
fn for_each_multiset(mut f: impl FnMut(usize, usize, usize, usize)) {
    let mut flat = 0;
    for high in 0..N_INFOSETS {
        for mid in 0..=high {
            for low in 0..=mid {
                f(flat, low, mid, high);
                flat += 1;
            }
        }
    }
}

/// Expands the generated matchup table into the full
/// `N_INFOSETS x N_INFOSETS x N_INFOSETS` tensor of deal counts, widened to
/// `f32`.
///
/// The generated table (see the C++ `MatchupGenerator`) stores one slot per
/// unordered infoset multiset, holding the deal count for a single seating
/// (ordered triple). Writing that count to every axis permutation therefore
/// yields a fully symmetric tensor: entry `[i, j, k]` is how many ways the
/// specific ordered seats hold infosets `i`, `j`, `k`.
pub fn matchups() -> Array3<f32> {
    let mut result = Array3::<f32>::zeros((N_INFOSETS, N_INFOSETS, N_INFOSETS));
    for_each_multiset(|flat, low, mid, high| {
        let count = generated::threeway::threeway_matchup::MATCHUPS[flat] as f32;
        result[[low, mid, high]] = count;
        result[[low, high, mid]] = count;
        result[[mid, low, high]] = count;
        result[[mid, high, low]] = count;
        result[[high, low, mid]] = count;
        result[[high, mid, low]] = count;
    });
    result
}

/// Expands the generated equity table into the full
/// `N_INFOSETS x N_INFOSETS x N_INFOSETS` tensor of three-way pot shares.
///
/// Entry `[own, a, b]` is the pot share of infoset `own` all-in against `a`
/// and `b`; the trailing two axes are symmetric (the opponents are unordered).
///
/// The generated table (see the C++ `EquityGenerator`) stores, per multiset,
/// the shares of the two lowest sorted infosets as fixed-point `u16`; the
/// third share is the remainder, since the three shares of an all-in pot sum
/// to 1. Impossible matchups (e.g. three identical pocket pairs) are stored as
/// zeros, so their reconstructed third share is 1 — meaningless, but their
/// deal count is zero, so a count-weighted consumer never reads them.
pub fn equities() -> Array3<f32> {
    let mut result = Array3::<f32>::zeros((N_INFOSETS, N_INFOSETS, N_INFOSETS));
    for_each_multiset(|flat, low, mid, high| {
        let [q_low, q_mid] = generated::threeway::threeway_equity::EQUITIES[flat];
        let e_low = q_low as f32 / u16::MAX as f32;
        let e_mid = q_mid as f32 / u16::MAX as f32;
        let e_high = 1.0 - e_low - e_mid;
        // When the multiset repeats an infoset the cells below collide; the
        // repeated positions carry (nearly) equal shares, so any write order
        // is consistent, but writing `high` first lets a directly quantized
        // share win over the reconstructed remainder on a collision.
        result[[high, low, mid]] = e_high;
        result[[high, mid, low]] = e_high;
        result[[mid, low, high]] = e_mid;
        result[[mid, high, low]] = e_mid;
        result[[low, mid, high]] = e_low;
        result[[low, high, mid]] = e_low;
    });
    result
}

/// Expands the generated heads-up equity table into the full
/// `N_INFOSETS x N_INFOSETS` matrix of `f32` equities.
///
/// Entry `[i, j]` is the equity of infoset `i` against infoset `j`. The
/// three-way solver uses it whenever only two players see a showdown: one
/// player folded, or the side pot between the two deeper stacks. The folded
/// (or short) player's card removal is ignored — the standard approximation.
///
/// Same decoding as the heads-up crate's `equities()`: the generated table
/// stores only the strict upper triangle as fixed-point `u16`; the diagonal
/// is a known 0.5 and the lower triangle is the complement.
pub fn headsup_equities() -> Array2<f32> {
    let mut result = Array2::<f32>::zeros((N_INFOSETS, N_INFOSETS));
    let mut k = 0; // running index into the flattened upper triangle
    for i in 0..N_INFOSETS {
        result[[i, i]] = 0.5;
        for j in (i + 1)..N_INFOSETS {
            let equity = generated::headsup::headsup_equity::EQUITIES[k] as f32 / u16::MAX as f32;
            result[[i, j]] = equity;
            result[[j, i]] = 1.0 - equity;
            k += 1;
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn valid_matchups() {
        let matchup_table = matchups();

        assert_eq!(matchup_table.shape(), &[N_INFOSETS, N_INFOSETS, N_INFOSETS]);

        // Fully symmetric: the two transpositions below generate all 3! axis
        // permutations.
        assert_eq!(matchup_table, matchup_table.view().permuted_axes([1, 0, 2]));
        assert_eq!(matchup_table, matchup_table.view().permuted_axes([0, 2, 1]));

        // Total ordered deals: C(52,2) * C(50,2) * C(48,2).
        let total: f64 = matchup_table.iter().map(|&w| w as f64).sum();
        assert_eq!(total, 1326.0 * 1225.0 * 1128.0);
    }

    #[test]
    fn valid_equities() {
        let equity_table = equities();
        let matchup_table = matchups();

        assert_eq!(equity_table.shape(), &[N_INFOSETS, N_INFOSETS, N_INFOSETS]);

        // Opponents are unordered, so the trailing axes are symmetric.
        assert_eq!(equity_table, equity_table.view().permuted_axes([0, 2, 1]));

        // For every dealable matchup the three shares are valid probabilities
        // summing to the whole pot (within quantization).
        for_each_multiset(|_, low, mid, high| {
            if matchup_table[[low, mid, high]] == 0.0 {
                return;
            }
            let shares = [
                equity_table[[low, mid, high]],
                equity_table[[mid, low, high]],
                equity_table[[high, low, mid]],
            ];
            for share in shares {
                assert!((0.0..=1.0).contains(&share), "share out of range: {share}");
            }
            // Each share is quantized independently (a repeated infoset reads
            // a quantized share where the exact table would have had the
            // remainder), so the sum is only accurate to a few quanta.
            let sum: f32 = shares.iter().sum();
            assert!(
                (sum - 1.0).abs() < 3.0 / u16::MAX as f32,
                "shares of ({low}, {mid}, {high}) sum to {sum}"
            );
        });
    }

    #[test]
    fn valid_headsup_equities() {
        let equity_table = headsup_equities();

        assert_eq!(equity_table.shape(), &[N_INFOSETS, N_INFOSETS]);

        for i in 0..N_INFOSETS {
            assert_eq!(equity_table[[i, i]], 0.5);
            for j in (i + 1)..N_INFOSETS {
                let upper = equity_table[[i, j]];
                let lower = equity_table[[j, i]];
                assert!((0.0..=1.0).contains(&upper));
                assert!((0.0..=1.0).contains(&lower));
                assert!(f32::abs(1.0 - (upper + lower)) < F32_EPSILON);
            }
        }
    }
}
