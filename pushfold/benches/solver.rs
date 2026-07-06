use criterion::{Criterion, criterion_group, criterion_main};
use pushfold::pushfold::PushFoldSolver;
use std::hint::black_box;

const STACK: f32 = 5.0;
const SB: f32 = 0.5;
const ANTE: f32 = 0.125;

/// Table construction and per-solve allocation, amortized once per solver.
fn bench_new(c: &mut Criterion) {
    c.bench_function("new", |b| b.iter(PushFoldSolver::new));
}

/// End-to-end solve cost across a range of iteration counts. The solver is
/// reused across samples so this measures CFR work, not table construction;
/// each `solve` rebuilds its stake-dependent state via `setup`.
fn bench_solve(c: &mut Criterion) {
    let mut solver = PushFoldSolver::new();
    let mut group = c.benchmark_group("solve");
    for iterations in [100u32, 500, 2000] {
        group.bench_function(format!("{iterations}_iters"), |b| {
            b.iter(|| {
                solver
                    .solve(
                        black_box(STACK),
                        black_box(SB),
                        black_box(ANTE),
                        black_box(iterations),
                    )
                    .unwrap()
            });
        });
    }
    group.finish();
}

criterion_group!(benches, bench_new, bench_solve);
criterion_main!(benches);
