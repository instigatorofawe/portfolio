#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

#include <pushfold/Infosets.hpp>

#include "pushfold/Constants.hpp"
#include "pushfold/Deck.hpp"

namespace pushfold {
namespace {

// Infoset index of a hand spelled the way kHands lays them out.
constexpr size_t Infoset(size_t row, size_t col) { return row * kNumRanks + col; }

Card Of(Rank rank, Suit suit) { return Card{rank, suit}; }

// A pocket pair lands on the diagonal regardless of card order or suits.
TEST(InfosetsTest, PairsSitOnDiagonal) {
    EXPECT_EQ(InfosetIndex(Of(Rank::Ace, Suit::Clubs), Of(Rank::Ace, Suit::Spades)), Infoset(0, 0));
    EXPECT_EQ(InfosetIndex(Of(Rank::Ace, Suit::Spades), Of(Rank::Ace, Suit::Clubs)), Infoset(0, 0));
    EXPECT_EQ(InfosetIndex(Of(Rank::Two, Suit::Hearts), Of(Rank::Two, Suit::Diamonds)), Infoset(12, 12));
}

// Suited hands go to the upper triangle, with the high card as the row.
TEST(InfosetsTest, SuitedGoesToUpperTriangle) {
    // AKs -> row Ace (0), col King (1).
    EXPECT_EQ(InfosetIndex(Of(Rank::Ace, Suit::Hearts), Of(Rank::King, Suit::Hearts)), Infoset(0, 1));
    // QJs -> row Queen (2), col Jack (3).
    EXPECT_EQ(InfosetIndex(Of(Rank::Jack, Suit::Spades), Of(Rank::Queen, Suit::Spades)), Infoset(2, 3));
}

// Offsuit hands go to the lower triangle, with the low card as the row.
TEST(InfosetsTest, OffsuitGoesToLowerTriangle) {
    // AKo -> row King (1), col Ace (0).
    EXPECT_EQ(InfosetIndex(Of(Rank::Ace, Suit::Clubs), Of(Rank::King, Suit::Hearts)), Infoset(1, 0));
    // QJo -> row Jack (3), col Queen (2).
    EXPECT_EQ(InfosetIndex(Of(Rank::Queen, Suit::Diamonds), Of(Rank::Jack, Suit::Clubs)), Infoset(3, 2));
}

// Card order must not change the resulting infoset.
TEST(InfosetsTest, OrderIndependent) {
    const Card ace = Of(Rank::Ace, Suit::Clubs);
    const Card king = Of(Rank::King, Suit::Hearts);
    EXPECT_EQ(InfosetIndex(ace, king), InfosetIndex(king, ace));

    const Card ten_s = Of(Rank::Ten, Suit::Spades);
    const Card nine_s = Of(Rank::Nine, Suit::Spades);
    EXPECT_EQ(InfosetIndex(ten_s, nine_s), InfosetIndex(nine_s, ten_s));
}

// Every card pair maps into range, and the 169 distinct infosets are all hit:
// 13 pairs + 78 suited + 78 offsuit.
TEST(InfosetsTest, CoversAllInfosets) {
    std::array<bool, kNumInfosets> seen{};
    for (uint8_t a = 0; a < kNumCards; ++a) {
        for (uint8_t b = a + 1; b < kNumCards; ++b) {
            const size_t index = InfosetIndex(*Card::FromIndex(a), *Card::FromIndex(b));
            ASSERT_LT(index, kNumInfosets);
            seen[index] = true;
        }
    }
    EXPECT_EQ(std::count(seen.begin(), seen.end(), true), kNumInfosets);
}

}  // namespace
}  // namespace pushfold
