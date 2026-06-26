#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>

#include <pushfold/Frontends/RustFrontend.hpp>

namespace pushfold {
namespace {

// Drives a frontend over an in-memory stream via the ostream-borrowing
// constructor, runs `emit` against it, then returns everything written.
template <typename EmitFn>
std::string Generate(EmitFn emit) {
    std::ostringstream out;
    {
        RustFrontend frontend(out);
        emit(frontend);
    }
    return out.str();
}

TEST(RustFrontendTest, EmitsNothingWithoutTables) {
    EXPECT_EQ(Generate([](RustFrontend&) {}), "");
}

TEST(RustFrontendTest, Emits1DUnsignedArrayAsNumbers) {
    const std::array<std::uint8_t, 3> values = {0, 200, 255};
    const std::string output = Generate([&](RustFrontend& fe) { fe.EmitArray("BYTES", values); });
    EXPECT_EQ(output,
              "pub static BYTES: [u8; 3] = [\n"
              "    0, 200, 255,\n"
              "];\n");
}

TEST(RustFrontendTest, Emits1DFloatArrayWithForcedDecimalPoint) {
    const std::array<float, 3> values = {0.5f, 1.0f, 0.0f};
    const std::string output = Generate([&](RustFrontend& fe) { fe.EmitArray("FLOATS", values); });
    EXPECT_EQ(output,
              "pub static FLOATS: [f32; 3] = [\n"
              "    0.5, 1.0, 0.0,\n"
              "];\n");
}

TEST(RustFrontendTest, Emits1DArrayValuesOnOneLine) {
    std::array<std::uint16_t, 13> values{};
    for (std::uint16_t i = 0; i < values.size(); ++i) {
        values[i] = i;
    }
    const std::string output = Generate([&](RustFrontend& fe) { fe.EmitArray("SEQ", values); });
    EXPECT_EQ(output,
              "pub static SEQ: [u16; 13] = [\n"
              "    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,\n"
              "];\n");
}

TEST(RustFrontendTest, Emits2DArrayWithNestedBrackets) {
    const std::array<std::array<float, 2>, 2> values = {{{0.5f, 0.25f}, {0.75f, 1.0f}}};
    const std::string output = Generate([&](RustFrontend& fe) { fe.EmitArray("TABLE", values); });
    EXPECT_EQ(output,
              "pub static TABLE: [[f32; 2]; 2] = [\n"
              "    [0.5, 0.25],\n"
              "    [0.75, 1.0],\n"
              "];\n");
}

TEST(RustFrontendTest, EmitsStringArrayAsStrSlices) {
    const std::array<std::string_view, 2> values = {"AKs", "QQ"};
    const std::string output = Generate([&](RustFrontend& fe) { fe.EmitArray("HANDS", values); });
    EXPECT_EQ(output,
              "pub static HANDS: [&str; 2] = [\n"
              "    \"AKs\", \"QQ\",\n"
              "];\n");
}

TEST(RustFrontendTest, EscapesSpecialCharactersInStrings) {
    const std::array<std::string_view, 1> values = {"a\"b\\c\td"};
    const std::string output = Generate([&](RustFrontend& fe) { fe.EmitArray("ESC", values); });
    EXPECT_NE(output.find("\"a\\\"b\\\\c\\td\""), std::string::npos) << output;
}

TEST(RustFrontendTest, EmitsAttributeAboveStatic) {
    const std::array<std::uint8_t, 1> values = {1};
    const std::string output = Generate([&](RustFrontend& fe) { fe.EmitArray("GATED", values, "#[cfg(test)]"); });
    EXPECT_EQ(output,
              "#[cfg(test)]\n"
              "pub static GATED: [u8; 1] = [\n"
              "    1,\n"
              "];\n");
}

TEST(RustFrontendTest, EmitsAttributeAbove2DStatic) {
    const std::array<std::array<std::string_view, 1>, 1> values = {{{"AA"}}};
    const std::string output = Generate([&](RustFrontend& fe) { fe.EmitArray("HANDS", values, "#[cfg(test)]"); });
    EXPECT_EQ(output,
              "#[cfg(test)]\n"
              "pub static HANDS: [[&str; 1]; 1] = [\n"
              "    [\"AA\"],\n"
              "];\n");
}

TEST(RustFrontendTest, EmitsModuleDeclaration) {
    const std::string output = Generate([](RustFrontend& fe) { fe.EmitModule("equity"); });
    EXPECT_EQ(output, "pub mod equity;\n");
}

TEST(RustFrontendTest, EmitsGatedModuleDeclaration) {
    const std::string output = Generate([](RustFrontend& fe) { fe.EmitModule("hands", "#[cfg(test)]"); });
    EXPECT_EQ(output,
              "#[cfg(test)]\n"
              "pub mod hands;\n");
}

TEST(RustFrontendTest, EmitsModuleDeclarationsContiguously) {
    const std::string output = Generate([](RustFrontend& fe) {
        fe.EmitModule("equity");
        fe.EmitModule("hands", "#[cfg(test)]");
        fe.EmitModule("matchup");
    });
    EXPECT_EQ(output,
              "pub mod equity;\n"
              "#[cfg(test)]\n"
              "pub mod hands;\n"
              "pub mod matchup;\n");
}

TEST(RustFrontendTest, SeparatesConsecutiveTablesWithBlankLine) {
    const std::array<std::uint8_t, 1> a = {1};
    const std::array<std::uint8_t, 1> b = {2};
    const std::string output = Generate([&](RustFrontend& fe) {
        fe.EmitArray("A", a);
        fe.EmitArray("B", b);
    });
    EXPECT_EQ(output,
              "pub static A: [u8; 1] = [\n"
              "    1,\n"
              "];\n"
              "\n"
              "pub static B: [u8; 1] = [\n"
              "    2,\n"
              "];\n");
}

}  // namespace
}  // namespace pushfold
