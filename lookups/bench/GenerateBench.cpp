#include <benchmark/benchmark.h>
#include <cstdint>
#include <memory>

#include <pushfold/Headsup/Equity.hpp>
#include <pushfold/Headsup/Matchups.hpp>

namespace pushfold::headsup {
namespace {

// Mirrors generate.cpp: a fresh generator solved and quantized to the fixed
// point form written into the equity table. Solve() dominates (exact
// enumeration of every infoset matchup), so the quantization cost is noise.
void BM_GenerateEquities(benchmark::State& state) {
    for (auto _ : state) {
        // Heap-allocated to match generate.cpp and keep the ~114KB equity matrix
        // off the stack.
        const auto generator = std::make_unique<EquityGenerator>();
        auto table = generator->Quantize<std::uint16_t>();
        benchmark::DoNotOptimize(table);
    }
}
// Solve() runs in seconds, so report in milliseconds; the default 0.5s min time
// then settles on a single iteration. UseRealTime() because Solve() fans out
// across threads with OpenMP: the default CPU-time clock only counts the calling
// thread and would wildly understate the elapsed (and so wall) time.
BENCHMARK(BM_GenerateEquities)->Unit(benchmark::kMillisecond)->UseRealTime();

// Mirrors generate.cpp: a fresh generator solved and flattened to the upper
// triangle written into the matchup table.
void BM_GenerateMatchups(benchmark::State& state) {
    for (auto _ : state) {
        const auto generator = std::make_unique<MatchupGenerator>();
        auto table = generator->Flatten();
        benchmark::DoNotOptimize(table);
    }
}
BENCHMARK(BM_GenerateMatchups)->Unit(benchmark::kMillisecond);

}  // namespace
}  // namespace pushfold::headsup
