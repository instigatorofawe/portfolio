#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>

#include <pushfold/Equity.hpp>

#include "pushfold/Constants.hpp"

namespace pushfold {
namespace {

using EquityTable = std::array<std::array<float, kNumInfosets>, kNumInfosets>;

// Builds a deterministic equity table that mimics the layout the solver
// produces: 0.5 on the diagonal, an upper triangle that sweeps the whole
// [0, 1] range, and a lower triangle that is its complement.
std::unique_ptr<EquityTable> MakeSyntheticTable() {
    auto table = std::make_unique<EquityTable>();
    for (size_t i = 0; i < kNumInfosets; ++i) {
        (*table)[i][i] = 0.5f;
    }
    size_t index = 0;
    for (size_t i = 0; i < kNumInfosets; ++i) {
        for (size_t j = i + 1; j < kNumInfosets; ++j) {
            const float equity = static_cast<float>(index) / static_cast<float>(kNumEquityMatchups - 1);
            (*table)[i][j] = equity;
            (*table)[j][i] = 1.0f - equity;
            ++index;
        }
    }
    return table;
}

// Quantizing to T rounds to the nearest of max<T>() + 1 levels, so the worst
// case round-trip error is half a step; allow a small slack for float casts.
template <typename T>
constexpr float RoundTripTolerance() {
    return 0.5f / static_cast<float>(std::numeric_limits<T>::max()) + 1e-6f;
}

template <typename T>
void ExpectRoundTrip() {
    auto table = MakeSyntheticTable();

    EquityGenerator generator(*table);
    ASSERT_TRUE(generator.Ready());

    const std::array<T, kNumEquityMatchups> quantized = generator.Quantize<T>();
    EquityGenerator restored = EquityGenerator::Dequantize<T>(quantized);
    ASSERT_TRUE(restored.Ready());

    const EquityTable& original = *table;
    const EquityTable& recovered = restored.Equities();

    const float tolerance = RoundTripTolerance<T>();
    for (size_t i = 0; i < kNumInfosets; ++i) {
        for (size_t j = 0; j < kNumInfosets; ++j) {
            EXPECT_NEAR(recovered[i][j], original[i][j], tolerance) << "mismatch at (" << i << ", " << j << ")";
        }
    }
}

}  // namespace

TEST(EquityTest, DefaultConstructedIsNotReady) {
    EquityGenerator generator;
    EXPECT_FALSE(generator.Ready());
}

TEST(EquityTest, ConstructedFromTableIsReady) {
    auto table = MakeSyntheticTable();
    EquityGenerator generator(*table);
    EXPECT_TRUE(generator.Ready());
}

TEST(EquityTest, QuantizeRoundTripUint8) { ExpectRoundTrip<std::uint8_t>(); }

TEST(EquityTest, QuantizeRoundTripUint16) { ExpectRoundTrip<std::uint16_t>(); }

}  // namespace pushfold
