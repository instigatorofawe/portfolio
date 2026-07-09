#include <algorithm>
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <iterator>
#include <ranges>
#include <set>
#include <vector>

#include <pushfold/Threeway/Multiset.hpp>

namespace pushfold::threeway {
namespace {

// Compile-time guarantees that Multiset slots into the std::ranges machinery the
// way a stateless, self-contained deal view should. The reference type is a
// prvalue (deals are materialized on dereference), yet iterator_concept still
// carries the forward guarantee, exactly as iota_view does.
static_assert(std::forward_iterator<Multiset<6>::Iterator>);
static_assert(std::ranges::forward_range<Multiset<6>>);
static_assert(std::ranges::view<Multiset<6>>);
static_assert(std::ranges::sized_range<Multiset<6>>);
static_assert(std::ranges::borrowed_range<Multiset<6>>);

using Deal = Multiset<6>::Deal;

// Collects every deal the view yields into a flat vector for inspection.
template <size_t N>
std::vector<typename Multiset<N>::Deal> Collect() {
    std::vector<typename Multiset<N>::Deal> out;
    for (const auto& deal : Multiset<N>{}) {
        out.push_back(deal);
    }
    return out;
}

// Canonicalizes a deal to a form that is invariant under reordering hands or the
// two cards within a hand, so two deals that describe the same unordered triple
// of hands compare equal regardless of how they were emitted.
template <size_t N>
std::array<std::array<size_t, 2>, 3> Canonical(const typename Multiset<N>::Deal& deal) {
    std::array<std::array<size_t, 2>, 3> canonical;
    for (size_t i = 0; i < 3; ++i) {
        canonical[i] = {deal[i][0], deal[i][1]};
        std::ranges::sort(canonical[i]);
    }
    std::ranges::sort(canonical);
    return canonical;
}

TEST(MultisetTest, SixCardsYieldsTheFifteenPartitions) {
    auto deals = Collect<6>();
    ASSERT_EQ(deals.size(), 15u);

    // With exactly six cards every deal is a partition of {0, ..., 5}, and the
    // fifteen are distinct.
    std::set<std::array<std::array<size_t, 2>, 3>> distinct;
    for (const auto& deal : deals) {
        std::array<size_t, 6> flat = {deal[0][0], deal[0][1], deal[1][0], deal[1][1], deal[2][0], deal[2][1]};
        std::ranges::sort(flat);
        EXPECT_TRUE(std::ranges::equal(flat, std::views::iota(size_t{0}, size_t{6}))) << "a deal must cover 0..5 once";
        distinct.insert(Canonical<6>(deal));
    }
    EXPECT_EQ(distinct.size(), 15u);
}

TEST(MultisetTest, EmitsCanonicallyOrderedDeals) {
    for (const auto& deal : Multiset<8>{}) {
        for (const auto& hand : deal) {
            EXPECT_LT(hand[0], hand[1]) << "cards within a hand must ascend and be distinct";
        }
        EXPECT_LT(deal[0][0], deal[1][0]) << "hands must be ordered by their first card";
        EXPECT_LT(deal[1][0], deal[2][0]) << "hands must be ordered by their first card";
    }
}

TEST(MultisetTest, HandsAreDisjointAndInRange) {
    for (const auto& deal : Multiset<9>{}) {
        std::array<size_t, 6> cards = {deal[0][0], deal[0][1], deal[1][0], deal[1][1], deal[2][0], deal[2][1]};
        for (size_t card : cards) {
            EXPECT_LT(card, 9u);
        }
        std::ranges::sort(cards);
        EXPECT_TRUE(std::ranges::adjacent_find(cards) == cards.end()) << "the six cards must be distinct";
    }
}

TEST(MultisetTest, ProducesEveryUnorderedTripleExactlyOnce) {
    auto deals = Collect<8>();
    std::set<std::array<std::array<size_t, 2>, 3>> distinct;
    for (const auto& deal : deals) {
        distinct.insert(Canonical<8>(deal));
    }
    // No two emitted deals collapse to the same unordered triple of hands.
    EXPECT_EQ(distinct.size(), deals.size());
}

TEST(MultisetTest, CountMatchesCombinationsTimesPartitions) {
    // C(N, 6) * 15.
    EXPECT_EQ((Multiset<6>::size()), 15u);
    EXPECT_EQ((Multiset<7>::size()), 7u * 15u);     // C(7,6) = 7
    EXPECT_EQ((Multiset<8>::size()), 28u * 15u);    // C(8,6) = 28
    EXPECT_EQ((Multiset<52>::size()), 305377800u);  // C(52,6) = 20358520
    // size() and the actual number of yielded deals agree.
    EXPECT_EQ((Multiset<9>::size()), (Collect<9>().size()));
}

TEST(MultisetTest, FewerThanSixCardsYieldsNothing) {
    EXPECT_EQ((Multiset<5>::size()), 0u);
    EXPECT_TRUE((Multiset<5>{}.begin() == Multiset<5>{}.end()));
    EXPECT_TRUE((Collect<5>().empty()));
}

TEST(MultisetTest, StartsAtTheLexicographicallySmallestDeal) {
    auto deals = Collect<6>();
    ASSERT_FALSE(deals.empty());
    const Deal expected = {{{0, 1}, {2, 3}, {4, 5}}};
    EXPECT_EQ(deals.front(), expected);
}

TEST(MultisetTest, InteroperatesWithRangeAlgorithms) {
    Multiset<7> multiset;
    EXPECT_EQ(std::ranges::distance(multiset), static_cast<std::ptrdiff_t>(Multiset<7>::size()));

    // A composed pipeline consumes the view lazily. take_view over a forward
    // range pairs a counted_iterator with a distinct sentinel, so gather it with
    // std::ranges::to rather than a (begin, end) vector ctor.
    auto taken = multiset | std::views::take(2) | std::ranges::to<std::vector>();
    const std::vector<Multiset<7>::Deal> expected = {
        {{{0, 1}, {2, 3}, {4, 5}}},
        {{{0, 1}, {2, 4}, {3, 5}}},
    };
    EXPECT_EQ(taken, expected);
}

TEST(MultisetTest, ForwardIteratorSupportsMultipass) {
    Multiset<6> multiset;
    auto first = multiset.begin();
    auto second = first;
    ++second;
    // Advancing the copy must not disturb the original (multipass guarantee).
    EXPECT_EQ(*first, (Deal{{{0, 1}, {2, 3}, {4, 5}}}));
    EXPECT_EQ(*second, (Deal{{{0, 1}, {2, 4}, {3, 5}}}));
    EXPECT_TRUE(first != second);
}

}  // namespace
}  // namespace pushfold::threeway
