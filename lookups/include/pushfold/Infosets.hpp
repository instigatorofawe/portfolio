#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "pushfold/Constants.hpp"
#include "pushfold/Deck.hpp"

namespace pushfold {

/**
 * @brief Maps a two-card hand to its infoset index in [0, kNumInfosets).
 *
 * Two hold'em cards collapse to one of 169 infosets (13 pairs, 78 suited, 78
 * offsuit). The index mirrors the kHands layout: pairs sit on the diagonal,
 * suited hands in the upper triangle (high card is the row), offsuit hands in
 * the lower triangle (low card is the row).
 */
inline size_t InfosetIndex(Card a, Card b) {
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

}  // namespace pushfold
