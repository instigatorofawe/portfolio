use crate::constants::*;
use crate::util::*;
use wasm_bindgen::prelude::*;

/// Matchup index for the button player's infoset `i`, member `j`.
///
/// The button's matchups are laid out row-major, so an infoset's members form a
/// contiguous block. Computing the index is cheaper than the 114 KB lookup
/// table it replaces.
#[inline]
fn bu_index(i: usize, j: usize) -> usize {
    N_INFOSETS * i + j
}

/// Matchup index for the big blind player's infoset `i`, member `j`.
///
/// The big blind's grouping is the transpose of the button's, so members are
/// strided by `N_INFOSETS`.
#[inline]
fn bb_index(i: usize, j: usize) -> usize {
    N_INFOSETS * j + i
}

/// Scatters each infoset's `strategy` value across the `members` matchups that
/// belong to it, addressed by `index(infoset, member)`.
fn expand_strategy<const N: usize>(
    out: &mut [f32],
    strategy: &[f32; N],
    members: usize,
    index: impl Fn(usize, usize) -> usize,
) {
    strategy.iter().enumerate().for_each(|(i, &s)| {
        (0..members).for_each(|j| out[index(i, j)] = s);
    });
}

fn infoset_probability<const N: usize>(
    probability: &[f32],
    members: usize,
    index: impl Fn(usize, usize) -> usize,
) -> [f32; N] {
    std::array::from_fn(|i| (0..members).map(|j| probability[index(i, j)]).sum())
}

fn infoset_ev<const N: usize>(
    probability: &[f32],
    ev: &[f32],
    members: usize,
    index: impl Fn(usize, usize) -> usize,
) -> [f32; N] {
    std::array::from_fn(|i| {
        let total_prob: f32 = (0..members).map(|j| probability[index(i, j)]).sum();
        (0..members)
            .map(|j| {
                let idx = index(i, j);
                ev[idx] * probability[idx]
            })
            .sum::<f32>()
            / total_prob
    })
}

fn regret_match<const N: usize>(regrets: &[f32; N]) -> [f32; N] {
    const EPSILON: f32 = 1e-6;
    let any_nonzero = regrets.iter().any(|x| *x > 0.);

    if any_nonzero {
        let mut result: [f32; N] = std::array::from_fn(|i| EPSILON.max(regrets[i]));
        let total: f32 = result.iter().sum();
        scalar_div(&mut result, total);

        result
    } else {
        [1. / N as f32; N]
    }
}

fn update_strategy<const N: usize>(
    avg_strat: &mut [f32; N],
    total_prob: &mut [f32; N],
    strat: &[f32; N],
    prob: &[f32; N],
) {
    let mut new_total_prob = *total_prob;
    elem_add(&mut new_total_prob, prob);

    let mut decay = *total_prob;
    elem_div(&mut decay, &new_total_prob);
    elem_mul(avg_strat, &decay);

    let mut contribution = *prob;
    elem_div(&mut contribution, &new_total_prob);
    elem_mul(&mut contribution, strat);
    elem_add(avg_strat, &contribution);

    elem_add(total_prob, prob);
}

#[wasm_bindgen]
pub fn solve_push_fold(stack: f32, sb: f32, ante: f32, iter: u32) -> Vec<f32> {
    // The matchup table ships as a symmetric upper triangle of `u8` counts to
    // keep the wasm blob small; expand it once into the full `f32` matrix the
    // solver reads from.
    let matchups: Box<[f32; N_MATCHUPS]> = matchups();

    let infoset_p_root: [f32; N_INFOSETS] =
        infoset_probability(matchups.as_slice(), N_INFOSETS, bu_index);

    let mut regrets_bu = [[0_f32; N_INFOSETS]; 2];
    let mut regrets_bb = [[0_f32; N_INFOSETS]; 2];

    let mut strat_bu = [0.5_f32; N_INFOSETS];
    let mut strat_bb = [0.5_f32; N_INFOSETS];

    let mut avg_strat_bu = [0.5_f32; N_INFOSETS];
    let mut avg_strat_bb = [0.5_f32; N_INFOSETS];

    let mut total_prob_bu: [f32; N_INFOSETS] =
        infoset_probability(matchups.as_slice(), N_INFOSETS, bu_index);
    let mut total_prob_bb: [f32; N_INFOSETS];
    {
        let mut prob_b = matchups.clone();
        scalar_mul(prob_b.as_mut_slice(), 0.5);
        total_prob_bb = infoset_probability(prob_b.as_slice(), N_INFOSETS, bb_index);
    }

    // Compute terminal node payouts
    let payouts_f: Box<[f32; N_MATCHUPS]> = Box::new([-sb - ante; N_MATCHUPS]);
    let payouts_bf: Box<[f32; N_MATCHUPS]> = Box::new([1.0 + ante; N_MATCHUPS]);
    let payouts_bc;
    {
        let mut payouts = equities();
        scalar_sub(payouts.as_mut_slice(), 0.5);
        scalar_mul(payouts.as_mut_slice(), 2.0 * (stack + ante));
        payouts_bc = payouts;
    }

    // Reusable scratch buffers, allocated once and overwritten each iteration.
    let mut expanded_strat_bu = Box::new([0_f32; N_MATCHUPS]);
    let mut expanded_strat_bb = Box::new([0_f32; N_MATCHUPS]);
    let mut p_b = Box::new([0_f32; N_MATCHUPS]);
    let mut p_f = Box::new([0_f32; N_MATCHUPS]);
    let mut p_bc = Box::new([0_f32; N_MATCHUPS]);
    let mut p_bf = Box::new([0_f32; N_MATCHUPS]);
    let mut ev_b = Box::new([0_f32; N_MATCHUPS]);
    let mut ev_root = Box::new([0_f32; N_MATCHUPS]);
    let mut scratch = Box::new([0_f32; N_MATCHUPS]);

    for current_iter in 1..=iter {
        let decay_coefficient: f32 = (current_iter as f32) / (current_iter as f32 + 1.0);
        // Compute probabilities and EV of each node
        expand_strategy(
            expanded_strat_bu.as_mut_slice(),
            &strat_bu,
            N_INFOSETS,
            bu_index,
        );
        expand_strategy(
            expanded_strat_bb.as_mut_slice(),
            &strat_bb,
            N_INFOSETS,
            bb_index,
        );

        // p_b = matchups * strat_bu, p_f = matchups - p_b
        p_b.copy_from_slice(matchups.as_slice());
        elem_mul(p_b.as_mut_slice(), expanded_strat_bu.as_slice());
        p_f.copy_from_slice(matchups.as_slice());
        elem_sub(p_f.as_mut_slice(), p_b.as_slice());

        // p_bc = p_b * strat_bb, p_bf = p_b - p_bc
        p_bc.copy_from_slice(p_b.as_slice());
        elem_mul(p_bc.as_mut_slice(), expanded_strat_bb.as_slice());
        p_bf.copy_from_slice(p_b.as_slice());
        elem_sub(p_bf.as_mut_slice(), p_bc.as_slice());

        // ev_b = (payouts_bc * p_bc + payouts_bf * p_bf) / p_b
        ev_b.copy_from_slice(payouts_bc.as_slice());
        elem_mul(ev_b.as_mut_slice(), p_bc.as_slice());
        scratch.copy_from_slice(payouts_bf.as_slice());
        elem_mul(scratch.as_mut_slice(), p_bf.as_slice());
        elem_add(ev_b.as_mut_slice(), scratch.as_slice());
        elem_div(ev_b.as_mut_slice(), p_b.as_slice());

        // ev_root = (ev_b * p_b + payouts_f * p_f) / matchups
        ev_root.copy_from_slice(ev_b.as_slice());
        elem_mul(ev_root.as_mut_slice(), p_b.as_slice());
        scratch.copy_from_slice(payouts_f.as_slice());
        elem_mul(scratch.as_mut_slice(), p_f.as_slice());
        elem_add(ev_root.as_mut_slice(), scratch.as_slice());
        elem_div(ev_root.as_mut_slice(), matchups.as_slice());

        // Compute regrets for each infoset
        // Compute new strategy via regret matching
        // Update average strategy
        {
            let infoset_ev_bf: [f32; N_INFOSETS] =
                infoset_ev(p_bf.as_slice(), payouts_bf.as_slice(), N_INFOSETS, bb_index);
            let infoset_ev_bc: [f32; N_INFOSETS] =
                infoset_ev(p_bc.as_slice(), payouts_bc.as_slice(), N_INFOSETS, bb_index);
            let infoset_ev_b: [f32; N_INFOSETS] =
                infoset_ev(p_b.as_slice(), ev_b.as_slice(), N_INFOSETS, bb_index);

            let mut regrets_bb_bc = infoset_ev_b;
            elem_sub(&mut regrets_bb_bc, &infoset_ev_bc);
            elem_add(&mut regrets_bb[0], &regrets_bb_bc);

            let mut regrets_bb_bf = infoset_ev_b;
            elem_sub(&mut regrets_bb_bf, &infoset_ev_bf);
            elem_add(&mut regrets_bb[1], &regrets_bb_bf);

            regrets_bb[0].iter_mut().for_each(|x| {
                if *x < 0.0 {
                    *x = 0.0;
                };
            });
            regrets_bb[1].iter_mut().for_each(|x| {
                if *x < 0.0 {
                    *x = 0.0;
                };
            });

            strat_bb =
                std::array::from_fn(|i| regret_match(&[regrets_bb[0][i], regrets_bb[1][i]])[0]);
            let infoset_p_b: [f32; N_INFOSETS] =
                infoset_probability(p_b.as_slice(), N_INFOSETS, bb_index);

            update_strategy(
                &mut avg_strat_bb,
                &mut total_prob_bb,
                &strat_bb,
                &infoset_p_b,
            );

            scalar_mul(&mut regrets_bb[0], decay_coefficient);
            scalar_mul(&mut regrets_bb[1], decay_coefficient);
            scalar_mul(&mut total_prob_bb, decay_coefficient);
        }

        {
            let infoset_ev_b: [f32; N_INFOSETS] =
                infoset_ev(p_b.as_slice(), ev_b.as_slice(), N_INFOSETS, bu_index);
            let infoset_ev_f: [f32; N_INFOSETS] =
                infoset_ev(p_f.as_slice(), payouts_f.as_slice(), N_INFOSETS, bu_index);
            let infoset_ev_root: [f32; N_INFOSETS] = infoset_ev(
                matchups.as_slice(),
                ev_root.as_slice(),
                N_INFOSETS,
                bu_index,
            );

            let mut regrets_bu_b = infoset_ev_b;
            elem_sub(&mut regrets_bu_b, &infoset_ev_root);
            elem_add(&mut regrets_bu[0], &regrets_bu_b);

            let mut regrets_bu_f = infoset_ev_f;
            elem_sub(&mut regrets_bu_f, &infoset_ev_root);
            elem_add(&mut regrets_bu[1], &regrets_bu_f);

            regrets_bu[0].iter_mut().for_each(|x| {
                if *x < 0.0 {
                    *x = 0.0;
                };
            });
            regrets_bu[1].iter_mut().for_each(|x| {
                if *x < 0.0 {
                    *x = 0.0;
                };
            });

            strat_bu =
                std::array::from_fn(|i| regret_match(&[regrets_bu[0][i], regrets_bu[1][i]])[0]);

            update_strategy(
                &mut avg_strat_bu,
                &mut total_prob_bu,
                &strat_bu,
                &infoset_p_root,
            );

            scalar_mul(&mut regrets_bu[0], decay_coefficient);
            scalar_mul(&mut regrets_bu[1], decay_coefficient);
            scalar_mul(&mut total_prob_bu, decay_coefficient);
        }
    }

    // Test output
    #[cfg(test)]
    {
        let hands = hands();
        let print_strategy = |strat: &[f32; N_INFOSETS]| {
            strat.iter().enumerate().for_each(|(i, x)| {
                if *x > 0.999 {
                    print!("{}", hands[i]);
                    if i < N_INFOSETS - 1 {
                        print!(",");
                    }
                } else if *x > 0.001 {
                    print!("{}:{:.3}", hands[i], x);
                    if i < N_INFOSETS - 1 {
                        print!(",");
                    }
                }
            });
            println!();
        };
        print_strategy(&avg_strat_bu);
        println!();
        print_strategy(&avg_strat_bb);
    }

    // wasm-bindgen requires an owned, dynamically-sized return value across the
    // ABI, so the two strategy arrays are concatenated into a `Vec` here.
    [avg_strat_bu, avg_strat_bb].concat()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_matchups() {
        let matchups = matchups();
        let total_matchups = matchups.iter().sum::<f32>();
        assert_eq!(total_matchups as u32, 52 * 51 * 50 * 49 / 4);

        assert_eq!(matchups.iter().map(|x| *x as u32).max().unwrap(), 144);
        assert_eq!(matchups.iter().map(|x| *x as u32).min().unwrap(), 6);

        use std::collections::HashSet;
        let mut unique_values: Vec<u32> = matchups
            .iter()
            .map(|x| *x as u32)
            .collect::<HashSet<u32>>()
            .into_iter()
            .collect();
        unique_values.sort();

        println!("{:?}", unique_values);
    }

    #[test]
    fn test_expand_strategy() {
        let strategies = [0_f32, 1., 2.];

        // Members [[0, 1], [2, 3], [4, 5]]
        let mut expanded_strategy = [0_f32; 6];
        expand_strategy(&mut expanded_strategy, &strategies, 2, |i, j| 2 * i + j);
        assert_eq!(expanded_strategy, [0., 0., 1., 1., 2., 2.]);

        // Members [[0, 5], [1, 4], [2, 3]]
        let mut expanded_strategy = [0_f32; 6];
        expand_strategy(&mut expanded_strategy, &strategies, 2, |i, j| {
            if j == 0 { i } else { 5 - i }
        });
        assert_eq!(expanded_strategy, [0., 1., 2., 2., 1., 0.]);
    }

    #[test]
    fn test_infosets() {
        for i in 0..N_INFOSETS {
            for j in 0..N_INFOSETS {
                assert_eq!(N_INFOSETS * i + j, bu_index(i, j));
                assert_eq!(N_INFOSETS * i + j, bb_index(j, i));
            }
        }
    }

    #[test]
    fn test_infoset_probability() {
        let probs = [1_f32, 2., 3., 4.];
        let infoset_probs: [f32; 2] = infoset_probability(&probs, 2, |i, j| 2 * i + j);
        assert_eq!(infoset_probs, [3., 7.]);

        let matchups = matchups();
        let infoset_probs_bu: [f32; N_INFOSETS] =
            infoset_probability(matchups.as_slice(), N_INFOSETS, bu_index);
        let infoset_probs_bb: [f32; N_INFOSETS] =
            infoset_probability(matchups.as_slice(), N_INFOSETS, bb_index);

        assert_eq!(52 * 51 * 50 * 49 / 4 / infoset_probs_bu[0] as u32, 221); // Pocket pair, 1326/6
        assert_eq!(52 * 51 * 50 * 49 / 4 / infoset_probs_bu[1] as u32, 1326 / 4); // Suited combo, 1326/4
        assert_eq!(
            52 * 51 * 50 * 49 / 4 / infoset_probs_bu[13] as u32,
            1326 / 12
        ); // Offsuit combo, 1326/12
        assert_eq!(52 * 51 * 50 * 49 / 4 / infoset_probs_bu[14] as u32, 221); // 1326 / 6
        //
        assert_eq!(52 * 51 * 50 * 49 / 4 / infoset_probs_bb[0] as u32, 221); // Pocket pair, 1326/6
        assert_eq!(52 * 51 * 50 * 49 / 4 / infoset_probs_bb[1] as u32, 1326 / 4); // Suited combo, 1326/4
        assert_eq!(
            52 * 51 * 50 * 49 / 4 / infoset_probs_bb[13] as u32,
            1326 / 12
        ); // Offsuit combo, 1326/12
        assert_eq!(52 * 51 * 50 * 49 / 4 / infoset_probs_bb[14] as u32, 221); // 1326 / 6
    }

    #[test]
    fn test_infoset_ev() {
        let probs = [1_f32, 3., 3., 4.];
        let evs = [1_f32, 2., 0., 7.];
        let infoset_evs: [f32; 2] = infoset_ev(&probs, &evs, 2, |i, j| 2 * i + j);
        assert_eq!(infoset_evs, [1.75, 4.]);
    }

    #[test]
    fn test_regret_match() {
        assert_eq!(regret_match(&[0., 0.]), [0.5, 0.5]);
        assert_eq!(regret_match(&[0., -1.]), [0.5, 0.5]);
        assert_eq!(regret_match(&[1., 2., 4., 1.]), [0.125, 0.25, 0.5, 0.125]);
    }

    #[test]
    fn test_push_fold() {
        solve_push_fold(5.0, 0.5, 0.125, 200);
    }
}
