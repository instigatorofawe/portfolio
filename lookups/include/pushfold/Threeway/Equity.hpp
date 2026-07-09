#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <omp/EquityCalculator.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "pushfold/Constants.hpp"
#include "pushfold/Threeway/Colex.hpp"
#include "pushfold/Threeway/Constants.hpp"

namespace pushfold::threeway {

/**
 * @brief Generates a lookup table of three-way all-in equities.
 *
 * A three-way deal collapses to an unordered multiset of three infosets, indexed
 * by its colex rank (see Colex.hpp), so the table is flat with one entry per
 * matchup — but each entry is a triplet: the pot share of each of the three
 * hands. The three floats are ordered to match ColexTuple(index), i.e. by
 * ascending infoset, so a consumer holding an ordered assignment maps its player
 * to the corresponding sorted position. The triplet sums to ~1 (all-in equity
 * splits the whole pot), and repeated infosets simply carry equal shares.
 *
 * Impossible matchups (e.g. three identical pocket pairs) have no legal deal, so
 * their frequency is zero and their equities are never weighted; they are stored
 * as zeros.
 */
class EquityGenerator {
    static constexpr size_t kPlayers = 3;
    // The three pot shares sum to 1, so storing two recovers the third. Keeping
    // only the first two halves the emitted table.
    static constexpr size_t kStoredEquities = kPlayers - 1;

    using Equity = std::array<float, kPlayers>;
    using Table = std::array<Equity, kNumMatchupEntries>;
    using QuantizedEquity = std::array<uint16_t, kStoredEquities>;
    using QuantizedTable = std::array<QuantizedEquity, kNumMatchupEntries>;

    std::unique_ptr<Table> equities_;
    bool solved_;

   public:
    EquityGenerator() : equities_(std::make_unique<Table>()), solved_(false) {}

    /**
     * @brief Constructs from a precomputed equity table, forwarding the values
     *        into place and marking the table as already solved.
     */
    template <typename U>
        requires std::same_as<std::remove_cvref_t<U>, Table>
    explicit EquityGenerator(U&& equities)
        : equities_(std::make_unique<Table>(std::forward<U>(equities))), solved_(true) {}

    bool Ready() const { return solved_; }

    /**
     * @brief Solves for the equity table by exact enumeration.
     *
     * Each colex slot is one matchup; ColexTuple recovers its (ascending) infoset
     * triple, which becomes three OMPEval hand ranges. Every matchup is
     * independent and writes to a distinct slot, so the loop runs in parallel.
     * Dynamic scheduling balances the uneven per-matchup cost (blocking and range
     * overlap change how many deals enumerate).
     *
     * @p progress, if set, is called periodically with (completed, total) matchup
     * counts. It is invoked from a serialized section, so it needs no locking of
     * its own; counts may arrive slightly out of order across threads.
     */
    void Solve(const std::function<void(size_t completed, size_t total)>& progress = {}) {
        if (solved_) {
            return;
        }

        // Report progress every this many matchups: frequent enough for a live
        // bar over a multi-minute solve, rare enough that the serialized callback
        // is not a contention point.
        constexpr size_t kProgressInterval = 2048;
        std::atomic<size_t> completed = 0;

#pragma omp parallel for schedule(dynamic)
        for (size_t index = 0; index < kNumMatchupEntries; ++index) {
            const std::array<size_t, kPlayers> tuple = ColexTuple(index);

            std::vector<omp::CardRange> ranges;
            ranges.reserve(kPlayers);
            for (size_t p = 0; p < kPlayers; ++p) {
                const std::string_view hand = kHands[tuple[p] / kNumRanks][tuple[p] % kNumRanks];
                ranges.emplace_back(std::string(hand));
            }

            // Parallelism source depends on the build: with OpenMP the loop above
            // runs matchups concurrently, so each one stays single-threaded to
            // avoid oversubscribing the cores. Without OpenMP the loop is serial,
            // so let each matchup fan out across all hardware threads instead.
#ifdef _OPENMP
            constexpr unsigned kThreadsPerMatchup = 1;
#else
            constexpr unsigned kThreadsPerMatchup = 0;  // 0 = all hardware threads
#endif
            omp::EquityCalculator eq;
            // start() returns false when no legal deal exists for the ranges (an
            // impossible matchup). Its frequency is zero, so leave the slot's
            // equities at their zero-initialized value and skip the calculation.
            if (eq.start(ranges, 0, 0, /*enumerateAll=*/true, /*stdevTarget=*/5e-5, /*callback=*/nullptr,
                         /*updateInterval=*/0.2, /*threadCount=*/kThreadsPerMatchup)) {
                eq.wait();
                const omp::EquityCalculator::Results results = eq.getResults();
                (*equities_)[index] = {
                    static_cast<float>(results.equity[0]),
                    static_cast<float>(results.equity[1]),
                    static_cast<float>(results.equity[2]),
                };
            }

            // Count every matchup, solved or impossible, so the bar reaches 100%;
            // force a report on the last one regardless of the interval.
            const size_t done = completed.fetch_add(1) + 1;
            if (progress && (done % kProgressInterval == 0 || done == kNumMatchupEntries)) {
#pragma omp critical(threeway_equity_progress)
                progress(done, kNumMatchupEntries);
            }
        }

        solved_ = true;
    }

    /**
     * @brief Returns the solved equity table, solving lazily on first access.
     */
    const Table& Equities() {
        Solve();
        return *equities_;
    }

    /**
     * @brief Quantizes the table to fixed point for emission.
     *
     * Each matchup keeps its first two pot shares, each rounded to one of
     * max<uint16_t>() + 1 levels over [0, 1]; the third is dropped and recovered
     * by Dequantize as the remainder. Returned by unique_ptr because the table is
     * several megabytes. Impossible matchups (stored as zeros) quantize to zeros.
     */
    std::unique_ptr<QuantizedTable> Quantize() {
        Solve();
        auto result = std::make_unique<QuantizedTable>();
        for (size_t index = 0; index < kNumMatchupEntries; ++index) {
            const Equity& equity = (*equities_)[index];
            for (size_t p = 0; p < kStoredEquities; ++p) {
                (*result)[index][p] =
                    static_cast<uint16_t>(static_cast<double>(equity[p]) * std::numeric_limits<uint16_t>::max() + 0.5);
            }
        }
        return result;
    }

    /**
     * @brief Rebuilds an equity table from its fixed-point representation.
     *
     * Inverts Quantize: the two stored shares are scaled back into [0, 1] and the
     * third is set to 1 minus their sum, mirroring the layout Quantize emits.
     * (An impossible matchup's dropped third reconstructs to 1 rather than 0, but
     * its deal frequency is zero, so the value is never weighted.)
     */
    static EquityGenerator Dequantize(const QuantizedTable& values) {
        EquityGenerator result;
        result.solved_ = true;
        for (size_t index = 0; index < kNumMatchupEntries; ++index) {
            Equity& equity = (*result.equities_)[index];
            float stored_sum = 0.0f;
            for (size_t p = 0; p < kStoredEquities; ++p) {
                equity[p] =
                    static_cast<float>(static_cast<double>(values[index][p]) / std::numeric_limits<uint16_t>::max());
                stored_sum += equity[p];
            }
            equity[kStoredEquities] = 1.0f - stored_sum;
        }
        return result;
    }
};

}  // namespace pushfold::threeway
