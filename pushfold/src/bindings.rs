//! wasm-bindgen bindings for the push-fold solver.
//!
//! These thin newtype wrappers expose the pure-Rust solver in
//! [`crate::pushfold`] to JavaScript. Each holds the corresponding core value
//! and forwards to it; the only JS-specific work is flattening nalgebra vectors
//! into `Float32Array`-shaped `Vec<f32>` getters. `SolverError` is exported
//! directly from the core (it carries the `#[wasm_bindgen]` attribute there,
//! since a JS enum has to be annotated on its own declaration).

use wasm_bindgen::prelude::*;

use crate::pushfold::{PushFoldSolver as CoreSolver, SolverError, Strategies as CoreStrategies};

/// Converged strategies from a solve. `bu_push` and `bb_call` are each 169
/// frequencies in `[0, 1]`, row-major over the 13x13 hand grid (the button's
/// push frequency and the big blind's call frequency respectively); wasm-bindgen
/// surfaces the getters to JS as `Float32Array`s.
#[wasm_bindgen]
pub struct Strategies {
    inner: CoreStrategies,
}

#[wasm_bindgen]
impl Strategies {
    /// Button push frequency per infoset (169 entries).
    #[wasm_bindgen(getter)]
    pub fn bu_push(&self) -> Vec<f32> {
        self.inner.bu_push.as_slice().to_vec()
    }

    /// Big blind call frequency per infoset (169 entries).
    #[wasm_bindgen(getter)]
    pub fn bb_call(&self) -> Vec<f32> {
        self.inner.bb_call.as_slice().to_vec()
    }

    /// Nash gap of the returned strategy pair, in big-blind units per deal.
    #[wasm_bindgen(getter)]
    pub fn exploitability(&self) -> f32 {
        self.inner.exploitability
    }
}

/// Push/fold solver handle. Constructing it builds the (constant) equity and
/// matchup lookup tables once; reuse the same handle to solve at different
/// stakes rather than paying that setup per solve.
#[wasm_bindgen]
pub struct PushFoldSolver {
    inner: CoreSolver,
}

#[wasm_bindgen]
impl PushFoldSolver {
    #[wasm_bindgen(constructor)]
    pub fn new() -> Self {
        PushFoldSolver {
            inner: CoreSolver::new(),
        }
    }

    /// Runs CFR+ for `iterations` and returns the averaged strategies, or
    /// throws a [`SolverError`] into JS if the stakes are invalid.
    pub fn solve(
        &mut self,
        stack: f32,
        sb: f32,
        ante: f32,
        iterations: u32,
    ) -> Result<Strategies, SolverError> {
        self.inner
            .solve(stack, sb, ante, iterations)
            .map(|inner| Strategies { inner })
    }
}

impl Default for PushFoldSolver {
    fn default() -> Self {
        Self::new()
    }
}
