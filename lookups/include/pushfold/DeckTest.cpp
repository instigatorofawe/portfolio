#include <cstdint>
#include <gtest/gtest.h>
#include <optional>

#include <pushfold/Deck.hpp>

#include "pushfold/Constants.hpp"

namespace pushfold {
namespace {

TEST(DeckTest, IndexZeroIsAceOfClubs) {
    Card card{Rank::Ace, Suit::Clubs};
    EXPECT_EQ(card.Index(), 0);
}

TEST(DeckTest, LastIndexIsTwoOfSpades) {
    Card card{Rank::Two, Suit::Spades};
    EXPECT_EQ(card.Index(), kNumCards - 1);
}

TEST(DeckTest, IndexLaysSuitsOutInBlocksOfThirteen) {
    // Suit is the high part of the index, rank the low part: each suit occupies a
    // contiguous block of kNumRanks indices.
    EXPECT_EQ((Card{Rank::Ace, Suit::Diamonds}).Index(), kNumRanks);
    EXPECT_EQ((Card{Rank::King, Suit::Hearts}).Index(), 2 * kNumRanks + 1);
}

TEST(DeckTest, FromIndexRejectsOutOfRange) {
    EXPECT_FALSE(Card::FromIndex(kNumCards).has_value());
    EXPECT_FALSE(Card::FromIndex(255).has_value());
}

TEST(DeckTest, FromIndexDecodesKnownCards) {
    const std::optional<Card> ace_clubs = Card::FromIndex(0);
    ASSERT_TRUE(ace_clubs.has_value());
    EXPECT_EQ(ace_clubs->rank_, Rank::Ace);
    EXPECT_EQ(ace_clubs->suit_, Suit::Clubs);

    const std::optional<Card> two_spades = Card::FromIndex(kNumCards - 1);
    ASSERT_TRUE(two_spades.has_value());
    EXPECT_EQ(two_spades->rank_, Rank::Two);
    EXPECT_EQ(two_spades->suit_, Suit::Spades);
}

// FromIndex and Index are inverses across the whole deck, and every valid index
// maps to a distinct card.
TEST(DeckTest, FromIndexIsInverseOfIndex) {
    for (uint8_t index = 0; index < kNumCards; ++index) {
        const std::optional<Card> card = Card::FromIndex(index);
        ASSERT_TRUE(card.has_value()) << "no card for index " << static_cast<int>(index);
        Card decoded = *card;
        EXPECT_EQ(decoded.Index(), index);
    }
}

}  // namespace
}  // namespace pushfold
