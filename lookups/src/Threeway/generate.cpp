#include <chrono>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <string_view>

#include <pushfold/Constants.hpp>
#include <pushfold/Frontends/CppFrontend.hpp>
#include <pushfold/Frontends/RustFrontend.hpp>
#include <pushfold/Frontends/TypeScriptFrontend.hpp>
#include <pushfold/Threeway/Constants.hpp>
#include <pushfold/Threeway/Equity.hpp>
#include <pushfold/Threeway/Matchups.hpp>

namespace {

using namespace pushfold;
using namespace pushfold::threeway;

// Per-frontend naming. C++ tables follow the kCamelCase convention used in
// Constants.hpp; Rust/TypeScript statics follow SCREAMING_CASE. The extension
// also picks the output filename. Tables are prefixed "threeway_" so they can
// share a generated directory with the heads-up tables without colliding.
struct Naming {
    std::string_view extension;
    std::string_view equity;
    std::string_view matchup;
    std::string_view hands;
};

constexpr Naming kCppNaming = {.extension = "hpp", .equity = "kEquities", .matchup = "kMatchups", .hands = "kHands"};
constexpr Naming kRustNaming = {.extension = "rs", .equity = "EQUITIES", .matchup = "MATCHUPS", .hands = "HANDS"};
constexpr Naming kTsNaming = {.extension = "ts", .equity = "EQUITIES", .matchup = "MATCHUPS", .hands = "HANDS"};

// Renders a duration as e.g. "1h04m09s" / "12m03s" / "9s" for the progress line.
std::string FormatDuration(double seconds) {
    const long total = static_cast<long>(seconds + 0.5);
    const long h = total / 3600, m = (total % 3600) / 60, s = total % 60;
    if (h > 0) {
        return std::format("{}h{:02}m{:02}s", h, m, s);
    }
    if (m > 0) {
        return std::format("{}m{:02}s", m, s);
    }
    return std::format("{}s", s);
}

// A live, single-line progress bar (on stderr, so it stays out of the stdout
// log) with elapsed time and a linear ETA extrapolated from the fraction done.
auto MakeProgressBar() {
    return [start = std::chrono::steady_clock::now()](size_t done, size_t total) {
        const double frac = total > 0 ? static_cast<double>(done) / static_cast<double>(total) : 1.0;
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const double eta = frac > 0.0 ? elapsed * (1.0 - frac) / frac : 0.0;

        constexpr int kWidth = 30;
        const int filled = static_cast<int>(frac * kWidth);
        std::string bar(static_cast<size_t>(filled), '#');
        bar.append(static_cast<size_t>(kWidth - filled), '-');

        std::print(stderr, "\r  [{}] {:>3}% ({}/{})  elapsed {}  eta {}   ", bar, static_cast<int>(frac * 100.0), done,
                   total, FormatDuration(elapsed), FormatDuration(eta));
        if (done == total) {
            std::print(stderr, "\n");
        }
    };
}

// Opens one table file with @p Frontend and emits @p table under @p name. The
// frontend closes the namespace/file when it destructs at the end of scope. A
// non-empty @p rust_attribute annotates the Rust static (e.g. "#[cfg(test)]");
// it has no C++/TS analogue and is ignored by those frontends.
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
// @p dir. Each table is only solved when selected, so the flags can skip the
// expensive equity solve.
template <typename Frontend>
void Generate(const std::filesystem::path& dir, const Naming& naming, const Selection& selection) {
    // The equity table dominates the runtime: it enumerates every three-way
    // matchup exactly. Keep the generators off the stack (multi-megabyte tables).
    if (selection.equity) {
        std::println("Solving equity table ({} matchups, exact enumeration)...", kNumMatchupEntries);
        const auto equity = std::make_unique<EquityGenerator>();
        equity->Solve(MakeProgressBar());
        const std::unique_ptr equities = equity->Quantize();
        WriteTable<Frontend>(dir / std::format("threeway_equity.{}", naming.extension), naming.equity, *equities);
    }

    if (selection.matchup) {
        std::println("Solving matchup table ({} entries)...", kNumMatchupEntries);
        const auto matchup = std::make_unique<MatchupGenerator>();
        WriteTable<Frontend>(dir / std::format("threeway_matchup.{}", naming.extension), naming.matchup,
                             matchup->Matchups());
    }

    // The hands table maps each infoset index to its label. It is fixed reference
    // data (not solved) shared with the heads-up tables, and only the consumers'
    // tests need it, so the Rust copy is gated behind the `hands` cargo feature.
    if (selection.hands) {
        WriteTable<Frontend>(dir / std::format("hands.{}", naming.extension), naming.hands, kHands,
                             "#[cfg(feature = \"hands\")]");
    }

    // Rust groups the generated files under a directory module, which needs a
    // mod.rs declaring each table file as a submodule. The other frontends pull
    // the files in directly (C++ via #include, TypeScript via module path), so
    // they need no equivalent.
    if constexpr (std::same_as<Frontend, RustFrontend>) {
        RustFrontend mod((dir / "mod.rs").string());
        if (selection.equity) {
            mod.EmitModule("threeway_equity");
        }
        if (selection.hands) {
            mod.EmitModule("hands", "#[cfg(feature = \"hands\")]");
        }
        if (selection.matchup) {
            mod.EmitModule("threeway_matchup");
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
        // Create the output directory (and parents) if it does not exist; a no-op
        // when it already does.
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
