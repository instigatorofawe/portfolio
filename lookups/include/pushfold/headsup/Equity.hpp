#pragma once

#include <array>
#include <concepts>
#include <limits>
#include <omp/EquityCalculator.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "pushfold/Constants.hpp"
#include "pushfold/headsup/Constants.hpp"

namespace pushfold::headsup {

/**
 * @brief Generates lookup table of equities
 */
class EquityGenerator {
    std::array<std::array<float, kNumInfosets>, kNumInfosets> equities_;
    bool solved_;

   public:
    /**
     * @brief Default constructor
     */
    EquityGenerator() : equities_{}, solved_(false) {}

    /**
     * @brief Constructs from a precomputed equity table, forwarding the values
     *        into place and marking the table as already solved.
     */
    template <typename U>
        requires std::same_as<std::remove_cvref_t<U>, std::array<std::array<float, kNumInfosets>, kNumInfosets>>
    explicit EquityGenerator(U&& equities) : equities_(std::forward<U>(equities)), solved_(true) {}

    /**
     * @brief Accessor into whether we're solved
     */
    bool Ready() const { return solved_; }

    /**
     * @brief Solves for our lookup table values
     */
    void Solve() {
        if (solved_) {
            return;
        }

        // A hand has exactly 50% equity against itself.
        for (size_t i = 0; i < kNumInfosets; ++i) {
            equities_[i][i] = 0.5f;
        }

        // Fill the upper triangle by exact enumeration; the lower triangle is
        // the complement, since heads-up all-in equity is zero-sum. Each matchup
        // is independent and writes to distinct cells, so the rows can be solved
        // in parallel. Dynamic scheduling balances the work: row i has
        // kNumInfosets - 1 - i matchups, so a static split would leave low-index
        // threads idle while high-index ones finish early.
#pragma omp parallel for schedule(dynamic)
        for (size_t i = 0; i < kNumInfosets; ++i) {
            for (size_t j = i + 1; j < kNumInfosets; ++j) {
                const std::string_view hand_i = kHands[i / kNumRanks][i % kNumRanks];
                const std::string_view hand_j = kHands[j / kNumRanks][j % kNumRanks];

                // Parallelism source depends on the build: with OpenMP the loop
                // above runs matchups concurrently, so each one stays
                // single-threaded to avoid oversubscribing the cores (and paying
                // thread-pool setup 14k times). Without OpenMP the loop is serial,
                // so let each matchup fan out across all hardware threads instead.
#ifdef _OPENMP
                constexpr unsigned kThreadsPerMatchup = 1;
#else
                constexpr unsigned kThreadsPerMatchup = 0;  // 0 = all hardware threads
#endif
                omp::EquityCalculator eq;
                eq.start({omp::CardRange(std::string(hand_i)), omp::CardRange(std::string(hand_j))}, 0, 0,
                         /*enumerateAll=*/true, /*stdevTarget=*/5e-5, /*callback=*/nullptr,
                         /*updateInterval=*/0.2, /*threadCount=*/kThreadsPerMatchup);
                eq.wait();

                const float equity = static_cast<float>(eq.getResults().equity[0]);
                equities_[i][j] = equity;
                equities_[j][i] = 1.0f - equity;
            }
        }

        solved_ = true;
    }

    /**
     * @brief Returns the solved equity table, solving lazily on first access.
     */
    const std::array<std::array<float, kNumInfosets>, kNumInfosets>& Equities() {
        Solve();
        return equities_;
    }

    /**
     * @brief Converts our computed equity table to fixed point
     */
    template <typename T>
        requires(std::unsigned_integral<T> && sizeof(T) < sizeof(float))
    std::array<T, kNumEquityMatchups> Quantize() {
        Solve();
        std::array<T, kNumEquityMatchups> result;
        size_t index = 0;
        for (size_t i = 0; i < kNumInfosets; ++i) {
            for (size_t j = i + 1; j < kNumInfosets; ++j) {
                result[index] = static_cast<double>(equities_[i][j]) * std::numeric_limits<T>::max() + 0.5;
                ++index;
            }
        }
        return result;
    }

    /**
     * @brief Loads computed equity table from fixed point representation
     */
    template <typename T>
        requires(std::unsigned_integral<T> && sizeof(T) < sizeof(float))
    static EquityGenerator Dequantize(const std::array<T, kNumEquityMatchups>& values) {
        EquityGenerator result;
        result.solved_ = true;

        // A hand has exactly 50% equity against itself.
        for (size_t i = 0; i < kNumInfosets; ++i) {
            result.equities_[i][i] = 0.5f;
        }

        // The upper triangle is stored; the lower triangle is its complement,
        // mirroring the layout produced by Quantize().
        size_t index = 0;
        for (size_t i = 0; i < kNumInfosets; ++i) {
            for (size_t j = i + 1; j < kNumInfosets; ++j) {
                const float equity =
                    static_cast<float>(static_cast<double>(values[index]) / std::numeric_limits<T>::max());
                result.equities_[i][j] = equity;
                result.equities_[j][i] = 1.0f - equity;
                ++index;
            }
        }

        return result;
    }
};

}  // namespace pushfold::headsup
