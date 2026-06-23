#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>

#include <pushfold/Frontends/TypeScriptFrontend.hpp>

namespace pushfold {
namespace {

// Drives a frontend over an in-memory stream via the ostream-borrowing
// constructor, runs `emit` against it, then returns everything written.
template <typename EmitFn>
std::string Generate(EmitFn emit) {
    std::ostringstream out;
    {
        TypeScriptFrontend frontend(out);
        emit(frontend);
    }
    return out.str();
}

TEST(TypeScriptFrontendTest, EmitsNothingWithoutTables) {
    EXPECT_EQ(Generate([](TypeScriptFrontend&) {}), "");
}

TEST(TypeScriptFrontendTest, Emits1DUnsignedArrayAsNumbers) {
    const std::array<std::uint8_t, 3> values = {0, 200, 255};
    const std::string output = Generate([&](TypeScriptFrontend& fe) { fe.EmitArray("BYTES", values); });
    EXPECT_EQ(output,
              "export const BYTES: readonly number[] = [\n"
              "    0, 200, 255,\n"
              "];\n");
}

TEST(TypeScriptFrontendTest, Emits1DFloatArrayAsNumbers) {
    const std::array<float, 3> values = {0.5f, 1.0f, 0.0f};
    const std::string output = Generate([&](TypeScriptFrontend& fe) { fe.EmitArray("FLOATS", values); });
    EXPECT_EQ(output,
              "export const FLOATS: readonly number[] = [\n"
              "    0.5, 1, 0,\n"
              "];\n");
}

TEST(TypeScriptFrontendTest, Emits1DArrayValuesOnOneLine) {
    std::array<std::uint16_t, 13> values{};
    for (std::uint16_t i = 0; i < values.size(); ++i) {
        values[i] = i;
    }
    const std::string output = Generate([&](TypeScriptFrontend& fe) { fe.EmitArray("SEQ", values); });
    EXPECT_EQ(output,
              "export const SEQ: readonly number[] = [\n"
              "    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,\n"
              "];\n");
}

TEST(TypeScriptFrontendTest, Emits2DArrayWithNestedBrackets) {
    const std::array<std::array<float, 2>, 2> values = {{{0.5f, 0.25f}, {0.75f, 1.0f}}};
    const std::string output = Generate([&](TypeScriptFrontend& fe) { fe.EmitArray("TABLE", values); });
    EXPECT_EQ(output,
              "export const TABLE: readonly (readonly number[])[] = [\n"
              "    [0.5, 0.25],\n"
              "    [0.75, 1],\n"
              "];\n");
}

TEST(TypeScriptFrontendTest, EmitsStringArrayAsStringLiterals) {
    const std::array<std::string_view, 2> values = {"AKs", "QQ"};
    const std::string output = Generate([&](TypeScriptFrontend& fe) { fe.EmitArray("HANDS", values); });
    EXPECT_EQ(output,
              "export const HANDS: readonly string[] = [\n"
              "    \"AKs\", \"QQ\",\n"
              "];\n");
}

TEST(TypeScriptFrontendTest, EscapesSpecialCharactersInStrings) {
    const std::array<std::string_view, 1> values = {"a\"b\\c\td"};
    const std::string output = Generate([&](TypeScriptFrontend& fe) { fe.EmitArray("ESC", values); });
    EXPECT_NE(output.find("\"a\\\"b\\\\c\\td\""), std::string::npos) << output;
}

TEST(TypeScriptFrontendTest, SeparatesConsecutiveTablesWithBlankLine) {
    const std::array<std::uint8_t, 1> a = {1};
    const std::array<std::uint8_t, 1> b = {2};
    const std::string output = Generate([&](TypeScriptFrontend& fe) {
        fe.EmitArray("A", a);
        fe.EmitArray("B", b);
    });
    EXPECT_EQ(output,
              "export const A: readonly number[] = [\n"
              "    1,\n"
              "];\n"
              "\n"
              "export const B: readonly number[] = [\n"
              "    2,\n"
              "];\n");
}

}  // namespace
}  // namespace pushfold
