#include <algorithm>
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <iterator>
#include <ranges>
#include <vector>

#include <pushfold/Combinator.hpp>

namespace pushfold {
namespace {

// Compile-time guarantees that Combinator slots into the std::ranges machinery
// the way a stateless, self-contained combination view should.
static_assert(std::forward_iterator<Combinator<5, 2>::Iterator>);
static_assert(std::ranges::forward_range<Combinator<5, 2>>);
static_assert(std::ranges::view<Combinator<5, 2>>);
static_assert(std::ranges::sized_range<Combinator<5, 2>>);
static_assert(std::ranges::borrowed_range<Combinator<5, 2>>);

// Collects every combination the view yields into a flat vector for inspection.
template <size_t N, size_t K>
std::vector<std::array<size_t, K>> Collect() {
    std::vector<std::array<size_t, K>> out;
    for (const auto& combination : Combinator<N, K>{}) {
        out.push_back(combination);
    }
    return out;
}

TEST(CombinatorTest, EnumeratesKnownSmallCase) {
    const std::vector<std::array<size_t, 2>> expected = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
    EXPECT_EQ((Collect<4, 2>()), expected);
}

TEST(CombinatorTest, StartsAndEndsAtTheExtremes) {
    auto combinations = Collect<6, 3>();
    ASSERT_FALSE(combinations.empty());
    EXPECT_EQ(combinations.front(), (std::array<size_t, 3>{0, 1, 2}));
    EXPECT_EQ(combinations.back(), (std::array<size_t, 3>{3, 4, 5}));
}

TEST(CombinatorTest, ProducesStrictlyAscendingIndices) {
    for (const auto& combination : Combinator<7, 4>{}) {
        EXPECT_TRUE(std::ranges::is_sorted(combination, std::ranges::less{}));
        EXPECT_TRUE(std::ranges::adjacent_find(combination) == combination.end()) << "indices must be distinct";
    }
}

TEST(CombinatorTest, EmitsInStrictLexicographicOrder) {
    auto combinations = Collect<8, 3>();
    for (size_t i = 1; i < combinations.size(); ++i) {
        EXPECT_TRUE(std::ranges::lexicographical_compare(combinations[i - 1], combinations[i]))
            << "combination " << i << " is not strictly greater than its predecessor";
    }
}

TEST(CombinatorTest, CountMatchesBinomialCoefficient) {
    EXPECT_EQ((Combinator<5, 2>::size()), 10u);
    EXPECT_EQ((Combinator<10, 3>::size()), 120u);
    EXPECT_EQ((Combinator<52, 5>::size()), 2598960u);
    // size() and the actual number of yielded combinations agree.
    EXPECT_EQ((Combinator<9, 4>::size()), (Collect<9, 4>().size()));
}

TEST(CombinatorTest, EmptySelectionYieldsOneEmptyCombination) {
    auto combinations = Collect<5, 0>();
    ASSERT_EQ(combinations.size(), 1u);
    EXPECT_TRUE(combinations.front().empty());
    EXPECT_EQ((Combinator<5, 0>::size()), 1u);
}

TEST(CombinatorTest, SelectingMoreThanAvailableYieldsNothing) {
    EXPECT_EQ((Combinator<3, 4>::size()), 0u);
    EXPECT_TRUE((Combinator<3, 4>{}.begin() == Combinator<3, 4>{}.end()));
    EXPECT_TRUE((Collect<3, 4>().empty()));
}

TEST(CombinatorTest, SelectingAllYieldsTheSingleFullSet) {
    auto combinations = Collect<4, 4>();
    ASSERT_EQ(combinations.size(), 1u);
    EXPECT_EQ(combinations.front(), (std::array<size_t, 4>{0, 1, 2, 3}));
}

TEST(CombinatorTest, InteroperatesWithRangeAlgorithms) {
    Combinator<6, 2> combinator;
    EXPECT_EQ(std::ranges::distance(combinator), static_cast<std::ptrdiff_t>(Combinator<6, 2>::size()));

    // A composed range pipeline consumes the view lazily and correctly. take_view
    // over a forward range pairs a counted_iterator with a distinct sentinel, so
    // gather it with std::ranges::to rather than a (begin, end) vector ctor.
    auto taken = combinator | std::views::take(3) | std::ranges::to<std::vector>();
    const std::vector<std::array<size_t, 2>> expected = {{0, 1}, {0, 2}, {0, 3}};
    EXPECT_EQ(taken, expected);
}

TEST(CombinatorTest, ForwardIteratorSupportsMultipass) {
    Combinator<5, 2> combinator;
    auto first = combinator.begin();
    auto second = first;
    ++second;
    // Advancing the copy must not disturb the original (multipass guarantee).
    EXPECT_EQ(*first, (std::array<size_t, 2>{0, 1}));
    EXPECT_EQ(*second, (std::array<size_t, 2>{0, 2}));
    EXPECT_TRUE(first != second);
}

}  // namespace
}  // namespace pushfold
