use criterion::{Criterion, criterion_group, criterion_main};
use pushfold::solve_push_fold;
use std::hint::black_box;

fn bench_solve_push_fold(c: &mut Criterion) {
    c.bench_function("solve_push_fold(5.0, 0.5, 0.125, 200)", |b| {
        b.iter(|| {
            solve_push_fold(
                black_box(5.0),
                black_box(0.5),
                black_box(0.125),
                black_box(200),
            )
            .unwrap()
        })
    });
}

criterion_group!(benches, bench_solve_push_fold);
criterion_main!(benches);
