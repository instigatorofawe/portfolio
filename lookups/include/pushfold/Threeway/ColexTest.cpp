#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <set>
#include <vector>

#include <pushfold/Constants.hpp>
#include <pushfold/Threeway/Colex.hpp>
#include <pushfold/Threeway/Constants.hpp>

namespace pushfold::threeway {
namespace {

// Enumerates the colex ranks of every multiset of three indices drawn from
// [0, M), in the weakly-increasing order the colex ranking follows.
template <size_t M>
std::vector<size_t> RanksInOrder() {
    std::vector<size_t> ranks;
    for (size_t c = 0; c < M; ++c) {
        for (size_t b = 0; b <= c; ++b) {
            for (size_t a = 0; a <= b; ++a) {
                ranks.push_back(ColexIndex(a, b, c));
            }
        }
    }
    return ranks;
}

TEST(ColexTest, MatchesHandComputedSmallCase) {
    // The ten multisets of three drawn from {0, 1, 2}, in colex order.
    EXPECT_EQ(ColexIndex(0, 0, 0), 0u);
    EXPECT_EQ(ColexIndex(0, 0, 1), 1u);
    EXPECT_EQ(ColexIndex(0, 1, 1), 2u);
    EXPECT_EQ(ColexIndex(1, 1, 1), 3u);
    EXPECT_EQ(ColexIndex(0, 0, 2), 4u);
    EXPECT_EQ(ColexIndex(0, 1, 2), 5u);
    EXPECT_EQ(ColexIndex(1, 1, 2), 6u);
    EXPECT_EQ(ColexIndex(0, 2, 2), 7u);
    EXPECT_EQ(ColexIndex(1, 2, 2), 8u);
    EXPECT_EQ(ColexIndex(2, 2, 2), 9u);
}

TEST(ColexTest, IgnoresArgumentOrder) {
    // Every permutation of the same three indices maps to one flat index.
    const std::array<std::array<size_t, 3>, 6> permutations = {{
        {3, 7, 40},
        {3, 40, 7},
        {7, 3, 40},
        {7, 40, 3},
        {40, 3, 7},
        {40, 7, 3},
    }};
    const size_t expected = ColexIndex(3, 7, 40);
    for (const auto& p : permutations) {
        EXPECT_EQ(ColexIndex(p[0], p[1], p[2]), expected);
    }

    // Repeated indices (with-replacement selection) are handled the same way.
    EXPECT_EQ(ColexIndex(5, 5, 9), ColexIndex(9, 5, 5));
    EXPECT_EQ(ColexIndex(5, 5, 9), ColexIndex(5, 9, 5));
}

TEST(ColexTest, IsAContiguousBijectionOverTheMultisets) {
    // Over [0, 8), the ranks are exactly {0, ..., C(10, 3) - 1} with no gaps or
    // collisions, so the mapping is a dense bijection onto the flat table.
    auto ranks = RanksInOrder<8>();
    const std::set<size_t> distinct(ranks.begin(), ranks.end());
    ASSERT_EQ(distinct.size(), ranks.size()) << "no two multisets share a flat index";
    EXPECT_EQ(*distinct.begin(), 0u);
    EXPECT_EQ(*distinct.rbegin(), ranks.size() - 1u) << "ranks fill [0, count) with no gaps";
    EXPECT_EQ(ranks.size(), 120u);  // C(8 + 2, 3) = 120
}

TEST(ColexTest, EmitsInStrictlyAscendingColexOrder) {
    // Walking the multisets in colex order (largest element outermost) yields
    // strictly increasing ranks.
    auto ranks = RanksInOrder<12>();
    for (size_t i = 1; i < ranks.size(); ++i) {
        EXPECT_EQ(ranks[i], ranks[i - 1] + 1) << "rank " << i << " is not the immediate successor";
    }
}

TEST(ColexTest, SpansTheFullInfosetTable) {
    // With real infoset indices the ranks cover exactly [0, kNumMatchupEntries).
    EXPECT_EQ(kNumMatchupEntries, 818805u);  // C(169 + 2, 3)
    EXPECT_EQ(ColexIndex(0, 0, 0), 0u);
    EXPECT_EQ(ColexIndex(kNumInfosets - 1, kNumInfosets - 1, kNumInfosets - 1), kNumMatchupEntries - 1u);
}

TEST(ColexTest, TupleRecoversHandComputedSmallCase) {
    // The inverse of the multisets in MatchesHandComputedSmallCase.
    EXPECT_EQ(ColexTuple(0), (std::array<size_t, 3>{0, 0, 0}));
    EXPECT_EQ(ColexTuple(1), (std::array<size_t, 3>{0, 0, 1}));
    EXPECT_EQ(ColexTuple(2), (std::array<size_t, 3>{0, 1, 1}));
    EXPECT_EQ(ColexTuple(3), (std::array<size_t, 3>{1, 1, 1}));
    EXPECT_EQ(ColexTuple(4), (std::array<size_t, 3>{0, 0, 2}));
    EXPECT_EQ(ColexTuple(5), (std::array<size_t, 3>{0, 1, 2}));
    EXPECT_EQ(ColexTuple(9), (std::array<size_t, 3>{2, 2, 2}));
}

TEST(ColexTest, TupleIsSortedAscending) {
    // Every recovered triple is a weakly-increasing multiset, the canonical form.
    for (size_t index = 0; index < 120u; ++index) {  // C(8 + 2, 3) = 120
        auto tuple = ColexTuple(index);
        EXPECT_LE(tuple[0], tuple[1]) << "at index " << index;
        EXPECT_LE(tuple[1], tuple[2]) << "at index " << index;
    }
}

TEST(ColexTest, TupleInvertsIndexOverTheWholeTable) {
    // index -> tuple -> index round-trips across the entire flat table.
    for (size_t index = 0; index < kNumMatchupEntries; ++index) {
        auto tuple = ColexTuple(index);
        EXPECT_EQ(ColexIndex(tuple[0], tuple[1], tuple[2]), index);
    }
}

TEST(ColexTest, IndexInvertsTupleForEveryMultiset) {
    // tuple -> index -> tuple round-trips for every multiset drawn from [0, 20).
    for (size_t c = 0; c < 20u; ++c) {
        for (size_t b = 0; b <= c; ++b) {
            for (size_t a = 0; a <= b; ++a) {
                EXPECT_EQ(ColexTuple(ColexIndex(a, b, c)), (std::array<size_t, 3>{a, b, c}));
            }
        }
    }
}

}  // namespace
}  // namespace pushfold::threeway
