use criterion::{Criterion, criterion_group, criterion_main};
use pushfold_threeway::solver::ThreewaySolver;
use std::hint::black_box;

const STACK_BU: f32 = 5.0;
const STACK_SB: f32 = 5.0;
const STACK_BB: f32 = 5.0;
const SB: f32 = 0.5;
const ANTE: f32 = 0.125;

/// Table construction and per-solve allocation, amortized once per solver.
fn bench_new(c: &mut Criterion) {
    c.bench_function("new", |b| b.iter(ThreewaySolver::new));
}

/// End-to-end solve cost across a range of iteration counts. The solver is
/// reused across samples so this measures CFR work, not table construction;
/// each `solve` rebuilds its stake-dependent state via `setup`.
///
/// Each CFR+ iteration here is O(N^3) (the extra player turns the heads-up
/// crate's O(N^2) contraction into a tensor contraction), so both the
/// iteration counts and the sample size are kept far below the heads-up
/// benchmark's to keep a full `cargo bench` run tractable.
fn bench_solve(c: &mut Criterion) {
    let mut solver = ThreewaySolver::new();
    let mut group = c.benchmark_group("solve");
    group.sample_size(10);
    for iterations in [10u32, 50, 200] {
        group.bench_function(format!("{iterations}_iters"), |b| {
            b.iter(|| {
                solver
                    .solve(
                        black_box(STACK_BU),
                        black_box(STACK_SB),
                        black_box(STACK_BB),
                        black_box(SB),
                        black_box(ANTE),
                        black_box(iterations),
                        None,
                    )
                    .unwrap()
            });
        });
    }
    group.finish();
}

criterion_group!(benches, bench_new, bench_solve);
criterion_main!(benches);
