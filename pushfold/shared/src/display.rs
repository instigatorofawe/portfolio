use crate::constants::{N_INFOSETS, N_RANKS};

/// Maps each infoset index to its label (e.g. `"AKs"`), flattening the generated
/// 13x13 hand grid row-major to match the infoset numbering the lookup tables
/// use (`i / N_RANKS` selects the row, `i % N_RANKS` the column).
///
/// Behind the `hands` feature: it is reference data only the solver crates' tests
/// consume, so it stays out of release builds unless explicitly enabled.
#[cfg(feature = "hands")]
pub fn hands() -> [&'static str; N_INFOSETS] {
    std::array::from_fn(|i| crate::generated::headsup::hands::HANDS[i / N_RANKS][i % N_RANKS])
}

/// Renders a strategy vector as the 13x13 hand grid, one cell per infoset
/// showing the label and its action frequency as a percentage. The layout
/// matches a standard range chart: pairs on the diagonal, suited hands in
/// the upper triangle, offsuit in the lower.
#[cfg(feature = "hands")]
pub fn format_grid(strat: &[f32]) -> String {
    let labels = hands();
    let mut out = String::new();
    for r in 0..N_RANKS {
        for c in 0..N_RANKS {
            let i = r * N_RANKS + c;
            out.push_str(&format!("{:>4} {:>3.0}%  ", labels[i], strat[i] * 100.0));
        }
        out.push('\n');
    }
    out
}
