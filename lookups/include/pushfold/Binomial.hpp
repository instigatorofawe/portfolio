#pragma once

#include <cstdint>

namespace pushfold {

/**
 * @brief Computes the binomial coefficient
 *
 * @note The result and intermediate products are held in uint64_t with no
 * overflow check. The caller must keep C(n, k) below 2^64; this holds for all
 * k when n <= 62 (the largest row that fits), and for sufficiently small or
 * large k on wider rows. Passing larger values yields a silently wrapped
 * result.
 */
inline constexpr uint64_t BinomialCoefficient(uint64_t n, uint64_t k) {
    if (k > n) return 0;
    if (k > n - k) k = n - k;  // symmetry
    uint64_t result = 1;
    for (uint64_t i = 1; i <= k; i++) {
        result *= (n - k + i);
        result /= i;
    }
    return result;
}

}  // namespace pushfold
