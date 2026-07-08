#include <array>
#include <cstdint>
#include <gtest/gtest.h>

#include <pushfold/headsup/Matchups.hpp>

#include "pushfold/Constants.hpp"

namespace pushfold::headsup {
namespace {

// Infoset index of a hand spelled the way kHands lays them out.
constexpr size_t Infoset(size_t row, size_t col) { return row * kNumRanks + col; }

TEST(MatchupsTest, DefaultConstructedIsNotReady) {
    MatchupGenerator generator;
    EXPECT_FALSE(generator.Ready());
}

TEST(MatchupsTest, SolveMarksReady) {
    MatchupGenerator generator;
    generator.Solve();
    EXPECT_TRUE(generator.Ready());
}

TEST(MatchupsTest, MatchupsIsSymmetric) {
    MatchupGenerator generator;
    const auto& matchups = generator.Matchups();
    for (size_t i = 0; i < kNumInfosets; ++i) {
        for (size_t j = 0; j < kNumInfosets; ++j) {
            EXPECT_EQ(matchups[i][j], matchups[j][i]) << "asymmetry at (" << i << ", " << j << ")";
        }
    }
}

// Every 4-card combination contributes three splits, each crediting two ordered
// deals, so the whole table sums to C(52, 4) * 3 * 2.
TEST(MatchupsTest, TotalEqualsAllOrderedDeals) {
    MatchupGenerator generator;
    const auto& matchups = generator.Matchups();
    uint64_t total = 0;
    for (size_t i = 0; i < kNumInfosets; ++i) {
        for (size_t j = 0; j < kNumInfosets; ++j) {
            total += matchups[i][j];
        }
    }
    EXPECT_EQ(total, static_cast<uint64_t>(270725) * 3 * 2);
}

// Two disjoint-rank offsuit hands: 12 combos each, no card conflicts -> 144.
TEST(MatchupsTest, OffsuitVersusOffsuitDisjointRanks) {
    MatchupGenerator generator;
    const auto& matchups = generator.Matchups();
    const size_t ako = Infoset(1, 0);  // offsuit, lower triangle
    const size_t qjo = Infoset(3, 2);
    EXPECT_EQ(matchups[ako][qjo], 144);
}

// Both players pocket aces: the four aces split into two hands three ways,
// counted in both orders -> 6.
TEST(MatchupsTest, PocketAcesMirror) {
    MatchupGenerator generator;
    const auto& matchups = generator.Matchups();
    const size_t aa = Infoset(0, 0);
    EXPECT_EQ(matchups[aa][aa], 6);
}

TEST(MatchupsTest, ConstructedFromTableIsReady) {
    std::array<std::array<uint8_t, kNumInfosets>, kNumInfosets> table{};
    MatchupGenerator generator(table);
    EXPECT_TRUE(generator.Ready());
}

TEST(MatchupsTest, FlattenHasEntryPerUpperTriangleCell) {
    MatchupGenerator generator;
    const auto flat = generator.Flatten();
    EXPECT_EQ(flat.size(), kNumMatchupEntries);
}

TEST(MatchupsTest, FlattenUnflattenRoundTrip) {
    MatchupGenerator solved;
    const auto& original = solved.Matchups();

    MatchupGenerator restored = MatchupGenerator::Unflatten(solved.Flatten());
    ASSERT_TRUE(restored.Ready());
    const auto& recovered = restored.Matchups();

    for (size_t i = 0; i < kNumInfosets; ++i) {
        for (size_t j = 0; j < kNumInfosets; ++j) {
            EXPECT_EQ(recovered[i][j], original[i][j]) << "mismatch at (" << i << ", " << j << ")";
        }
    }
}

}  // namespace
}  // namespace pushfold::headsup
