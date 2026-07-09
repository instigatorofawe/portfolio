#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "pushfold/Binomial.hpp"

namespace pushfold::threeway {

/**
 * @brief Flattens an unordered multiset of three infoset indices to a dense,
 *        contiguous flat index.
 *
 * A three-way deal collapses to an unordered triple of infosets chosen with
 * replacement (two — or all three — players can share an infoset). Sorting the
 * triple ascending and shifting each element by its position turns the
 * weakly-increasing triple {h0 <= h1 <= h2} into a strictly-increasing one
 * {h0 < h1+1 < h2+2}, so the combinatorial number system (colexicographic
 * ranking) applies: the rank is the sum of C(h_i + i, i + 1) over the three
 * positions.
 *
 * The result is contiguous over [0, C(M + 2, 3)) when every index is drawn from
 * [0, M), so it directly addresses a flat lookup table. The order of the three
 * arguments does not matter.
 */
inline size_t ColexIndex(size_t a, size_t b, size_t c) {
    std::array<size_t, 3> hands = {a, b, c};
    std::ranges::sort(hands);  // weakly increasing: hands[0] <= hands[1] <= hands[2]

    size_t index = 0;
    for (size_t i = 0; i < hands.size(); ++i) {
        // Shift by the position (i) to strictify, then take the colex term. The
        // i == 0 term is just hands[0]; the higher terms vanish while an element
        // is smaller than its position + 1, exactly as the number system needs.
        index += static_cast<size_t>(BinomialCoefficient(hands[i] + i, i + 1));
    }
    return index;
}

/**
 * @brief Recovers the sorted infoset triple that ColexIndex maps to a flat index.
 *
 * The inverse of ColexIndex, used when generating flat tables: it walks the flat
 * index space and needs the infoset multiset behind each slot. It unranks the
 * combinatorial number system greedily from the top position down — for each
 * position i (largest first) it takes the greatest strictified element whose
 * colex term C(element, i + 1) still fits in what remains — then undoes the
 * position shift to return the weakly-increasing triple {h0 <= h1 <= h2}.
 *
 * `index` must be < C(M + 2, 3) for the intended alphabet size M; the result is
 * exactly the (sorted) arguments any ColexIndex call producing `index` was given.
 */
inline std::array<size_t, 3> ColexTuple(size_t index) {
    std::array<size_t, 3> hands{};
    size_t remaining = index;
    for (size_t i = 3; i-- > 0;) {  // positions 2, 1, 0
        const size_t choose = i + 1;
        // The colex term is zero until the element reaches its position i, so
        // start there and climb while the next term still fits.
        size_t element = i;
        while (BinomialCoefficient(element + 1, choose) <= remaining) {
            ++element;
        }
        remaining -= static_cast<size_t>(BinomialCoefficient(element, choose));
        hands[i] = element - i;  // undo the position shift
    }
    return hands;
}

}  // namespace pushfold::threeway
