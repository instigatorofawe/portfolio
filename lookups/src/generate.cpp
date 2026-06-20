#include <array>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <string_view>

#include <pushfold/Constants.hpp>
#include <pushfold/Equity.hpp>
#include <pushfold/Frontends/CppFrontend.hpp>
#include <pushfold/Frontends/RustFrontend.hpp>
#include <pushfold/Matchups.hpp>

namespace {

using namespace pushfold;

// Fixed-point type for the equity table. uint16_t keeps the round-trip error
// under 1/2^16 (see EquityTest) while halving the table size against a float.
using EquityScalar = std::uint16_t;

// Per-frontend naming. C++ tables follow the kCamelCase convention used in
// Constants.hpp; Rust statics follow SCREAMING_CASE. The extension also picks
// the output filename.
struct Naming {
    std::string_view extension;
    std::string_view equity;
    std::string_view matchup;
    std::string_view hands;
};

constexpr Naming kCppNaming = {.extension = "hpp", .equity = "kEquities", .matchup = "kMatchups", .hands = "kHands"};
constexpr Naming kRustNaming = {.extension = "rs", .equity = "EQUITIES", .matchup = "MATCHUPS", .hands = "HANDS"};

// Opens one table file with @p Frontend and emits @p table under @p name. The
// frontend closes the namespace/file when it destructs at the end of scope. A
// non-empty @p rust_attribute annotates the Rust static (e.g. "#[cfg(test)]");
// it has no C++ analogue and is ignored by the C++ frontend.
template <typename Frontend, typename Table>
void WriteTable(const std::filesystem::path& path, std::string_view name, const Table& table,
                std::string_view rust_attribute = {}) {
    Frontend frontend(path.string());
    if constexpr (std::same_as<Frontend, RustFrontend>) {
        frontend.EmitArray(name, table, rust_attribute);
    } else {
        frontend.EmitArray(name, table);
    }
    std::println("  wrote {}", path.string());
}

// Computes the equity and matchup tables, then emits both as @p Frontend source
// into @p dir using the given naming/extension.
template <typename Frontend>
void Generate(const std::filesystem::path& dir, const Naming& naming) {
    // The generators hold ~114KB equity / ~28KB matchup matrices; keep them off
    // the stack. Solving the equity table enumerates every infoset matchup, so
    // it dominates the runtime.
    std::println("Solving equity table ({} matchups)...", kNumEquityMatchups);
    const auto equity = std::make_unique<EquityGenerator>();
    const std::array<EquityScalar, kNumEquityMatchups> equities = equity->Quantize<EquityScalar>();

    std::println("Solving matchup table ({} entries)...", kNumMatchupEntries);
    const auto matchup = std::make_unique<MatchupGenerator>();
    const std::array<std::uint8_t, kNumMatchupEntries> matchups = matchup->Flatten();

    WriteTable<Frontend>(dir / std::format("equity.{}", naming.extension), naming.equity, equities);
    WriteTable<Frontend>(dir / std::format("matchup.{}", naming.extension), naming.matchup, matchups);

    // The hands table maps each infoset index to its label. It is fixed
    // reference data (not solved), and only the consumers' tests need it, so the
    // Rust copy is gated behind #[cfg(test)] to keep it out of release builds.
    WriteTable<Frontend>(dir / std::format("hands.{}", naming.extension), naming.hands, kHands, "#[cfg(test)]");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::println(stderr, "usage: {} <output-dir> <rust|cpp>", argv[0]);
        return 2;
    }

    const std::filesystem::path dir = argv[1];
    const std::string_view frontend = argv[2];

    try {
        // Create the output directory (and parents) if it does not exist; a
        // no-op when it already does.
        std::filesystem::create_directories(dir);

        if (frontend == "cpp") {
            Generate<CppFrontend>(dir, kCppNaming);
        } else if (frontend == "rust") {
            Generate<RustFrontend>(dir, kRustNaming);
        } else {
            std::println(stderr, "error: unknown frontend '{}' (expected 'rust' or 'cpp')", frontend);
            return 2;
        }
    } catch (const std::exception& e) {
        std::println(stderr, "error: {}", e.what());
        return 1;
    }

    return 0;
}
