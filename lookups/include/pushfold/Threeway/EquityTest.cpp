#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>

#include <pushfold/Threeway/Colex.hpp>
#include <pushfold/Threeway/Constants.hpp>
#include <pushfold/Threeway/Equity.hpp>

namespace pushfold::threeway {
namespace {

// The flat equity table: one triplet (pot share per hand) per colex matchup,
// ordered to match ColexTuple. Mirrors the private layout EquityGenerator holds.
using EquityTable = std::array<std::array<float, 3>, kNumMatchupEntries>;

// A cheap synthetic table (no OMPEval): each slot carries a distinct,
// deterministic triplet that is a valid pot split (non-negative, sums to 1), so
// read-back is unambiguous. Heap-allocated because the table is several
// megabytes.
std::unique_ptr<EquityTable> MakeSyntheticTable() {
    auto table = std::make_unique<EquityTable>();
    for (size_t index = 0; index < kNumMatchupEntries; ++index) {
        const float base = static_cast<float>(index % 1000) / 1000.0f;
        const float e0 = base * 0.4f;
        const float e1 = base * 0.35f;
        (*table)[index] = {e0, e1, 1.0f - e0 - e1};
    }
    return table;
}

TEST(ThreewayEquityTest, DefaultConstructedIsNotReady) {
    EquityGenerator generator;
    EXPECT_FALSE(generator.Ready());
}

TEST(ThreewayEquityTest, ConstructedFromTableIsReady) {
    auto table = MakeSyntheticTable();
    EquityGenerator generator(*table);
    EXPECT_TRUE(generator.Ready());
}

// A generator built from a precomputed table hands it straight back, without
// invoking the (expensive) solver.
TEST(ThreewayEquityTest, EquitiesReturnsConstructedTable) {
    auto table = MakeSyntheticTable();
    EquityGenerator generator(*table);

    const EquityTable& recovered = generator.Equities();
    for (size_t index = 0; index < kNumMatchupEntries; ++index) {
        EXPECT_EQ(recovered[index], (*table)[index]) << "mismatch at index " << index;
    }
}

// Quantizing keeps the first two shares and drops the third; Dequantize scales
// the two back and rebuilds the third as the remainder. Both stored shares
// round-trip within half a level, and the third accumulates at most both errors.
TEST(ThreewayEquityTest, QuantizeRoundTrip) {
    auto table = MakeSyntheticTable();
    EquityGenerator generator(*table);

    const auto quantized = generator.Quantize();
    EquityGenerator restored = EquityGenerator::Dequantize(*quantized);
    ASSERT_TRUE(restored.Ready());

    // Half a level for each stored share, and the third carries both; a little
    // slack covers the float casts.
    constexpr float kLevel = 1.0f / static_cast<float>(std::numeric_limits<uint16_t>::max());
    const EquityTable& original = *table;
    const EquityTable& recovered = restored.Equities();
    for (size_t index = 0; index < kNumMatchupEntries; ++index) {
        EXPECT_NEAR(recovered[index][0], original[index][0], 0.5f * kLevel + 1e-6f) << "share 0 at " << index;
        EXPECT_NEAR(recovered[index][1], original[index][1], 0.5f * kLevel + 1e-6f) << "share 1 at " << index;
        EXPECT_NEAR(recovered[index][2], original[index][2], kLevel + 1e-6f) << "share 2 at " << index;
    }
}

}  // namespace
}  // namespace pushfold::threeway
