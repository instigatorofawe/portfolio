use crate::constants::*;
use nalgebra::{DMatrix, DVector};

/// Why the solver rejected its inputs.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SolverError {
    /// stack, SB, or ante was NaN or infinite.
    NonFiniteInput,
    /// stack was zero or negative.
    StackNotPositive,
    /// SB or ante was negative.
    NegativeBlindOrAnte,
    /// SB was larger than the stack.
    SmallBlindExceedsStack,
    /// iterations was zero.
    ZeroIterations,
}

/// Averaged (Nash-converging) strategies, indexed by canonical infoset
/// (row-major over the 13x13 hand grid). Values are the push frequency for
/// the button and the call frequency for the big blind.
pub struct Strategies {
    pub bu_push: DVector<f32>,
    pub bb_call: DVector<f32>,
    /// Nash gap of the averaged strategy pair, in units of the small blind's
    /// bet size per deal (sum of both players' best-response improvements).
    pub exploitability: f32,
}

/// Push/fold solver over the 169 canonical infosets.
///
/// All per-iteration work is four 169x169 matrix-vector products against two
/// constant matrices: the matchup weights `W` (card removal baked into deal
/// counts) and the weight-fused call payouts `B = W .* pc`. The full
/// matchup-level (169^2) probability and EV matrices are never materialized;
/// every quantity CFR needs is a contraction of those matrices with the
/// players' strategy vectors.
///
/// Sign convention: all payoffs are from the button's perspective; the big
/// blind maximizes their negation.
pub struct PushFoldSolver {
    /// e(i, j): all-in equity of infoset i vs infoset j.
    equity_table: DMatrix<f32>,
    /// W(i, j): count of card-disjoint deals. Row i = button hand,
    /// column j = big blind hand.
    matchup_table: DMatrix<f32>,

    // --- Per-solve derived state, rebuilt by `setup` ---
    /// B(i, j) = W(i, j) * pc(i, j), where pc is the all-in call payout
    /// (button perspective).
    b: DMatrix<f32>,
    /// Row sums of W: the button's (constant) counterfactual chance weight
    /// per hand.
    w_row: DVector<f32>,
    /// Button's payoff when open-folding: -sb - ante.
    p_fold: f32,
    /// Button's payoff when the big blind folds to the push: 1 + ante.
    p_steal: f32,
}

/// Regret matching for a two-action infoset: probability of the first action.
/// Inputs are nonnegative (CFR+ clamps at zero); uniform fallback when the
/// infoset has accumulated no positive regret yet.
#[inline]
fn regret_match_binary(r_first: f32, r_second: f32) -> f32 {
    let total = r_first + r_second;
    if total > 0.0 { r_first / total } else { 0.5 }
}

fn validate(stack: f32, sb: f32, ante: f32, iterations: u32) -> Result<(), SolverError> {
    if ![stack, sb, ante].iter().all(|v| v.is_finite()) {
        return Err(SolverError::NonFiniteInput);
    }
    if stack <= 0.0 {
        return Err(SolverError::StackNotPositive);
    }
    if sb < 0.0 || ante < 0.0 {
        return Err(SolverError::NegativeBlindOrAnte);
    }
    if sb > stack {
        return Err(SolverError::SmallBlindExceedsStack);
    }
    if iterations == 0 {
        return Err(SolverError::ZeroIterations);
    }
    Ok(())
}

impl PushFoldSolver {
    /// Initializes the environment. The equity and matchup tables are
    /// constant for the lifetime of the solver; everything stake-dependent
    /// is rebuilt per solve.
    pub fn new() -> Self {
        let equity_table = equities();
        let matchup_table = matchups();
        let n = matchup_table.nrows();
        PushFoldSolver {
            equity_table,
            matchup_table,
            b: DMatrix::zeros(n, n),
            w_row: DVector::zeros(n),
            p_fold: 0.0,
            p_steal: 0.0,
        }
    }

    /// Builds the stake-dependent contraction matrix and payoff constants.
    fn setup(&mut self, stack: f32, sb: f32, ante: f32) {
        self.p_fold = -sb - ante;
        self.p_steal = 1.0 + ante;

        // Call payout: win or lose (stack + ante) scaled by equity edge.
        let all_in = stack + ante;
        self.b = self
            .matchup_table
            .zip_map(&self.equity_table, |w, e| w * (2.0 * e - 1.0) * all_in);

        self.w_row = DVector::from_fn(self.matchup_table.nrows(), |i, _| {
            self.matchup_table.row(i).sum()
        });
    }

    /// Runs CFR+ (regret clamping, alternating updates, linear averaging)
    /// and returns the averaged strategies with their exploitability.
    pub fn solve(
        &mut self,
        stack: f32,
        sb: f32,
        ante: f32,
        iterations: u32,
    ) -> Result<Strategies, SolverError> {
        validate(stack, sb, ante, iterations)?;
        self.setup(stack, sb, ante);

        let n = self.matchup_table.nrows();

        // Cumulative clamped regrets, one pair per player.
        let mut r_bu_push = DVector::<f32>::zeros(n);
        let mut r_bu_fold = DVector::<f32>::zeros(n);
        let mut r_bb_call = DVector::<f32>::zeros(n);
        let mut r_bb_fold = DVector::<f32>::zeros(n);

        // Current strategies (probability of push / call).
        let mut sigma_bu = DVector::from_element(n, 0.5_f32);
        let mut sigma_bb = DVector::from_element(n, 0.5_f32);

        // Linearly weighted average-strategy accumulators. Both players'
        // own reach is identically 1 in this game (each has a single
        // decision point with no prior own actions), so no reach factor.
        let mut avg_bu = DVector::<f32>::zeros(n);
        let mut avg_bb = DVector::<f32>::zeros(n);
        let mut weight_sum = 0.0_f32;

        for t in 1..=iterations {
            // --- Button update (counterfactual weight: chance only) ---
            let u = &self.matchup_table * &sigma_bb; // W * sigma_bb
            let v = &self.b * &sigma_bb; //             B * sigma_bb
            for i in 0..n {
                // Unnormalized counterfactual action values.
                let v_push = v[i] + self.p_steal * (self.w_row[i] - u[i]);
                let v_fold = self.p_fold * self.w_row[i];
                let delta = v_push - v_fold;
                // v(a) - v(node) factored: node value mixes the two actions.
                r_bu_push[i] = (r_bu_push[i] + (1.0 - sigma_bu[i]) * delta).max(0.0);
                r_bu_fold[i] = (r_bu_fold[i] - sigma_bu[i] * delta).max(0.0);
                sigma_bu[i] = regret_match_binary(r_bu_push[i], r_bu_fold[i]);
            }

            // --- Big blind update, against the button's *new* strategy ---
            // tr_mul computes A^T * x without materializing the transpose.
            let r = self.matchup_table.tr_mul(&sigma_bu); // counterfactual reach
            let q = self.b.tr_mul(&sigma_bu); //             call-payoff mass
            for j in 0..n {
                // Big blind payoffs are the negation of the button's:
                // v_call = -q[j], v_fold = -p_steal * r[j].
                let delta = self.p_steal * r[j] - q[j];
                r_bb_call[j] = (r_bb_call[j] + (1.0 - sigma_bb[j]) * delta).max(0.0);
                r_bb_fold[j] = (r_bb_fold[j] - sigma_bb[j] * delta).max(0.0);
                sigma_bb[j] = regret_match_binary(r_bb_call[j], r_bb_fold[j]);
            }

            // --- Linearly weighted strategy averaging (CFR+) ---
            let w = t as f32;
            avg_bu.axpy(w, &sigma_bu, 1.0);
            avg_bb.axpy(w, &sigma_bb, 1.0);
            weight_sum += w;
        }

        avg_bu /= weight_sum;
        avg_bb /= weight_sum;
        let exploitability = self.exploitability(&avg_bu, &avg_bb);

        Ok(Strategies {
            bu_push: avg_bu,
            bb_call: avg_bb,
            exploitability,
        })
    }

    /// Nash gap of a strategy pair: the sum of both players' best-response
    /// improvements, normalized per deal. Zero exactly at equilibrium.
    /// Requires `setup` to have run (called internally by `solve`).
    pub fn exploitability(&self, sigma_bu: &DVector<f32>, sigma_bb: &DVector<f32>) -> f32 {
        let n = self.matchup_table.nrows();
        let total_mass: f32 = self.w_row.sum();

        // Button best response vs sigma_bb.
        let u = &self.matchup_table * sigma_bb;
        let v = &self.b * sigma_bb;
        let mut br_bu = 0.0_f32;
        for i in 0..n {
            let v_push = v[i] + self.p_steal * (self.w_row[i] - u[i]);
            let v_fold = self.p_fold * self.w_row[i];
            br_bu += v_push.max(v_fold);
        }

        // Big blind best response vs sigma_bu. The push branch is where the
        // big blind has a choice; the button-folds branch contributes a
        // fixed -p_fold per unit of fold mass (the big blind collects what
        // the button surrenders) and must be included for the gap to be a
        // true game-value difference.
        let r = self.matchup_table.tr_mul(sigma_bu);
        let q = self.b.tr_mul(sigma_bu);
        let mut br_bb = 0.0_f32;
        for j in 0..n {
            let v_call = -q[j];
            let v_fold = -self.p_steal * r[j];
            br_bb += v_call.max(v_fold);
        }
        let fold_mass = total_mass - r.sum();
        br_bb += -self.p_fold * fold_mass;

        (br_bu + br_bb) / total_mass
    }
}

impl Default for PushFoldSolver {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::constants::{N_RANKS, hands};

    const STACK: f32 = 5.0;
    const SB: f32 = 0.5;
    const ANTE: f32 = 0.125;

    /// Renders a strategy vector as the 13x13 hand grid, one cell per infoset
    /// showing the label and its action frequency as a percentage. The layout
    /// matches a standard range chart: pairs on the diagonal, suited hands in
    /// the upper triangle, offsuit in the lower.
    fn format_grid(strat: &DVector<f32>) -> String {
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

    /// Solves at the shared stakes and prints both converged ranges. Run with
    /// `cargo test -p pushfold print_strategies -- --nocapture` to see output.
    #[test]
    fn print_strategies() {
        let mut solver = PushFoldSolver::new();
        let out = solver.solve(STACK, SB, ANTE, 1000).unwrap();
        println!(
            "\nConverged strategies at stack={STACK}bb sb={SB} ante={ANTE} \
             (exploitability {:.2e})",
            out.exploitability
        );
        println!("\nButton push range:\n{}", format_grid(&out.bu_push));
        println!("Big blind call range:\n{}", format_grid(&out.bb_call));
    }

    #[test]
    fn test_validate() {
        use SolverError::*;
        assert!(validate(5.0, 0.5, 0.125, 200).is_ok());
        assert!(validate(1.0, 0.5, 0.0, 1).is_ok());
        assert_eq!(validate(f32::NAN, 0.5, 0.125, 200), Err(NonFiniteInput));
        assert_eq!(validate(0.0, 0.5, 0.125, 200), Err(StackNotPositive));
        assert_eq!(validate(5.0, -0.5, 0.125, 200), Err(NegativeBlindOrAnte));
        assert_eq!(validate(5.0, 6.0, 0.125, 200), Err(SmallBlindExceedsStack));
        assert_eq!(validate(5.0, 0.5, 0.125, 0), Err(ZeroIterations));
    }

    #[test]
    fn test_strategies_are_probabilities() {
        let mut solver = PushFoldSolver::new();
        let out = solver.solve(STACK, SB, ANTE, 200).unwrap();
        for x in out.bu_push.iter().chain(out.bb_call.iter()) {
            assert!((0.0..=1.0).contains(x), "strategy out of range: {x}");
        }
    }

    #[test]
    fn test_premium_hands_are_pure() {
        // AA is infoset 0 in the row-major 13x13 grid. At 5bb it is a pure
        // push and a pure call; with no epsilon floor the average should be
        // exactly (or extremely near) 1.
        let mut solver = PushFoldSolver::new();
        let out = solver.solve(STACK, SB, ANTE, 1000).unwrap();
        assert!(out.bu_push[0] > 0.999, "AA push freq {}", out.bu_push[0]);
        assert!(out.bb_call[0] > 0.999, "AA call freq {}", out.bb_call[0]);
    }

    #[test]
    fn test_converges() {
        let mut solver = PushFoldSolver::new();
        let coarse = solver.solve(STACK, SB, ANTE, 100).unwrap();
        let fine = solver.solve(STACK, SB, ANTE, 2000).unwrap();
        assert!(fine.exploitability >= 0.0);
        assert!(
            fine.exploitability < coarse.exploitability,
            "gap did not shrink: {} -> {}",
            coarse.exploitability,
            fine.exploitability
        );
        assert!(
            fine.exploitability < 1e-3,
            "gap too large: {}",
            fine.exploitability
        );
    }
}
