use crate::constants::*;
use nalgebra::{DMatrix, DVector};
use wasm_bindgen::prelude::*;

// The shared error type (see pushfold_shared for why it is not a
// `#[wasm_bindgen]` enum); re-exported so this crate's public API keeps a
// local path to its error type. This game never emits `BigBlindExceedsStack`.
pub use pushfold_shared::SolverError;

/// Solver result
#[wasm_bindgen]
pub struct Strategies {
    #[wasm_bindgen(skip)]
    pub bu_push: DVector<f32>,
    #[wasm_bindgen(skip)]
    pub bb_call: DVector<f32>,
    #[wasm_bindgen(readonly)]
    pub exploitability: f32,
}

#[wasm_bindgen]
impl Strategies {
    #[wasm_bindgen(getter)]
    pub fn bu_push(&self) -> Vec<f32> {
        self.bu_push.as_slice().to_vec()
    }

    #[wasm_bindgen(getter)]
    pub fn bb_call(&self) -> Vec<f32> {
        self.bb_call.as_slice().to_vec()
    }
}

#[wasm_bindgen]
pub struct HeadsUpSolver {
    equity_table: DMatrix<f32>,
    matchup_table: DMatrix<f32>,
    w_row: DVector<f32>,

    p_fold: f32,
    p_steal: f32,
    p_call: DMatrix<f32>,

    // Cumulative clamped regrets, one pair per player.
    r_bu_push: DVector<f32>,
    r_bu_fold: DVector<f32>,
    r_bb_call: DVector<f32>,
    r_bb_fold: DVector<f32>,

    // Current strategies (probability of push / call).
    sigma_bu: DVector<f32>,
    sigma_bb: DVector<f32>,

    // Linearly weighted average-strategy accumulators. Both players'
    // own reach is identically 1 in this game (each has a single
    // decision point with no prior own actions), so no reach factor.
    avg_bu: DVector<f32>,
    avg_bb: DVector<f32>,
    weight_sum: f32,

    // Workspace for the per-iteration matrix-vector products; each is
    // fully overwritten (gemv with beta = 0) before it is read.
    u: DVector<f32>,
    v: DVector<f32>,
    r: DVector<f32>,
    q: DVector<f32>,
}

/// Given clamped non-negative CFR+ regrets, computes frequency of first action
/// Defaults to uniform across actions
#[inline]
fn regret_match_binary(r_first: f32, r_second: f32) -> f32 {
    let total = r_first + r_second;
    if total > 0.0 { r_first / total } else { 0.5 }
}

fn validate_inputs(stack: f32, sb: f32, ante: f32, iterations: u32) -> Result<(), SolverError> {
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

#[wasm_bindgen]
impl HeadsUpSolver {
    #[wasm_bindgen(constructor)]
    pub fn new() -> Self {
        let equity_table = equities();
        let matchup_table = matchups();
        // Total combos of BU hands
        let w_row = DVector::from_fn(matchup_table.nrows(), |i, _| matchup_table.row(i).sum());

        HeadsUpSolver {
            equity_table,
            matchup_table,
            w_row,

            p_fold: 0.0,
            p_steal: 0.0,
            p_call: DMatrix::<f32>::zeros(N_INFOSETS, N_INFOSETS),

            r_bu_push: DVector::zeros(N_INFOSETS),
            r_bu_fold: DVector::zeros(N_INFOSETS),
            r_bb_call: DVector::zeros(N_INFOSETS),
            r_bb_fold: DVector::zeros(N_INFOSETS),

            sigma_bu: DVector::zeros(N_INFOSETS),
            sigma_bb: DVector::zeros(N_INFOSETS),

            avg_bu: DVector::zeros(N_INFOSETS),
            avg_bb: DVector::zeros(N_INFOSETS),
            weight_sum: 0.0,

            u: DVector::zeros(N_INFOSETS),
            v: DVector::zeros(N_INFOSETS),
            r: DVector::zeros(N_INFOSETS),
            q: DVector::zeros(N_INFOSETS),
        }
    }

    /// Runs CFR+ (regret clamping, alternating updates, linear averaging)
    /// and returns the averaged strategies with their exploitability.
    ///
    /// Thin boundary wrapper: rejected inputs surface to JS as an `Error`
    /// whose message is the `SolverError` `Display` string. The typed result
    /// lives in `solve_inner`, which tests exercise natively (a `JsError` can
    /// neither be constructed nor `Debug`-printed off-wasm).
    pub fn solve(
        &mut self,
        stack: f32,
        sb: f32,
        ante: f32,
        iterations: u32,
    ) -> Result<Strategies, JsError> {
        Ok(self.solve_inner(stack, sb, ante, iterations)?)
    }
}

impl HeadsUpSolver {
    /// `solve` with the typed error; see the wasm wrapper above.
    fn solve_inner(
        &mut self,
        stack: f32,
        sb: f32,
        ante: f32,
        iterations: u32,
    ) -> Result<Strategies, SolverError> {
        validate_inputs(stack, sb, ante, iterations)?;
        self.setup(stack, sb, ante);

        for t in 1..=iterations {
            self.cfr_iterate(t);
        }

        let bu_push = &self.avg_bu / self.weight_sum;
        let bb_call = &self.avg_bb / self.weight_sum;
        let exploitability = self.exploitability(&bu_push, &bb_call);

        Ok(Strategies {
            bu_push,
            bb_call,
            exploitability,
        })
    }
}

impl HeadsUpSolver {
    /// Builds the stake-dependent contraction matrix and payoff constants,
    /// and resets the CFR state so each solve starts fresh.
    ///
    /// Positive payoff is for BU; negative payoff is for BB
    fn setup(&mut self, stack: f32, sb: f32, ante: f32) {
        self.p_fold = -sb - ante;
        self.p_steal = 1.0 + ante;

        // Call payout: win or lose (stack + ante) scaled by equity edge.
        let all_in = stack + ante;
        self.p_call
            .iter_mut()
            .zip(self.matchup_table.iter())
            .zip(self.equity_table.iter())
            .for_each(|((p, &w), &e)| *p = w * (2.0 * e - 1.0) * all_in);

        self.r_bu_push.fill(0.0);
        self.r_bu_fold.fill(0.0);
        self.r_bb_call.fill(0.0);
        self.r_bb_fold.fill(0.0);
        self.sigma_bu.fill(0.5);
        self.sigma_bb.fill(0.5);
        self.avg_bu.fill(0.0);
        self.avg_bb.fill(0.0);
        self.weight_sum = 0.0;
    }

    /// One CFR+ iteration: alternating regret updates, then averaging.
    fn cfr_iterate(&mut self, t: u32) {
        self.update_bu();
        self.update_bb();
        self.update_average(t);
    }

    /// Button regret update (counterfactual weight: chance only).
    fn update_bu(&mut self) {
        self.u.gemv(1.0, &self.matchup_table, &self.sigma_bb, 0.0); // W * sigma_bb
        self.v.gemv(1.0, &self.p_call, &self.sigma_bb, 0.0); //      B * sigma_bb
        for i in 0..N_INFOSETS {
            // Unnormalized counterfactual action values.
            let v_push = self.v[i] + self.p_steal * (self.w_row[i] - self.u[i]);
            let v_fold = self.p_fold * self.w_row[i];
            let delta = v_push - v_fold;
            // v(a) - v(node) factored: node value mixes the two actions.
            self.r_bu_push[i] = (self.r_bu_push[i] + (1.0 - self.sigma_bu[i]) * delta).max(0.0);
            self.r_bu_fold[i] = (self.r_bu_fold[i] - self.sigma_bu[i] * delta).max(0.0);
            self.sigma_bu[i] = regret_match_binary(self.r_bu_push[i], self.r_bu_fold[i]);
        }
    }

    /// Big blind regret update, against the button's *new* strategy.
    fn update_bb(&mut self) {
        // gemv_tr computes A^T * x without materializing the transpose.
        self.r
            .gemv_tr(1.0, &self.matchup_table, &self.sigma_bu, 0.0); // counterfactual reach
        self.q.gemv_tr(1.0, &self.p_call, &self.sigma_bu, 0.0); //             call-payoff mass
        for j in 0..N_INFOSETS {
            // Big blind payoffs are the negation of the button's:
            // v_call = -q[j], v_fold = -p_steal * r[j].
            let delta = self.p_steal * self.r[j] - self.q[j];
            self.r_bb_call[j] = (self.r_bb_call[j] + (1.0 - self.sigma_bb[j]) * delta).max(0.0);
            self.r_bb_fold[j] = (self.r_bb_fold[j] - self.sigma_bb[j] * delta).max(0.0);
            self.sigma_bb[j] = regret_match_binary(self.r_bb_call[j], self.r_bb_fold[j]);
        }
    }

    /// Linearly weighted strategy averaging (CFR+).
    fn update_average(&mut self, t: u32) {
        let w = t as f32;
        self.avg_bu.axpy(w, &self.sigma_bu, 1.0);
        self.avg_bb.axpy(w, &self.sigma_bb, 1.0);
        self.weight_sum += w;
    }

    /// Nash gap of a strategy pair: the sum of both players' best-response
    /// improvements, normalized per deal. Zero exactly at equilibrium.
    fn exploitability(&self, sigma_bu: &DVector<f32>, sigma_bb: &DVector<f32>) -> f32 {
        let total_mass: f32 = self.w_row.sum();

        // Button best response vs sigma_bb.
        let u = &self.matchup_table * sigma_bb;
        let v = &self.p_call * sigma_bb;
        let mut br_bu = 0.0_f32;
        for i in 0..N_INFOSETS {
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
        let q = self.p_call.tr_mul(sigma_bu);
        let mut br_bb = 0.0_f32;
        for j in 0..N_INFOSETS {
            let v_call = -q[j];
            let v_fold = -self.p_steal * r[j];
            br_bb += v_call.max(v_fold);
        }
        let fold_mass = total_mass - r.sum();
        br_bb += -self.p_fold * fold_mass;

        (br_bu + br_bb) / total_mass
    }
}

impl Default for HeadsUpSolver {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use pushfold_shared::format_grid;

    const STACK: f32 = 5.0;
    const SB: f32 = 0.5;
    const ANTE: f32 = 0.125;

    /// Solves at the shared stakes and prints both converged ranges. Run with
    /// `cargo test -p pushfold print_strategies -- --nocapture` to see output.
    #[test]
    fn print_strategies() {
        let mut solver = HeadsUpSolver::new();
        let out = solver.solve_inner(STACK, SB, ANTE, 1000).unwrap();
        println!(
            "\nConverged strategies at stack={STACK}bb sb={SB} ante={ANTE} \
             (exploitability {:.2e})",
            out.exploitability
        );
        println!(
            "\nButton push range:\n{}",
            format_grid(out.bu_push.as_slice())
        );
        println!(
            "Big blind call range:\n{}",
            format_grid(out.bb_call.as_slice())
        );
    }

    #[test]
    fn test_validate() {
        use SolverError::*;
        assert!(validate_inputs(5.0, 0.5, 0.125, 200).is_ok());
        assert!(validate_inputs(1.0, 0.5, 0.0, 1).is_ok());
        assert_eq!(
            validate_inputs(f32::NAN, 0.5, 0.125, 200),
            Err(NonFiniteInput)
        );
        assert_eq!(validate_inputs(0.0, 0.5, 0.125, 200), Err(StackNotPositive));
        assert_eq!(
            validate_inputs(5.0, -0.5, 0.125, 200),
            Err(NegativeBlindOrAnte)
        );
        assert_eq!(
            validate_inputs(5.0, 6.0, 0.125, 200),
            Err(SmallBlindExceedsStack)
        );
        assert_eq!(validate_inputs(5.0, 0.5, 0.125, 0), Err(ZeroIterations));
    }

    #[test]
    fn test_strategies_are_probabilities() {
        let mut solver = HeadsUpSolver::new();
        let out = solver.solve_inner(STACK, SB, ANTE, 200).unwrap();
        for x in out.bu_push.iter().chain(out.bb_call.iter()) {
            assert!((0.0..=1.0).contains(x), "strategy out of range: {x}");
        }
    }

    #[test]
    fn test_premium_hands_are_pure() {
        // AA is infoset 0 in the row-major 13x13 grid. At 5bb it is a pure
        // push and a pure call; with no epsilon floor the average should be
        // exactly (or extremely near) 1.
        let mut solver = HeadsUpSolver::new();
        let out = solver.solve_inner(STACK, SB, ANTE, 1000).unwrap();
        assert!(out.bu_push[0] > 0.999, "AA push freq {}", out.bu_push[0]);
        assert!(out.bb_call[0] > 0.999, "AA call freq {}", out.bb_call[0]);
    }

    #[test]
    fn test_converges() {
        let mut solver = HeadsUpSolver::new();
        let coarse = solver.solve_inner(STACK, SB, ANTE, 100).unwrap();
        let fine = solver.solve_inner(STACK, SB, ANTE, 2000).unwrap();
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
