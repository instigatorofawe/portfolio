use crate::util::*;
use wasm_bindgen::prelude::*;

fn bu_infosets() -> Vec<Vec<usize>> {
    (0..169_usize)
        .map(|i| (0..169_usize).map(|j| 169 * i + j).collect())
        .collect()
}

fn bb_infosets() -> Vec<Vec<usize>> {
    (0..169_usize)
        .map(|i| (0..169_usize).map(|j| 169 * j + i).collect())
        .collect()
}

fn expand_strategy(strategy: &[f32], infosets: Vec<Vec<usize>>) -> Vec<f32> {
    let n_states: usize = infosets.iter().map(|x| x.len()).sum();
    let mut result = vec![0_f32; n_states];
    infosets.into_iter().enumerate().for_each(|(i, x)| {
        x.into_iter().for_each(|index| result[index] = strategy[i]);
    });
    result
}

fn infoset_probability(probability: &[f32], infosets: Vec<Vec<usize>>) -> Vec<f32> {
    let mut result = vec![0_f32; infosets.len()];
    infosets.into_iter().enumerate().for_each(|(i, x)| {
        result[i] = x.into_iter().map(|j| probability[j]).sum();
    });
    result
}

fn infoset_ev(probability: &[f32], ev: &[f32], infosets: Vec<Vec<usize>>) -> Vec<f32> {
    let mut result = vec![0_f32; infosets.len()];
    infosets.into_iter().enumerate().for_each(|(i, x)| {
        let total_prob: f32 = x.iter().map(|j| probability[*j]).sum();
        result[i] = x.into_iter().map(|j| ev[j] * probability[j]).sum::<f32>() / total_prob;
    });
    result
}

fn regret_match(regrets: &[f32]) -> Vec<f32> {
    const EPSILON: f32 = 1e-6;
    let n: f32 = regrets.len() as f32;
    let any_nonzero = regrets.iter().any(|x| *x > 0.);

    match any_nonzero {
        true => {
            let mut result: Vec<f32> = regrets.iter().map(|x| EPSILON.max(*x)).collect();
            let total: f32 = result.iter().sum();
            scalar_div(&mut result, total);

            result
        }
        false => vec![1. / n; regrets.len()],
    }
}

fn update_strategy(avg_strat: &mut [f32], total_prob: &mut [f32], strat: &[f32], prob: &[f32]) {
    let mut new_total_prob: Vec<f32> = total_prob.to_vec();
    elem_add(&mut new_total_prob, prob);

    let mut decay = total_prob.to_vec();
    elem_div(&mut decay, &new_total_prob);
    elem_mul(avg_strat, &decay);

    let mut contribution = prob.to_vec();
    elem_div(&mut contribution, &new_total_prob);
    elem_mul(&mut contribution, strat);
    elem_add(avg_strat, &contribution);

    elem_add(total_prob, prob);
}

#[wasm_bindgen]
pub fn load_equities() -> Vec<f32> {
    let bytes: &[u8; 169 * 169 * 4] = include_bytes!("equities.bin");
    bytes
        .chunks_exact(4)
        .map(|x| f32::from_le_bytes(x.try_into().unwrap()))
        .collect()
}

#[wasm_bindgen]
pub fn load_matchups() -> Vec<f32> {
    let bytes: &[u8; 169 * 169] = include_bytes!("matchups.bin");
    bytes.map(|x| x as f32).to_vec()
}

#[wasm_bindgen]
pub fn solve_push_fold(stack: f32, sb: f32, ante: f32, iter: u32) -> Vec<f32> {
    let equities = load_equities();
    let matchups = load_matchups();
    let infoset_p_root = infoset_probability(&matchups, bu_infosets());

    let mut regrets_bu = vec![vec![0_f32; 169]; 2];
    let mut regrets_bb = vec![vec![0_f32; 169]; 2];

    let mut strat_bu = vec![0.5_f32; 169];
    let mut strat_bb = vec![0.5_f32; 169];

    let mut avg_strat_bu = vec![0.5_f32; 169];
    let mut avg_strat_bb = vec![0.5_f32; 169];

    let mut total_prob_bu = infoset_probability(&matchups, bu_infosets());
    let mut total_prob_bb;
    {
        let mut prob_b = matchups.clone();
        scalar_mul(&mut prob_b, 0.5);
        total_prob_bb = infoset_probability(&prob_b, bb_infosets());
    }

    // Compute terminal node payouts
    let payouts_f = vec![-sb - ante; 169 * 169];
    let payouts_bf = vec![1.0 + ante; 169 * 169];
    let payouts_bc;
    {
        let mut payouts = equities.clone();
        scalar_sub(&mut payouts, 0.5);
        scalar_mul(&mut payouts, 2.0 * (stack + ante));
        payouts_bc = payouts;
    }

    for current_iter in 1..=iter {
        let decay_coefficient: f32 = (current_iter as f32) / (current_iter as f32 + 1.0);
        // Compute probabilities and EV of each node
        let p_b: Vec<f32>;
        let p_f: Vec<f32>;
        {
            let expanded_strat_bu = expand_strategy(&strat_bu, bu_infosets());
            let mut temp = matchups.clone();
            elem_mul(&mut temp, &expanded_strat_bu);
            p_b = temp;

            let mut temp = matchups.clone();
            elem_sub(&mut temp, &p_b);
            p_f = temp;
        }

        let p_bc: Vec<f32>;
        let p_bf: Vec<f32>;
        {
            let expanded_strat_bb = expand_strategy(&strat_bb, bb_infosets());
            let mut temp = p_b.clone();
            elem_mul(&mut temp, &expanded_strat_bb);
            p_bc = temp;

            let mut temp = p_b.clone();
            elem_sub(&mut temp, &p_bc);
            p_bf = temp;
        }

        let ev_b: Vec<f32>;
        {
            let mut temp = payouts_bc.clone();
            elem_mul(&mut temp, &p_bc);

            let mut temp2 = payouts_bf.clone();
            elem_mul(&mut temp2, &p_bf);

            elem_add(&mut temp, &temp2);
            elem_div(&mut temp, &p_b);
            ev_b = temp;
        }

        let ev_root: Vec<f32>;
        {
            let mut temp = ev_b.clone();
            elem_mul(&mut temp, &p_b);

            let mut temp2 = payouts_f.clone();
            elem_mul(&mut temp2, &p_f);

            elem_add(&mut temp, &temp2);
            elem_div(&mut temp, &matchups);
            ev_root = temp;
        }

        // Compute regrets for each infoset
        // Compute new strategy via regret matching
        // Update average strategy
        {
            let infoset_ev_bf = infoset_ev(&p_bf, &payouts_bf, bb_infosets());
            let infoset_ev_bc = infoset_ev(&p_bc, &payouts_bc, bb_infosets());
            let infoset_ev_b = infoset_ev(&p_b, &ev_b, bb_infosets());

            let mut regrets_bb_bc = infoset_ev_b.clone();
            elem_sub(&mut regrets_bb_bc, &infoset_ev_bc);
            elem_add(&mut regrets_bb[0], &regrets_bb_bc);

            let mut regrets_bb_bf = infoset_ev_b.clone();
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

            strat_bb = (0..169_usize)
                .map(|i| regret_match(&[regrets_bb[0][i], regrets_bb[1][i]])[0])
                .collect();
            let infoset_p_b = infoset_probability(&p_b, bb_infosets());

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
            let infoset_ev_b = infoset_ev(&p_b, &ev_b, bu_infosets());
            let infoset_ev_f = infoset_ev(&p_f, &payouts_f, bu_infosets());
            let infoset_ev_root = infoset_ev(&matchups, &ev_root, bu_infosets());

            let mut regrets_bu_b = infoset_ev_b.clone();
            elem_sub(&mut regrets_bu_b, &infoset_ev_root);
            elem_add(&mut regrets_bu[0], &regrets_bu_b);

            let mut regrets_bu_f = infoset_ev_f.clone();
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

            strat_bu = (0..169_usize)
                .map(|i| regret_match(&[regrets_bu[0][i], regrets_bu[1][i]])[0])
                .collect();

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
        static HANDS: &[&str; 169] = &[
            "22", "32s", "42s", "52s", "62s", "72s", "82s", "92s", "T2s", "J2s", "Q2s", "K2s",
            "A2s", "32o", "33", "43s", "53s", "63s", "73s", "83s", "93s", "T3s", "J3s", "Q3s",
            "K3s", "A3s", "42o", "43o", "44", "54s", "64s", "74s", "84s", "94s", "T4s", "J4s",
            "Q4s", "K4s", "A4s", "52o", "53o", "54o", "55", "65s", "75s", "85s", "95s", "T5s",
            "J5s", "Q5s", "K5s", "A5s", "62o", "63o", "64o", "65o", "66", "76s", "86s", "96s",
            "T6s", "J6s", "Q6s", "K6s", "A6s", "72o", "73o", "74o", "75o", "76o", "77", "87s",
            "97s", "T7s", "J7s", "Q7s", "K7s", "A7s", "82o", "83o", "84o", "85o", "86o", "87o",
            "88", "98s", "T8s", "J8s", "Q8s", "K8s", "A8s", "92o", "93o", "94o", "95o", "96o",
            "97o", "98o", "99", "T9s", "J9s", "Q9s", "K9s", "A9s", "T2o", "T3o", "T4o", "T5o",
            "T6o", "T7o", "T8o", "T9o", "TT", "JTs", "QTs", "KTs", "ATs", "J2o", "J3o", "J4o",
            "J5o", "J6o", "J7o", "J8o", "J9o", "JTo", "JJ", "QJs", "KJs", "AJs", "Q2o", "Q3o",
            "Q4o", "Q5o", "Q6o", "Q7o", "Q8o", "Q9o", "QTo", "QJo", "QQ", "KQs", "AQs", "K2o",
            "K3o", "K4o", "K5o", "K6o", "K7o", "K8o", "K9o", "KTo", "KJo", "KQo", "KK", "AKs",
            "A2o", "A3o", "A4o", "A5o", "A6o", "A7o", "A8o", "A9o", "ATo", "AJo", "AQo", "AKo",
            "AA",
        ];

        avg_strat_bu.iter().enumerate().for_each(|(i, x)| {
            if *x > 0.999 {
                print!("{}", HANDS[i]);
                if i < 168 {
                    print!(",");
                }
            } else if *x > 0.001 {
                print!("{}:{:.3}", HANDS[i], x);
                if i < 168 {
                    print!(",");
                }
            }
        });
        println!();
        println!();
        avg_strat_bb.iter().enumerate().for_each(|(i, x)| {
            if *x > 0.999 {
                print!("{}", HANDS[i]);
                if i < 168 {
                    print!(",");
                }
            } else if *x > 0.001 {
                print!("{}:{:.3}", HANDS[i], x);
                if i < 168 {
                    print!(",");
                }
            }
        });
        println!();
    }

    let mut result = Vec::new();
    result.extend(avg_strat_bu);
    result.extend(avg_strat_bb);
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_matchups() {
        let matchups = load_matchups();
        let total_matchups = matchups.iter().map(|x| *x).sum::<f32>();
        assert_eq!(total_matchups as u32, 52 * 51 * 50 * 49 / 4);

        assert_eq!(matchups.iter().map(|x| *x as u32).max().unwrap(), 144);
        assert_eq!(matchups.iter().map(|x| *x as u32).min().unwrap(), 6);

        use std::collections::HashSet;
        let mut unique_values: Vec<u32> = matchups
            .into_iter()
            .map(|x| x as u32)
            .collect::<HashSet<u32>>()
            .into_iter()
            .collect();
        unique_values.sort();

        println!("{:?}", unique_values);
    }

    #[test]
    fn test_expand_strategy() {
        let strategies: Vec<f32> = vec![0., 1., 2.];

        let infosets: Vec<Vec<usize>> = vec![vec![0, 1], vec![2, 3], vec![4, 5]];
        let expanded_strategy = expand_strategy(&strategies, infosets);
        assert_eq!(expanded_strategy, vec![0., 0., 1., 1., 2., 2.]);

        let infosets: Vec<Vec<usize>> = vec![vec![0, 5], vec![1, 4], vec![2, 3]];
        let expanded_strategy = expand_strategy(&strategies, infosets);
        assert_eq!(expanded_strategy, vec![0., 1., 2., 2., 1., 0.]);
    }

    #[test]
    fn test_infosets() {
        let infosets_bu = bu_infosets();
        let infosets_bb = bb_infosets();

        for i in 0..169_usize {
            for j in 0..169_usize {
                assert_eq!(i * 169 + j, infosets_bu[i][j]);
                assert_eq!(i * 169 + j, infosets_bb[j][i]);
            }
        }
    }

    #[test]
    fn test_infoset_probability() {
        let probs: Vec<f32> = vec![1., 2., 3., 4.];
        let infosets: Vec<Vec<usize>> = vec![vec![0, 1], vec![2, 3]];
        let infoset_probs = infoset_probability(&probs, infosets);
        assert_eq!(infoset_probs, vec![3., 7.]);

        let matchups = load_matchups();
        let infosets_bu = bu_infosets();
        let infoset_probs_bu = infoset_probability(&matchups, infosets_bu);
        let infosets_bb = bb_infosets();
        let infoset_probs_bb = infoset_probability(&matchups, infosets_bb);

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
        let probs: Vec<f32> = vec![1., 3., 3., 4.];
        let evs: Vec<f32> = vec![1., 2., 0., 7.];
        let infosets: Vec<Vec<usize>> = vec![vec![0, 1], vec![2, 3]];
        let infoset_evs = infoset_ev(&probs, &evs, infosets);
        assert_eq!(infoset_evs, vec![1.75, 4.]);
    }

    #[test]
    fn test_regret_match() {
        assert_eq!(regret_match(&[0., 0.]), vec![0.5, 0.5]);
        assert_eq!(regret_match(&[0., -1.]), vec![0.5, 0.5]);
        assert_eq!(
            regret_match(&[1., 2., 4., 1.]),
            vec![0.125, 0.25, 0.5, 0.125]
        );
    }

    #[test]
    fn test_push_fold() {
        solve_push_fold(5.0, 0.5, 0.125, 200);
    }
}
