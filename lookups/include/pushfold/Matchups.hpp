#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "pushfold/Combinator.hpp"
#include "pushfold/Constants.hpp"
#include "pushfold/Deck.hpp"

namespace pushfold {

/**
 * @brief Generates a lookup table counting how often each infoset matchup deals.
 *
 * Two hold'em hands collapse to one of 169 infosets (13 pairs, 78 suited, 78
 * offsuit). matchups_[i][j] is the number of ordered ways to deal two disjoint
 * two-card hands where the first belongs to infoset i and the second to infoset
 * j. The matrix is symmetric, and the counts weight the per-infoset equities in
 * EquityGenerator: blocking effects mean matchups are not equiprobable, so an
 * average over infosets must be weighted by these deal counts.
 */
class MatchupGenerator {
    std::array<std::array<uint8_t, kNumInfosets>, kNumInfosets> matchups_;
    bool solved_;

    /**
     * @brief Maps a two-card hand to its infoset index in [0, kNumInfosets).
     *
     * Mirrors the kHands layout: pairs sit on the diagonal, suited hands in the
     * upper triangle (high card is the row), offsuit hands in the lower triangle.
     */
    static size_t InfosetIndex(Card a, Card b) {
        const uint8_t ra = static_cast<uint8_t>(a.rank_);
        const uint8_t rb = static_cast<uint8_t>(b.rank_);
        const uint8_t high = std::min(ra, rb);  // lower enum value == higher rank
        const uint8_t low = std::max(ra, rb);

        // Suited (necessarily distinct ranks) -> upper triangle: row is the high
        // card. Pairs and offsuit hands -> diagonal / lower triangle: row is the
        // low card.
        if (a.suit_ == b.suit_) {
            return high * kNumRanks + low;
        }
        return low * kNumRanks + high;
    }

   public:
    MatchupGenerator() : matchups_{}, solved_(false) {}

    /**
     * @brief Constructs from a precomputed matchup table, forwarding the values
     *        into place and marking the table as already solved.
     */
    template <typename U>
        requires std::same_as<std::remove_cvref_t<U>, std::array<std::array<uint8_t, kNumInfosets>, kNumInfosets>>
    explicit MatchupGenerator(U&& matchups) : matchups_(std::forward<U>(matchups)), solved_(true) {}

    bool Ready() const { return solved_; }

    /**
     * @brief Solves for the matchup-count table.
     */
    void Solve() {
        if (solved_) {
            return;
        }

        // Every disjoint pair of two-card hands comes from exactly one 4-card
        // combination split into two halves. A 4-card set has three such splits;
        // enumerating all of them over every 4-card combination visits each
        // unordered pair of hands once. We credit both orderings so the matrix
        // stays symmetric and each cell counts ordered deals.
        for (const std::array<size_t, 4>& combo : Combinator<52, 4>{}) {
            const std::array<Card, 4> cards = {
                *Card::FromIndex(static_cast<uint8_t>(combo[0])),
                *Card::FromIndex(static_cast<uint8_t>(combo[1])),
                *Card::FromIndex(static_cast<uint8_t>(combo[2])),
                *Card::FromIndex(static_cast<uint8_t>(combo[3])),
            };

            // The three ways to split four cards {0,1,2,3} into two hands.
            static constexpr std::array<std::array<size_t, 4>, 3> kSplits = {{
                {0, 1, 2, 3},
                {0, 2, 1, 3},
                {0, 3, 1, 2},
            }};

            for (const std::array<size_t, 4>& split : kSplits) {
                const size_t i = InfosetIndex(cards[split[0]], cards[split[1]]);
                const size_t j = InfosetIndex(cards[split[2]], cards[split[3]]);
                ++matchups_[i][j];
                ++matchups_[j][i];
            }
        }

        solved_ = true;
    }

    /**
     * @brief Returns the solved matchup table, solving lazily on first access.
     */
    const std::array<std::array<uint8_t, kNumInfosets>, kNumInfosets>& Matchups() {
        Solve();
        return matchups_;
    }

    /**
     * @brief Flattens the symmetric table to its upper triangle (incl. diagonal).
     *
     * The matrix is symmetric, so storing the upper triangle and the diagonal is
     * lossless. Unlike the equity table, the diagonal carries real counts (both
     * players holding the same infoset), so it is kept.
     */
    std::array<uint8_t, kNumMatchupEntries> Flatten() {
        Solve();
        std::array<uint8_t, kNumMatchupEntries> result;
        size_t index = 0;
        for (size_t i = 0; i < kNumInfosets; ++i) {
            for (size_t j = i; j < kNumInfosets; ++j) {
                result[index] = matchups_[i][j];
                ++index;
            }
        }
        return result;
    }

    /**
     * @brief Rebuilds a matchup table from its flattened upper triangle,
     *        mirroring the layout produced by Flatten().
     */
    static MatchupGenerator Unflatten(const std::array<uint8_t, kNumMatchupEntries>& values) {
        MatchupGenerator result;
        result.solved_ = true;
        size_t index = 0;
        for (size_t i = 0; i < kNumInfosets; ++i) {
            for (size_t j = i; j < kNumInfosets; ++j) {
                const uint8_t count = values[index];
                result.matchups_[i][j] = count;
                result.matchups_[j][i] = count;
                ++index;
            }
        }
        return result;
    }
};

}  // namespace pushfold
