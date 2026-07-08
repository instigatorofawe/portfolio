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
#include <pushfold/Frontends/CppFrontend.hpp>
#include <pushfold/Frontends/RustFrontend.hpp>
#include <pushfold/Frontends/TypeScriptFrontend.hpp>
#include <pushfold/Headsup/Equity.hpp>
#include <pushfold/Headsup/Matchups.hpp>

namespace {

using namespace pushfold;
using namespace pushfold::headsup;

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
constexpr Naming kTsNaming = {.extension = "ts", .equity = "EQUITIES", .matchup = "MATCHUPS", .hands = "HANDS"};

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

// Which tables to generate. Defaulted to none so the flag parser can set only
// what was requested; main() turns "nothing selected" into "select all".
struct Selection {
    bool equity = false;
    bool matchup = false;
    bool hands = false;

    bool Any() const { return equity || matchup || hands; }
};

// Computes and emits the @p selection of tables as @p Frontend source into
// @p dir using the given naming/extension. Each table is only solved when it is
// selected, so the flags can skip the expensive equity solve.
template <typename Frontend>
void Generate(const std::filesystem::path& dir, const Naming& naming, const Selection& selection) {
    // The generators hold ~114KB equity / ~28KB matchup matrices; keep them off
    // the stack. Solving the equity table enumerates every infoset matchup, so
    // it dominates the runtime.
    if (selection.equity) {
        std::println("Solving equity table ({} matchups)...", kNumEquityMatchups);
        const auto equity = std::make_unique<EquityGenerator>();
        const std::array<EquityScalar, kNumEquityMatchups> equities = equity->Quantize<EquityScalar>();
        WriteTable<Frontend>(dir / std::format("headsup_equity.{}", naming.extension), naming.equity, equities);
    }

    if (selection.matchup) {
        std::println("Solving matchup table ({} entries)...", kNumMatchupEntries);
        const auto matchup = std::make_unique<MatchupGenerator>();
        const std::array<std::uint8_t, kNumMatchupEntries> matchups = matchup->Flatten();
        WriteTable<Frontend>(dir / std::format("headsup_matchup.{}", naming.extension), naming.matchup, matchups);
    }

    // The hands table maps each infoset index to its label. It is fixed
    // reference data (not solved), and only the consumers' tests need it, so the
    // Rust copy is gated behind the `hands` cargo feature (which the solver crates
    // enable only as a dev-dependency) to keep it out of release builds.
    if (selection.hands) {
        WriteTable<Frontend>(dir / std::format("hands.{}", naming.extension), naming.hands, kHands,
                             "#[cfg(feature = \"hands\")]");
    }

    // Rust groups the generated files under a directory module, which needs a
    // mod.rs declaring each table file as a submodule. The other frontends pull
    // the files in directly (C++ via #include, TypeScript via module path), so
    // they need no equivalent. The module names are the file stems
    // ("headsup_equity", "headsup_matchup", "hands"); the heads-up tables are
    // prefixed so three-way tables can share this module without colliding, and
    // the hands table is behind the `hands` cargo feature, so its module is gated
    // to match.
    if constexpr (std::same_as<Frontend, RustFrontend>) {
        RustFrontend mod((dir / "mod.rs").string());
        if (selection.equity) {
            mod.EmitModule("headsup_equity");
        }
        if (selection.hands) {
            mod.EmitModule("hands", "#[cfg(feature = \"hands\")]");
        }
        if (selection.matchup) {
            mod.EmitModule("headsup_matchup");
        }
        std::println("  wrote {}", (dir / "mod.rs").string());
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto print_usage = [&] {
        std::println(stderr, "usage: {} <output-dir> <rust|cpp|ts> [--equity] [--matchup] [--hands]", argv[0]);
        std::println(stderr, "  table flags select which tables to generate (default: all)");
    };

    std::string_view dir_arg;
    std::string_view frontend;
    Selection selection;
    int positional = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--equity") {
            selection.equity = true;
        } else if (arg == "--matchup") {
            selection.matchup = true;
        } else if (arg == "--hands") {
            selection.hands = true;
        } else if (arg.starts_with("--")) {
            std::println(stderr, "error: unknown flag '{}'", arg);
            print_usage();
            return 2;
        } else if (positional == 0) {
            dir_arg = arg;
            ++positional;
        } else if (positional == 1) {
            frontend = arg;
            ++positional;
        } else {
            std::println(stderr, "error: unexpected argument '{}'", arg);
            print_usage();
            return 2;
        }
    }

    if (positional != 2) {
        print_usage();
        return 2;
    }

    // No table flags means generate everything.
    if (!selection.Any()) {
        selection = {.equity = true, .matchup = true, .hands = true};
    }

    const std::filesystem::path dir = dir_arg;

    try {
        // Create the output directory (and parents) if it does not exist; a
        // no-op when it already does.
        std::filesystem::create_directories(dir);

        if (frontend == "cpp") {
            Generate<CppFrontend>(dir, kCppNaming, selection);
        } else if (frontend == "rust") {
            Generate<RustFrontend>(dir, kRustNaming, selection);
        } else if (frontend == "ts") {
            Generate<TypeScriptFrontend>(dir, kTsNaming, selection);
        } else {
            std::println(stderr, "error: unknown frontend '{}' (expected 'rust', 'cpp', or 'ts')", frontend);
            return 2;
        }
    } catch (const std::exception& e) {
        std::println(stderr, "error: {}", e.what());
        return 1;
    }

    return 0;
}
