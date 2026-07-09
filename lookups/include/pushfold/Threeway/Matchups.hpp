#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "pushfold/Constants.hpp"
#include "pushfold/Deck.hpp"
#include "pushfold/Infosets.hpp"
#include "pushfold/Threeway/Colex.hpp"
#include "pushfold/Threeway/Constants.hpp"
#include "pushfold/Threeway/Multiset.hpp"

namespace pushfold::threeway {

/**
 * @brief Generates a lookup table counting how often each three-way infoset
 *        matchup deals.
 *
 * A three-way deal collapses to an unordered multiset of three infosets, indexed
 * by its colex rank (see Colex.hpp). matchups_[ColexIndex(i, j, k)] is the number
 * of ways to deal three disjoint two-card hands to a fixed ordered triple of
 * seats belonging to infosets i, j, k — that is, one seating, not the unordered
 * deal. Storing the per-seating count lets the solver weight any ordered matchup
 * by the slot directly, mirroring the heads-up matchup table. Blocking effects
 * make matchups non-equiprobable, so equity averages must be weighted by these
 * counts.
 *
 * The counts sum to Multiset<kNumCards>::size() * 3! = C(52,2)*C(50,2)*C(48,2),
 * the total number of seat-ordered three-way deals. The largest entry is still
 * 12^3 = 1728 (three disjoint offsuit infosets with no shared ranks, one seating
 * each), so uint16_t holds every count with room to spare.
 */
class MatchupGenerator {
    using Table = std::array<uint16_t, kNumMatchupEntries>;

    std::unique_ptr<Table> matchups_;
    bool solved_;

    void IncrementDeal(const Multiset<kNumCards>::Deal& deal) {
        const auto infoset = [](const Multiset<kNumCards>::Hand& hand) {
            return InfosetIndex(*Card::FromIndex(static_cast<uint8_t>(hand[0])),
                                *Card::FromIndex(static_cast<uint8_t>(hand[1])));
        };
        const size_t i = infoset(deal[0]);
        const size_t j = infoset(deal[1]);
        const size_t k = infoset(deal[2]);

        // Multiset enumerates this deal once as an unordered triple of hands, but
        // the table is keyed per seating. The three hands are always physically
        // distinct, so the deal represents 3! = 6 seatings; those collapse into
        // 6 / seatings distinct ordered infoset triples (fewer when infosets
        // repeat), all sharing this colex slot. Crediting the per-seating count
        // keeps every ordered permutation's slot equal to its own frequency.
        const size_t distinct_orderings = (i == j && j == k) ? 1 : (i == j || j == k || i == k) ? 3 : 6;
        (*matchups_)[ColexIndex(i, j, k)] += static_cast<uint16_t>(6 / distinct_orderings);
    }

   public:
    MatchupGenerator() : matchups_(std::make_unique<Table>()), solved_(false) {}

    /**
     * @brief Constructs from a precomputed matchup table, forwarding the values
     *        into place and marking the table as already solved.
     */
    template <typename U>
        requires std::same_as<std::remove_cvref_t<U>, Table>
    explicit MatchupGenerator(U&& matchups)
        : matchups_(std::make_unique<Table>(std::forward<U>(matchups))), solved_(true) {}

    bool Ready() const { return solved_; }

    /**
     * @brief Solves for the matchup-count table.
     *
     * Multiset walks every unordered triple of disjoint two-card hands exactly
     * once; each is credited to the slot ColexIndex assigns its infoset triple,
     * weighted by its seatings (see IncrementDeal). Because ColexIndex sorts
     * before ranking, the three players' infosets fold into one canonical slot
     * regardless of deal order — the table is symmetric by construction.
     */
    void Solve() {
        if (solved_) {
            return;
        }

        for (const Multiset<kNumCards>::Deal& deal : Multiset<kNumCards>{}) {
            IncrementDeal(deal);
        }

        solved_ = true;
    }

    /**
     * @brief Returns the solved matchup table, solving lazily on first access.
     *
     * The table is indexed by colex rank, so it is already the dense flat form
     * ready to emit — no separate flatten step (unlike the symmetric heads-up
     * matrix).
     */
    const Table& Matchups() {
        Solve();
        return *matchups_;
    }
};

}  // namespace pushfold::threeway
