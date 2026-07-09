#include <gtest/gtest.h>

#include <pushfold/Binomial.hpp>

namespace pushfold {
namespace {

TEST(BinomialTest, KnownValues) {
    constexpr size_t kNumExamples = 4;
    uint64_t n = 10;
    uint64_t k[kNumExamples] = {2, 3, 4, 5};
    uint64_t solutions[kNumExamples] = {45, 120, 210, 252};

    for (size_t i = 0; i < kNumExamples; i++) {
        EXPECT_EQ(BinomialCoefficient(n, k[i]), solutions[i]);
    }
}

TEST(BinomialTest, Symmetry) { EXPECT_EQ(BinomialCoefficient(10, 4), BinomialCoefficient(10, 6)); }

// C(n, k) == C(n, n - k) across an entire row, exercising the symmetry
// reduction on line `if (k > n - k) k = n - k;` for both the reduced and
// unreduced branches.
TEST(BinomialTest, SymmetryAcrossRow) {
    constexpr uint64_t n = 20;
    for (uint64_t k = 0; k <= n; k++) {
        EXPECT_EQ(BinomialCoefficient(n, k), BinomialCoefficient(n, n - k)) << "k = " << k;
    }
}

// C(n, 0) == C(n, n) == 1 for every n, covering the loop's zero-iteration base
// case at both ends of a row.
TEST(BinomialTest, Identities) {
    for (uint64_t n = 0; n <= 30; n++) {
        EXPECT_EQ(BinomialCoefficient(n, 0), 1u) << "n = " << n;
        EXPECT_EQ(BinomialCoefficient(n, n), 1u) << "n = " << n;
        if (n >= 1) {
            EXPECT_EQ(BinomialCoefficient(n, 1), n) << "n = " << n;
            EXPECT_EQ(BinomialCoefficient(n, n - 1), n) << "n = " << n;
        }
    }
}

// The empty domain: C(0, 0) is 1, and any positive k over n == 0 is 0.
TEST(BinomialTest, ZeroN) {
    EXPECT_EQ(BinomialCoefficient(0, 0), 1u);
    EXPECT_EQ(BinomialCoefficient(0, 1), 0u);
    EXPECT_EQ(BinomialCoefficient(0, 5), 0u);
}

// k > n returns 0, covering the early-out guard on line `if (k > n) return 0;`.
TEST(BinomialTest, KGreaterThanN) {
    EXPECT_EQ(BinomialCoefficient(5, 6), 0u);
    EXPECT_EQ(BinomialCoefficient(10, 11), 0u);
    EXPECT_EQ(BinomialCoefficient(3, 100), 0u);
}

// Pascal's rule: C(n, k) == C(n-1, k-1) + C(n-1, k). A structural property that
// pins down every interior entry of the triangle.
TEST(BinomialTest, PascalsRule) {
    for (uint64_t n = 1; n <= 30; n++) {
        for (uint64_t k = 1; k < n; k++) {
            EXPECT_EQ(BinomialCoefficient(n, k), BinomialCoefficient(n - 1, k - 1) + BinomialCoefficient(n - 1, k))
                << "n = " << n << ", k = " << k;
        }
    }
}

// Each row sums to 2^n, catching any systematic off-by-one across a full row.
TEST(BinomialTest, RowSum) {
    for (uint64_t n = 0; n <= 62; n++) {
        uint64_t sum = 0;
        for (uint64_t k = 0; k <= n; k++) {
            sum += BinomialCoefficient(n, k);
        }
        EXPECT_EQ(sum, uint64_t{1} << n) << "n = " << n;
    }
}

// Large inputs that stay within uint64_t. C(62, 31) is the largest central
// coefficient below 2^63, so the intermediate `result *= (n - k + i)` before the
// division must not have overflowed to produce it.
TEST(BinomialTest, LargeValues) {
    EXPECT_EQ(BinomialCoefficient(52, 5), 2598960u);  // 5-card poker hands
    EXPECT_EQ(BinomialCoefficient(50, 25), 126410606437752ull);
    EXPECT_EQ(BinomialCoefficient(62, 31), 465428353255261088ull);
}

// The coefficient is declared constexpr; confirm it is usable in a constant
// expression so callers can index compile-time lookup tables with it.
TEST(BinomialTest, Constexpr) {
    static_assert(BinomialCoefficient(10, 3) == 120);
    static_assert(BinomialCoefficient(52, 2) == 1326);
    static_assert(BinomialCoefficient(5, 6) == 0);
    static_assert(BinomialCoefficient(0, 0) == 1);
    SUCCEED();
}

}  // namespace

}  // namespace pushfold
