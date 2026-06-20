#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <pushfold/Frontends/CppFrontend.hpp>

namespace pushfold {
namespace {

// Drives a frontend over an in-memory stream via the ostream-borrowing
// constructor, runs `emit` against it, then returns everything written once the
// frontend (and so the namespace footer) is done.
template <typename EmitFn>
std::string Generate(EmitFn emit, std::string ns = "pushfold") {
    std::ostringstream out;
    {
        CppFrontend frontend(out, std::move(ns));
        emit(frontend);
    }  // frontend destructs here: writes the namespace footer.
    return out.str();
}

constexpr std::string_view kPreamble =
    "#pragma once\n\n"
    "#include <array>\n"
    "#include <cstdint>\n"
    "#include <string_view>\n\n"
    "namespace pushfold {\n";

TEST(CppFrontendTest, EmitsPreambleAndNamespaceFooter) {
    const std::string output = Generate([](CppFrontend&) {});
    EXPECT_EQ(output, std::string(kPreamble) + "\n}  // namespace pushfold\n");
}

TEST(CppFrontendTest, UsesCustomNamespace) {
    const std::string output = Generate([](CppFrontend&) {}, "poker");
    EXPECT_NE(output.find("namespace poker {\n"), std::string::npos);
    EXPECT_NE(output.find("\n}  // namespace poker\n"), std::string::npos);
}

TEST(CppFrontendTest, Emits1DUnsignedArrayAsNumbers) {
    const std::array<std::uint8_t, 3> values = {0, 200, 255};
    const std::string output = Generate([&](CppFrontend& fe) { fe.EmitArray("kBytes", values); });
    // std::uint8_t must render as integers, not characters.
    EXPECT_NE(output.find("inline constexpr std::array<std::uint8_t, 3> kBytes = {\n"
                          "    0, 200, 255,\n"
                          "};\n"),
              std::string::npos)
        << output;
}

TEST(CppFrontendTest, Emits1DFloatArrayWithSuffixAndDecimalPoint) {
    const std::array<float, 3> values = {0.5f, 1.0f, 0.0f};
    const std::string output = Generate([&](CppFrontend& fe) { fe.EmitArray("kFloats", values); });
    EXPECT_NE(output.find("inline constexpr std::array<float, 3> kFloats = {\n"
                          "    0.5f, 1.f, 0.f,\n"
                          "};\n"),
              std::string::npos)
        << output;
}

TEST(CppFrontendTest, Emits1DArrayValuesOnOneLine) {
    std::array<std::uint16_t, 13> values{};
    for (std::uint16_t i = 0; i < values.size(); ++i) {
        values[i] = i;
    }
    const std::string output = Generate([&](CppFrontend& fe) { fe.EmitArray("kSeq", values); });
    EXPECT_NE(output.find("inline constexpr std::array<std::uint16_t, 13> kSeq = {\n"
                          "    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,\n"
                          "};\n"),
              std::string::npos)
        << output;
}

TEST(CppFrontendTest, Emits2DArrayWithNestedBraces) {
    const std::array<std::array<float, 2>, 2> values = {{{0.5f, 0.25f}, {0.75f, 1.0f}}};
    const std::string output = Generate([&](CppFrontend& fe) { fe.EmitArray("kTable", values); });
    EXPECT_NE(output.find("inline constexpr std::array<std::array<float, 2>, 2> kTable = {{\n"
                          "    {0.5f, 0.25f},\n"
                          "    {0.75f, 1.f},\n"
                          "}};\n"),
              std::string::npos)
        << output;
}

TEST(CppFrontendTest, EmitsStringArrayAsQuotedStringView) {
    const std::array<std::string_view, 2> values = {"AKs", "QQ"};
    const std::string output = Generate([&](CppFrontend& fe) { fe.EmitArray("kHands", values); });
    EXPECT_NE(output.find("inline constexpr std::array<std::string_view, 2> kHands = {\n"
                          "    \"AKs\", \"QQ\",\n"
                          "};\n"),
              std::string::npos)
        << output;
}

TEST(CppFrontendTest, EscapesSpecialCharactersInStrings) {
    const std::array<std::string_view, 1> values = {"a\"b\\c\td"};
    const std::string output = Generate([&](CppFrontend& fe) { fe.EmitArray("kEsc", values); });
    EXPECT_NE(output.find("\"a\\\"b\\\\c\\td\""), std::string::npos) << output;
}

}  // namespace
}  // namespace pushfold
