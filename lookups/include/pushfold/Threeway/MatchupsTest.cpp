#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

#include <pushfold/Threeway/Colex.hpp>
#include <pushfold/Threeway/Matchups.hpp>
#include <pushfold/Threeway/Multiset.hpp>

#include "pushfold/Constants.hpp"

namespace pushfold::threeway {
namespace {

// Infoset index of a hand spelled the way kHands lays them out: pairs on the
// diagonal, suited hands in the upper triangle (high card is the row).
constexpr size_t Pair(size_t rank) { return rank * kNumRanks + rank; }
constexpr size_t Suited(size_t high, size_t low) { return high * kNumRanks + low; }
constexpr size_t Offsuit(size_t high, size_t low) { return low * kNumRanks + high; }

// Solving walks every three-way deal, so it is far too expensive to repeat per
// test. The fixture solves a single generator once and shares it across the
// suite.
class ThreewayMatchupsTest : public testing::Test {
   protected:
    static void SetUpTestSuite() {
        if (generator_ == nullptr) {
            generator_ = new MatchupGenerator();
            generator_->Solve();
        }
    }

    static void TearDownTestSuite() {
        delete generator_;
        generator_ = nullptr;
    }

    static const auto& Matchups() { return generator_->Matchups(); }

    // Deal count for the infoset triple {a, b, c}, addressed through its colex
    // rank the same way the generator fills the flat table.
    static uint16_t At(size_t a, size_t b, size_t c) { return Matchups()[ColexIndex(a, b, c)]; }

    static MatchupGenerator* generator_;
};

MatchupGenerator* ThreewayMatchupsTest::generator_ = nullptr;

// A matchup count depends only on the multiset of infosets, not on which seat
// holds which. The flat table stores one canonical (colex) slot per multiset,
// so this symmetry is structural; the test guards that a lookup stays
// order-independent for a handful of triples spanning pair/suited/offsuit
// shapes. (Repeated infosets are symmetric trivially, so they would not
// exercise the property.)
TEST_F(ThreewayMatchupsTest, IsSymmetricUnderPermutation) {
    const std::array<std::array<size_t, 3>, 4> triples = {{
        {Pair(0), Pair(1), Pair(2)},                 // AA, KK, QQ
        {Suited(0, 1), Suited(2, 3), Suited(4, 5)},  // AKs, QJs, T9s
        {Offsuit(0, 1), Offsuit(2, 3), Offsuit(4, 5)},
        {Pair(0), Suited(1, 2), Offsuit(3, 4)},  // mixed shapes
    }};

    for (const auto& t : triples) {
        const size_t a = t[0], b = t[1], c = t[2];
        const uint16_t expected = At(a, b, c);
        const std::array<std::array<size_t, 3>, 5> others = {{
            {a, c, b},
            {b, a, c},
            {b, c, a},
            {c, a, b},
            {c, b, a},
        }};
        for (const auto& p : others) {
            EXPECT_EQ(At(p[0], p[1], p[2]), expected) << "asymmetry: (" << a << ", " << b << ", " << c << ") vs ("
                                                      << p[0] << ", " << p[1] << ", " << p[2] << ")";
        }
    }
}

// Each slot holds the count for one seating, so expanding it over the ordered
// infoset triples that share it (6 when all distinct, 3 with one repeat, 1 when
// all equal) and summing must recover every seat-ordered deal: each of the
// Multiset<52>::size() unordered deals stands for 3! seatings. Equivalently
// C(52,2)*C(50,2)*C(48,2). This mirrors how the solver expands the colex table
// and guards that seatings are credited without dropping or double-counting.
TEST_F(ThreewayMatchupsTest, SeatOrderedDealsExpandToAllOrderedDeals) {
    const auto& matchups = Matchups();
    uint64_t total = 0;
    for (size_t index = 0; index < kNumMatchupEntries; ++index) {
        const auto t = ColexTuple(index);
        const uint64_t orderings = (t[0] == t[1] && t[1] == t[2])                   ? 1
                                   : (t[0] == t[1] || t[1] == t[2] || t[0] == t[2]) ? 3
                                                                                    : 6;
        total += uint64_t{matchups[index]} * orderings;
    }
    EXPECT_EQ(total, Multiset<kNumCards>::size() * 6);
    EXPECT_EQ(total, uint64_t{1832266800});  // C(52,2) * C(50,2) * C(48,2)
}

// Three players cannot all hold the same pocket pair: a rank has only four
// cards, not the six a three-way pair-versus-pair deal would need.
TEST_F(ThreewayMatchupsTest, IdenticalPocketPairsNeverDeal) {
    for (size_t rank = 0; rank < kNumRanks; ++rank) {
        const size_t pair = Pair(rank);
        EXPECT_EQ(At(pair, pair, pair), 0u) << "pocket pair rank " << rank;
    }
}

// Three identical suited hands need three distinct suits for the same rank pair:
// C(4, 3) = 4 unordered deals, and with all three infosets equal every seating
// is distinct, so 4 * 3! = 24 seat-ordered combos.
TEST_F(ThreewayMatchupsTest, IdenticalSuitedHandsDealTwentyFourCombos) {
    for (size_t high = 0; high < kNumRanks; ++high) {
        for (size_t low = high + 1; low < kNumRanks; ++low) {
            const size_t suited = Suited(high, low);
            EXPECT_EQ(At(suited, suited, suited), 24u) << "suited hand (" << high << ", " << low << ")";
        }
    }
}

}  // namespace
}  // namespace pushfold::threeway
