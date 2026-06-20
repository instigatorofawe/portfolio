#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "pushfold/Constants.hpp"

namespace pushfold {

enum class Rank : uint8_t {
    Ace = 0,
    King = 1,
    Queen = 2,
    Jack = 3,
    Ten = 4,
    Nine = 5,
    Eight = 6,
    Seven = 7,
    Six = 8,
    Five = 9,
    Four = 10,
    Three = 11,
    Two = 12
};

enum class Suit : uint8_t { Clubs = 0, Diamonds = 1, Hearts = 2, Spades = 3 };

/**
 * @brief A card from a 52-card poker deck
 */
struct Card {
    Rank rank_;
    Suit suit_;

    /**
     * @brief Tries to convert an index to a card; nullopt if none exists
     */
    static std::optional<Card> FromIndex(uint8_t index) {
        if (index >= kNumCards) {
            return std::nullopt;
        }
        return Card{static_cast<Rank>(index % kNumRanks), static_cast<Suit>(index / kNumRanks)};
    }

    /**
     * @brief Gets the index for a given card
     */
    constexpr uint8_t Index() { return static_cast<uint8_t>(suit_) * kNumRanks + static_cast<uint8_t>(rank_); }
};

}  // namespace pushfold
