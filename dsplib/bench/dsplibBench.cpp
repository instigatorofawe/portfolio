#include <benchmark/benchmark.h>

#include <dsplib/dsplib.hpp>

static void BM_DbToLinear(benchmark::State& state) {
    double db = 0.0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(dsplib::db_to_linear(db));
        db += 0.5;
    }
}
BENCHMARK(BM_DbToLinear);
