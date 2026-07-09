use crate::constants::*;
use ndarray::{Array1, Array2, Array3, Axis};
use wasm_bindgen::prelude::*;

/// Player seat indices, used to key per-player stakes (stacks, three-way
/// contributions, the side-pot pair).
const BU: usize = 0;
const SB: usize = 1;
const BB: usize = 2;

/// Input validation errors
#[wasm_bindgen]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SolverError {
    NonFiniteInput,
    StackNotPositive,
    NegativeBlindOrAnte,
    SmallBlindExceedsStack,
    BigBlindExceedsStack,
    ZeroIterations,
}

/// Solver result: the six averaged strategies, one per decision point.
#[wasm_bindgen]
pub struct Strategies {
    /// Button open-pushes.
    #[wasm_bindgen(skip)]
    pub bu_push: Array1<f32>,
    /// Small blind calls a button push.
    #[wasm_bindgen(skip)]
    pub sb_call: Array1<f32>,
    /// Small blind open-pushes after a button fold.
    #[wasm_bindgen(skip)]
    pub sb_push: Array1<f32>,
    /// Big blind calls facing a button push and a small blind call.
    #[wasm_bindgen(skip)]
    pub bb_call_both: Array1<f32>,
    /// Big blind calls a button push after the small blind folds.
    #[wasm_bindgen(skip)]
    pub bb_call_bu: Array1<f32>,
    /// Big blind calls a small blind push after the button folds.
    #[wasm_bindgen(skip)]
    pub bb_call_sb: Array1<f32>,
    #[wasm_bindgen(readonly)]
    pub exploitability: f32,
}

#[wasm_bindgen]
impl Strategies {
    #[wasm_bindgen(getter)]
    pub fn bu_push(&self) -> Vec<f32> {
        self.bu_push.to_vec()
    }

    #[wasm_bindgen(getter)]
    pub fn sb_call(&self) -> Vec<f32> {
        self.sb_call.to_vec()
    }

    #[wasm_bindgen(getter)]
    pub fn sb_push(&self) -> Vec<f32> {
        self.sb_push.to_vec()
    }

    #[wasm_bindgen(getter)]
    pub fn bb_call_both(&self) -> Vec<f32> {
        self.bb_call_both.to_vec()
    }

    #[wasm_bindgen(getter)]
    pub fn bb_call_bu(&self) -> Vec<f32> {
        self.bb_call_bu.to_vec()
    }

    #[wasm_bindgen(getter)]
    pub fn bb_call_sb(&self) -> Vec<f32> {
        self.bb_call_sb.to_vec()
    }
}

/// Stake-dependent payoff constants, rebuilt by `setup` for each solve.
///
/// Chip deltas are measured from each player's starting stack; every terminal
/// sums to zero across the three players. Stacks are push amounts on top of
/// the ante (the heads-up crate's convention: an all-in commits stack + ante).
///
/// Heads-up showdowns (one player folded, or the side pot) risk the smaller
/// of the two live stacks `e`, so the winner takes a pot of `2e`, the antes,
/// and any dead blinds, at a cost of `e + ante`; `p_xy`/`c_xy` capture that
/// pot and cost for each pair. A three-way all-in splits into a main pot
/// (three times the shortest stack, plus antes) shared by all three per the
/// three-way equities, and a side pot (twice the middle-vs-shortest stack
/// difference) contested by the two deeper stacks per heads-up equity.
#[derive(Default)]
struct Stakes {
    /// Button wins both blinds and antes when everyone folds.
    steal_bu: f32,
    /// Small blind wins the big blind and both remaining antes on a steal.
    steal_sb: f32,
    /// Button folds its ante.
    fold_bu: f32,
    /// Small blind folds its blind and ante.
    fold_sb: f32,
    /// Big blind folds its blind and ante.
    fold_bb: f32,
    /// Heads-up pot and per-contender cost: button vs small blind (big
    /// blind's dead blind included), ...
    p01: f32,
    c01: f32,
    /// ... button vs big blind (small blind's dead blind included), ...
    p02: f32,
    c02: f32,
    /// ... and small blind vs big blind (only the button's dead ante).
    p12: f32,
    c12: f32,
    /// Three-way main pot: three times the shortest stack plus all antes.
    main: f32,
    /// Three-way side pot between the two deeper stacks.
    side: f32,
    /// Per-player three-way contribution: own stack capped at the middle
    /// stack, plus the ante.
    cost3: [f32; 3],
    /// The two players (ascending seat index) contesting the side pot.
    side_pair: [usize; 2],
}

/// Given clamped non-negative CFR+ regrets, computes frequency of first action
/// Defaults to uniform across actions
#[inline]
fn regret_match_binary(r_first: f32, r_second: f32) -> f32 {
    let total = r_first + r_second;
    if total > 0.0 { r_first / total } else { 0.5 }
}

/// One binary decision point's CFR+ state: cumulative clamped regrets for the
/// aggressive action (push/call) and for folding, the current strategy, and
/// the linearly weighted average-strategy accumulator. Every player's reach
/// at their own decision is identically 1 (nobody acts twice on a path), so
/// the average needs no reach factor.
struct Decision {
    r_act: Array1<f32>,
    r_fold: Array1<f32>,
    sigma: Array1<f32>,
    avg: Array1<f32>,
}

impl Decision {
    fn new() -> Self {
        Decision {
            r_act: Array1::zeros(N_INFOSETS),
            r_fold: Array1::zeros(N_INFOSETS),
            sigma: Array1::from_elem(N_INFOSETS, 0.5),
            avg: Array1::zeros(N_INFOSETS),
        }
    }

    fn reset(&mut self) {
        self.r_act.fill(0.0);
        self.r_fold.fill(0.0);
        self.sigma.fill(0.5);
        self.avg.fill(0.0);
    }

    /// CFR+ update from the two actions' unnormalized counterfactual values:
    /// clamped regret accumulation, then regret matching.
    fn update(&mut self, v_act: &Array1<f32>, v_fold: &Array1<f32>) {
        for i in 0..N_INFOSETS {
            // v(a) - v(node) factored: node value mixes the two actions.
            let delta = v_act[i] - v_fold[i];
            self.r_act[i] = (self.r_act[i] + (1.0 - self.sigma[i]) * delta).max(0.0);
            self.r_fold[i] = (self.r_fold[i] - self.sigma[i] * delta).max(0.0);
            self.sigma[i] = regret_match_binary(self.r_act[i], self.r_fold[i]);
        }
    }

    /// Linearly weighted strategy averaging (CFR+).
    fn accumulate(&mut self, weight: f32) {
        self.avg.scaled_add(weight, &self.sigma);
    }

    fn average(&self, weight_sum: f32) -> Array1<f32> {
        &self.avg / weight_sum
    }
}

/// Contracts a tensor's last axis with a strategy vector:
/// `result[x, y] = sum_z t[x, y, z] * v[z]`, as one `(N^2 x N)` mat-vec.
///
/// This is the solver's only O(N^3) operation. The matchup tensor is fully
/// symmetric, so a last-axis contraction serves any player: `result[own,
/// other]` sums over whichever third player `v` describes.
fn contract_last(t: &Array3<f32>, v: &Array1<f32>) -> Array2<f32> {
    t.view()
        .into_shape_with_order((N_INFOSETS * N_INFOSETS, N_INFOSETS))
        .expect("tensor is standard layout")
        .dot(v)
        .into_shape_with_order((N_INFOSETS, N_INFOSETS))
        .expect("result has N * N entries")
}

fn validate_inputs(
    stacks: [f32; 3],
    sb: f32,
    ante: f32,
    iterations: u32,
) -> Result<(), SolverError> {
    if !stacks.iter().chain([&sb, &ante]).all(|v| v.is_finite()) {
        return Err(SolverError::NonFiniteInput);
    }
    if stacks.iter().any(|&s| s <= 0.0) {
        return Err(SolverError::StackNotPositive);
    }
    if sb < 0.0 || ante < 0.0 {
        return Err(SolverError::NegativeBlindOrAnte);
    }
    if sb > stacks[SB] {
        return Err(SolverError::SmallBlindExceedsStack);
    }
    if stacks[BB] < 1.0 {
        return Err(SolverError::BigBlindExceedsStack);
    }
    if iterations == 0 {
        return Err(SolverError::ZeroIterations);
    }
    Ok(())
}

/// Three-handed push/fold solver.
///
/// The game: the button open-pushes or folds; the small blind reacts (call a
/// push, or open-push once the button folds); the big blind closes the action
/// at one of three histories. Two-player showdowns and the side pot of a
/// three-way all-in are priced with the heads-up equity table (the third
/// player's card removal is ignored); a three-way all-in's main pot uses the
/// generated three-way pot shares.
///
/// Runs CFR+ exactly like the heads-up solver — clamped regrets, alternating
/// updates (BU, then SB, then BB, each against the freshest strategies),
/// linear averaging. With three players CFR carries no convergence guarantee
/// to a Nash equilibrium, so `solve` reports NashConv (the summed
/// best-response gaps) as `exploitability`: a small value still certifies the
/// profile is hard to exploit.
#[wasm_bindgen]
pub struct ThreewaySolver {
    /// Symmetric deal-count tensor; `[x, y, z]` weighs ordered infosets.
    w: Array3<f32>,
    /// Deal counts times three-way pot share of the first axis's infoset;
    /// symmetric in the trailing (opponent) axes.
    we: Array3<f32>,
    /// Heads-up equity, `[own, opp]`.
    eq_hu: Array2<f32>,
    /// `w` contracted over one opponent: `w_pair[x, y] = sum_z w[x, y, z]`.
    w_pair: Array2<f32>,
    /// `w` contracted over both opponents: total deals containing infoset `x`.
    w_own: Array1<f32>,

    stakes: Stakes,

    bu: Decision,
    sb_call: Decision,
    sb_push: Decision,
    bb_both: Decision,
    bb_bu: Decision,
    bb_sb: Decision,
    weight_sum: f32,
}

#[wasm_bindgen]
impl ThreewaySolver {
    #[wasm_bindgen(constructor)]
    pub fn new() -> Self {
        let w = matchups();
        let we = &w * &equities();
        let w_pair = w.sum_axis(Axis(2));
        let w_own = w_pair.sum_axis(Axis(1));

        ThreewaySolver {
            w,
            we,
            eq_hu: headsup_equities(),
            w_pair,
            w_own,

            stakes: Stakes::default(),

            bu: Decision::new(),
            sb_call: Decision::new(),
            sb_push: Decision::new(),
            bb_both: Decision::new(),
            bb_bu: Decision::new(),
            bb_sb: Decision::new(),
            weight_sum: 0.0,
        }
    }

    /// Runs CFR+ (regret clamping, alternating updates, linear averaging)
    /// and returns the averaged strategies with their NashConv gap.
    pub fn solve(
        &mut self,
        stack_bu: f32,
        stack_sb: f32,
        stack_bb: f32,
        sb: f32,
        ante: f32,
        iterations: u32,
    ) -> Result<Strategies, SolverError> {
        let stacks = [stack_bu, stack_sb, stack_bb];
        validate_inputs(stacks, sb, ante, iterations)?;
        self.setup(stacks, sb, ante);

        for t in 1..=iterations {
            self.cfr_iterate(t);
        }

        let bu_push = self.bu.average(self.weight_sum);
        let sb_call = self.sb_call.average(self.weight_sum);
        let sb_push = self.sb_push.average(self.weight_sum);
        let bb_call_both = self.bb_both.average(self.weight_sum);
        let bb_call_bu = self.bb_bu.average(self.weight_sum);
        let bb_call_sb = self.bb_sb.average(self.weight_sum);
        let exploitability = self.exploitability(
            &bu_push,
            &sb_call,
            &sb_push,
            &bb_call_both,
            &bb_call_bu,
            &bb_call_sb,
        );

        Ok(Strategies {
            bu_push,
            sb_call,
            sb_push,
            bb_call_both,
            bb_call_bu,
            bb_call_sb,
            exploitability,
        })
    }
}

impl ThreewaySolver {
    /// Builds the stake-dependent payoff constants and resets the CFR state
    /// so each solve starts fresh.
    fn setup(&mut self, stacks: [f32; 3], sb: f32, ante: f32) {
        let e01 = stacks[BU].min(stacks[SB]);
        let e02 = stacks[BU].min(stacks[BB]);
        let e12 = stacks[SB].min(stacks[BB]);

        // Seats ordered by stack depth fix the three-way pot structure: the
        // shortest stack caps the main pot, the middle stack caps everyone's
        // contribution, and the two deepest contest the side pot.
        let mut by_depth = [BU, SB, BB];
        by_depth.sort_by(|&x, &y| stacks[x].total_cmp(&stacks[y]));
        let shortest = stacks[by_depth[0]];
        let middle = stacks[by_depth[1]];
        let mut side_pair = [by_depth[1], by_depth[2]];
        side_pair.sort();

        self.stakes = Stakes {
            steal_bu: sb + 1.0 + 2.0 * ante,
            steal_sb: 1.0 + 2.0 * ante,
            fold_bu: -ante,
            fold_sb: -sb - ante,
            fold_bb: -1.0 - ante,
            p01: 2.0 * e01 + 3.0 * ante + 1.0,
            c01: e01 + ante,
            p02: 2.0 * e02 + 3.0 * ante + sb,
            c02: e02 + ante,
            p12: 2.0 * e12 + 3.0 * ante,
            c12: e12 + ante,
            main: 3.0 * shortest + 3.0 * ante,
            side: 2.0 * (middle - shortest),
            cost3: stacks.map(|s| s.min(middle) + ante),
            side_pair,
        };

        self.bu.reset();
        self.sb_call.reset();
        self.sb_push.reset();
        self.bb_both.reset();
        self.bb_bu.reset();
        self.bb_sb.reset();
        self.weight_sum = 0.0;
    }

    /// One CFR+ iteration: alternating regret updates (each player reacting
    /// to the freshest opponent strategies), then averaging.
    ///
    /// All values reduce to six last-axis contractions of the two tensors
    /// with current strategy vectors; everything else is O(N^2). `dd3` and
    /// `wed3` are shared between the button and small blind updates, and `db`
    /// between the small blind and big blind updates.
    fn cfr_iterate(&mut self, t: u32) {
        let dc = contract_last(&self.w, &self.sb_call.sigma);
        let dd3 = contract_last(&self.w, &self.bb_both.sigma);
        let wed3 = contract_last(&self.we, &self.bb_both.sigma);
        let (v_push, v_fold) = self.bu_values(
            &self.sb_call.sigma,
            &self.bb_bu.sigma,
            &self.bb_both.sigma,
            &dc,
            &dd3,
            &wed3,
        );
        self.bu.update(&v_push, &v_fold);

        let db = contract_last(&self.w, &self.bu.sigma);
        let (v_call, v_fold_call, v_push, v_fold_push) = self.sb_values(
            &self.bu.sigma,
            &self.bb_sb.sigma,
            &self.bb_both.sigma,
            &db,
            &dd3,
            &wed3,
        );
        self.sb_call.update(&v_call, &v_fold_call);
        self.sb_push.update(&v_push, &v_fold_push);

        let dc = contract_last(&self.w, &self.sb_call.sigma);
        let wec = contract_last(&self.we, &self.sb_call.sigma);
        let (v_both, v_fold_both, v_bu, v_fold_bu, v_sb, v_fold_sb) = self.bb_values(
            &self.bu.sigma,
            &self.sb_call.sigma,
            &self.sb_push.sigma,
            &dc,
            &db,
            &wec,
        );
        self.bb_both.update(&v_both, &v_fold_both);
        self.bb_bu.update(&v_bu, &v_fold_bu);
        self.bb_sb.update(&v_sb, &v_fold_sb);

        let weight = t as f32;
        self.bu.accumulate(weight);
        self.sb_call.accumulate(weight);
        self.sb_push.accumulate(weight);
        self.bb_both.accumulate(weight);
        self.bb_bu.accumulate(weight);
        self.bb_sb.accumulate(weight);
        self.weight_sum += weight;
    }

    /// The three-way all-in value for the player on the given matrices' first
    /// axis: main pot share, plus the side pot when that player is in the
    /// side pair, minus the contribution.
    ///
    /// `weighted_we` and `weighted_w` are `we`/`w` contracted with one
    /// opponent's call vector; `other` is the remaining opponent's call
    /// vector. The side pot pits this player against one specific opponent,
    /// and its term needs `w` contracted with the *third* player's call
    /// vector, dotted through the heads-up equities with the side opponent's
    /// call vector — `sides` supplies that (weights, reach) pair for each
    /// candidate opponent, keyed by seat.
    fn allin_values(
        &self,
        seat: usize,
        weighted_we: &Array2<f32>,
        weighted_w: &Array2<f32>,
        other: &Array1<f32>,
        sides: [(usize, &Array2<f32>, &Array1<f32>); 2],
    ) -> Array1<f32> {
        let st = &self.stakes;
        let mut value = weighted_we.dot(other) * st.main - weighted_w.dot(other) * st.cost3[seat];
        if st.side > 0.0 && st.side_pair.contains(&seat) {
            let partner = if st.side_pair[0] == seat {
                st.side_pair[1]
            } else {
                st.side_pair[0]
            };
            let (_, weights, reach) = sides
                .into_iter()
                .find(|&(opp, _, _)| opp == partner)
                .expect("side pair member is one of the two opponents");
            value += &((&self.eq_hu * weights).dot(reach) * st.side);
        }
        value
    }

    /// Button counterfactual values (push, fold), weighted by chance and both
    /// opponents' strategies.
    fn bu_values(
        &self,
        sb_call: &Array1<f32>,
        bb_bu: &Array1<f32>,
        bb_both: &Array1<f32>,
        dc: &Array2<f32>,
        dd3: &Array2<f32>,
        wed3: &Array2<f32>,
    ) -> (Array1<f32>, Array1<f32>) {
        let st = &self.stakes;

        // SB folds: the BB folds (steal) or calls (heads-up BU vs BB).
        let sb_folds = &self.w_pair - dc; // [bu, bb], sum_j w * (1 - sb_call)
        let bb_folds = bb_bu.mapv(|x| 1.0 - x);
        let mut v_push = sb_folds.dot(&bb_folds) * st.steal_bu
            + ((&self.eq_hu * st.p02 - st.c02) * &sb_folds).dot(bb_bu);

        // SB calls, BB folds: heads-up BU vs SB over the BB's dead blind.
        let bb_folds_both = &self.w_pair - dd3; // [bu, sb]
        v_push += &((&self.eq_hu * st.p01 - st.c01) * &bb_folds_both).dot(sb_call);

        // SB calls, BB calls: three-way all-in.
        v_push += &self.allin_values(
            BU,
            wed3,
            dd3,
            sb_call,
            [(SB, dd3, sb_call), (BB, dc, bb_both)],
        );

        // Folding surrenders the ante on every deal containing this hand.
        let v_fold = &self.w_own * st.fold_bu;
        (v_push, v_fold)
    }

    /// Small blind counterfactual values for its two decision points: facing
    /// a button push (call, fold) and after a button fold (push, fold).
    fn sb_values(
        &self,
        bu_push: &Array1<f32>,
        bb_sb: &Array1<f32>,
        bb_both: &Array1<f32>,
        db: &Array2<f32>,
        dd3: &Array2<f32>,
        wed3: &Array2<f32>,
    ) -> (Array1<f32>, Array1<f32>, Array1<f32>, Array1<f32>) {
        let st = &self.stakes;

        // Call a push: the BB folds (heads-up SB vs BU) or calls (three-way).
        let bb_folds_both = &self.w_pair - dd3; // [sb, bu]
        let mut v_call = ((&self.eq_hu * st.p01 - st.c01) * &bb_folds_both).dot(bu_push);
        v_call += &self.allin_values(
            SB,
            wed3,
            dd3,
            bu_push,
            [(BU, dd3, bu_push), (BB, db, bb_both)],
        );

        // Fold to a push: surrender blind and ante over the BU's push reach.
        let push_reach = self.w_pair.dot(bu_push);
        let v_fold_call = &push_reach * st.fold_sb;

        // Open-push once the BU folds: the BB folds (steal) or calls
        // (heads-up SB vs BB).
        let bu_folds = &self.w_pair - db; // [sb, bb]
        let bb_folds = bb_sb.mapv(|x| 1.0 - x);
        let v_push = bu_folds.dot(&bb_folds) * st.steal_sb
            + ((&self.eq_hu * st.p12 - st.c12) * &bu_folds).dot(bb_sb);

        // Fold behind: surrender blind and ante over the BU's fold reach.
        let v_fold_push = (&self.w_own - &push_reach) * st.fold_sb;

        (v_call, v_fold_call, v_push, v_fold_push)
    }

    /// Big blind counterfactual values for its three decision points: facing
    /// push + call, facing push + fold, and facing fold + push.
    #[allow(clippy::type_complexity)]
    fn bb_values(
        &self,
        bu_push: &Array1<f32>,
        sb_call: &Array1<f32>,
        sb_push: &Array1<f32>,
        dc: &Array2<f32>,
        db: &Array2<f32>,
        wec: &Array2<f32>,
    ) -> (
        Array1<f32>,
        Array1<f32>,
        Array1<f32>,
        Array1<f32>,
        Array1<f32>,
        Array1<f32>,
    ) {
        let st = &self.stakes;

        // Facing push + call: calling closes a three-way all-in.
        let both_reach = dc.dot(bu_push); // sum w * bu_push * sb_call
        let v_call_both =
            self.allin_values(BB, wec, dc, bu_push, [(BU, dc, bu_push), (SB, db, sb_call)]);
        let v_fold_both = &both_reach * st.fold_bb;

        // Facing push + fold: heads-up BB vs BU over the SB's dead blind.
        let sb_folds = &self.w_pair - dc; // [bb, bu]
        let v_call_bu = ((&self.eq_hu * st.p02 - st.c02) * &sb_folds).dot(bu_push);
        let v_fold_bu = sb_folds.dot(bu_push) * st.fold_bb;

        // Facing fold + push: heads-up BB vs SB over the BU's dead ante.
        let bu_folds = &self.w_pair - db; // [bb, sb]
        let v_call_sb = ((&self.eq_hu * st.p12 - st.c12) * &bu_folds).dot(sb_push);
        let v_fold_sb = bu_folds.dot(sb_push) * st.fold_bb;

        (
            v_call_both,
            v_fold_both,
            v_call_bu,
            v_fold_bu,
            v_call_sb,
            v_fold_sb,
        )
    }

    /// NashConv of a strategy profile: the sum over all players of how much
    /// a best response would gain over playing the profile, normalized per
    /// deal. Each decision point is a player's only action on its paths, so
    /// the gap decomposes into per-infoset `max(actions) - mix(actions)`
    /// terms. Zero exactly at a Nash equilibrium; with three players CFR
    /// does not guarantee reaching one, but a small gap still bounds how much
    /// any single player can gain by deviating.
    fn exploitability(
        &self,
        bu_push: &Array1<f32>,
        sb_call: &Array1<f32>,
        sb_push: &Array1<f32>,
        bb_call_both: &Array1<f32>,
        bb_call_bu: &Array1<f32>,
        bb_call_sb: &Array1<f32>,
    ) -> f32 {
        let dc = contract_last(&self.w, sb_call);
        let db = contract_last(&self.w, bu_push);
        let dd3 = contract_last(&self.w, bb_call_both);
        let wed3 = contract_last(&self.we, bb_call_both);
        let wec = contract_last(&self.we, sb_call);

        let (v_push, v_fold) = self.bu_values(sb_call, bb_call_bu, bb_call_both, &dc, &dd3, &wed3);
        let (v_call, v_fold_call, v_open, v_fold_open) =
            self.sb_values(bu_push, bb_call_sb, bb_call_both, &db, &dd3, &wed3);
        let (v_both, v_fold_both, v_bu, v_fold_bu, v_sb, v_fold_sb) =
            self.bb_values(bu_push, sb_call, sb_push, &dc, &db, &wec);

        let decisions: [(&Array1<f32>, &Array1<f32>, &Array1<f32>); 6] = [
            (bu_push, &v_push, &v_fold),
            (sb_call, &v_call, &v_fold_call),
            (sb_push, &v_open, &v_fold_open),
            (bb_call_both, &v_both, &v_fold_both),
            (bb_call_bu, &v_bu, &v_fold_bu),
            (bb_call_sb, &v_sb, &v_fold_sb),
        ];

        let mut gap = 0.0_f32;
        for (sigma, v_act, v_fold) in decisions {
            for i in 0..N_INFOSETS {
                let node = sigma[i] * v_act[i] + (1.0 - sigma[i]) * v_fold[i];
                gap += v_act[i].max(v_fold[i]) - node;
            }
        }
        gap / self.w_own.sum()
    }
}

impl Default for ThreewaySolver {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use pushfold_shared::{N_RANKS, hands};

    const STACKS: [f32; 3] = [5.0, 5.0, 5.0];
    const SMALL_BLIND: f32 = 0.5;
    const ANTE: f32 = 0.125;

    fn solve(solver: &mut ThreewaySolver, stacks: [f32; 3], iterations: u32) -> Strategies {
        solver
            .solve(
                stacks[BU],
                stacks[SB],
                stacks[BB],
                SMALL_BLIND,
                ANTE,
                iterations,
            )
            .unwrap()
    }

    /// Renders a strategy vector as the 13x13 hand grid, one cell per infoset
    /// showing the label and its action frequency as a percentage. The layout
    /// matches a standard range chart: pairs on the diagonal, suited hands in
    /// the upper triangle, offsuit in the lower.
    fn format_grid(strat: &Array1<f32>) -> String {
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

    /// Solves at the shared stakes and prints all six converged ranges. Run
    /// with `cargo test -p pushfold-threeway print_strategies -- --nocapture`
    /// to see output.
    #[test]
    fn print_strategies() {
        let mut solver = ThreewaySolver::new();
        let out = solve(&mut solver, STACKS, 1000);
        println!(
            "\nConverged strategies at stacks={STACKS:?}bb sb={SMALL_BLIND} ante={ANTE} \
             (exploitability {:.2e})",
            out.exploitability
        );
        println!("\nButton push range:\n{}", format_grid(&out.bu_push));
        println!("SB call-a-push range:\n{}", format_grid(&out.sb_call));
        println!("SB open-push range:\n{}", format_grid(&out.sb_push));
        println!(
            "BB call vs push + call range:\n{}",
            format_grid(&out.bb_call_both)
        );
        println!(
            "BB call vs BU push range:\n{}",
            format_grid(&out.bb_call_bu)
        );
        println!(
            "BB call vs SB push range:\n{}",
            format_grid(&out.bb_call_sb)
        );
    }

    #[test]
    fn test_validate() {
        use SolverError::*;
        assert!(validate_inputs([5.0, 5.0, 5.0], 0.5, 0.125, 200).is_ok());
        assert!(validate_inputs([1.0, 2.0, 3.0], 0.5, 0.0, 1).is_ok());
        assert_eq!(
            validate_inputs([f32::NAN, 5.0, 5.0], 0.5, 0.125, 200),
            Err(NonFiniteInput)
        );
        assert_eq!(
            validate_inputs([5.0, 0.0, 5.0], 0.5, 0.125, 200),
            Err(StackNotPositive)
        );
        assert_eq!(
            validate_inputs([5.0, 5.0, 5.0], -0.5, 0.125, 200),
            Err(NegativeBlindOrAnte)
        );
        assert_eq!(
            validate_inputs([5.0, 0.25, 5.0], 0.5, 0.125, 200),
            Err(SmallBlindExceedsStack)
        );
        assert_eq!(
            validate_inputs([5.0, 5.0, 0.5,], 0.0, 0.125, 200),
            Err(BigBlindExceedsStack)
        );
        assert_eq!(
            validate_inputs([5.0, 5.0, 5.0], 0.5, 0.125, 0),
            Err(ZeroIterations)
        );
    }

    #[test]
    fn test_strategies_are_probabilities() {
        let mut solver = ThreewaySolver::new();
        let out = solve(&mut solver, STACKS, 200);
        for strategy in [
            &out.bu_push,
            &out.sb_call,
            &out.sb_push,
            &out.bb_call_both,
            &out.bb_call_bu,
            &out.bb_call_sb,
        ] {
            for x in strategy {
                assert!((0.0..=1.0).contains(x), "strategy out of range: {x}");
            }
        }
    }

    #[test]
    fn test_premium_hands_are_pure() {
        // AA is infoset 0 in the row-major 13x13 grid. At 5bb it pushes and
        // calls everywhere; with no epsilon floor the averages should be
        // exactly (or extremely near) 1.
        let mut solver = ThreewaySolver::new();
        let out = solve(&mut solver, STACKS, 1000);
        for (name, strategy) in [
            ("bu_push", &out.bu_push),
            ("sb_call", &out.sb_call),
            ("sb_push", &out.sb_push),
            ("bb_call_both", &out.bb_call_both),
            ("bb_call_bu", &out.bb_call_bu),
            ("bb_call_sb", &out.bb_call_sb),
        ] {
            assert!(strategy[0] > 0.999, "AA {name} freq {}", strategy[0]);
        }
    }

    #[test]
    fn test_converges() {
        let mut solver = ThreewaySolver::new();
        let coarse = solve(&mut solver, STACKS, 100);
        let fine = solve(&mut solver, STACKS, 1500);
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

    /// The payoffs are zero-sum, so the three players' expected values under
    /// any single profile must cancel. Checks with the converged profile of
    /// an asymmetric-stack game, exercising the side-pot branches too.
    #[test]
    fn test_values_are_zero_sum() {
        let mut solver = ThreewaySolver::new();
        let out = solve(&mut solver, [8.0, 5.0, 3.0], 200);

        let dc = contract_last(&solver.w, &out.sb_call);
        let db = contract_last(&solver.w, &out.bu_push);
        let dd3 = contract_last(&solver.w, &out.bb_call_both);
        let wed3 = contract_last(&solver.we, &out.bb_call_both);
        let wec = contract_last(&solver.we, &out.sb_call);

        let (v_push, v_fold) = solver.bu_values(
            &out.sb_call,
            &out.bb_call_bu,
            &out.bb_call_both,
            &dc,
            &dd3,
            &wed3,
        );
        let (v_call, v_fold_call, v_open, v_fold_open) = solver.sb_values(
            &out.bu_push,
            &out.bb_call_sb,
            &out.bb_call_both,
            &db,
            &dd3,
            &wed3,
        );
        let (v_both, v_fold_both, v_bu, v_fold_bu, v_sb, v_fold_sb) =
            solver.bb_values(&out.bu_push, &out.sb_call, &out.sb_push, &dc, &db, &wec);

        let mix = |sigma: &Array1<f32>, act: &Array1<f32>, fold: &Array1<f32>| -> f64 {
            (0..N_INFOSETS)
                .map(|i| (sigma[i] * act[i] + (1.0 - sigma[i]) * fold[i]) as f64)
                .sum()
        };
        let mut total = mix(&out.bu_push, &v_push, &v_fold)
            + mix(&out.sb_call, &v_call, &v_fold_call)
            + mix(&out.sb_push, &v_open, &v_fold_open)
            + mix(&out.bb_call_both, &v_both, &v_fold_both)
            + mix(&out.bb_call_bu, &v_bu, &v_fold_bu)
            + mix(&out.bb_call_sb, &v_sb, &v_fold_sb);

        // When the button and small blind both fold, the big blind collects
        // the blinds without ever acting, so that terminal appears in no BB
        // decision value; add it back for the full-game total. (It is
        // constant in the BB's strategy, so the solver itself never needs it.)
        let open_fold_reach = &solver.w_own - &solver.w_pair.dot(&out.bu_push);
        let bb_steal: f64 = (0..N_INFOSETS)
            .map(|j| ((1.0 - out.sb_push[j]) * open_fold_reach[j]) as f64)
            .sum();
        total += bb_steal * (SMALL_BLIND + 2.0 * ANTE) as f64;

        // Normalize per deal; f32 accumulation over 169^3 terms leaves some
        // noise, so the tolerance is looser than pure float equality.
        let per_deal = total / solver.w_own.sum() as f64;
        assert!(per_deal.abs() < 1e-3, "values not zero-sum: {per_deal}");
    }
}
