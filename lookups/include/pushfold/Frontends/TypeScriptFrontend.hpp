#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <format>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace pushfold {

/**
 * @brief Emits generated TypeScript source containing lookup tables as
 *        `export const NAME: readonly T[] = [...];` definitions.
 *
 * Tables are emitted at the top level: a TypeScript file is itself a module, so
 * each `export const` is directly importable and there is no wrapping construct.
 *
 * The tables are typed as `readonly` arrays rather than `as const` tuples: the
 * equity/matchup tables run to ~14K entries, which exceeds TypeScript's 10,000
 * element tuple-type limit, and a tuple type that large would be useless to the
 * compiler anyway. `readonly` still prevents mutation at use sites.
 *
 * The output stream is managed RAII-style: constructed from a filepath, the
 * object owns the std::ofstream and closes the file on destruction; constructed
 * from a borrowed std::ostream& (handy for testing against a std::ostringstream)
 * the caller retains ownership.
 */
class TypeScriptFrontend {
    std::ofstream owned_;     // backing store when we open the file ourselves
    std::ostream& out_;       // every write goes here (owned_ or a borrowed stream)
    bool wrote_any_ = false;  // whether a table has already been emitted

   public:
    /**
     * @brief Opens @p filepath for writing.
     */
    explicit TypeScriptFrontend(const std::string& filepath) : owned_(filepath), out_(owned_) {}

    /**
     * @brief Writes to a caller-owned stream. Lets a test capture output in a
     *        std::ostringstream instead of a file; the stream must outlive this
     *        object.
     */
    explicit TypeScriptFrontend(std::ostream& out) : out_(out) {}

    // Non-copyable / non-movable: it holds a reference to its output stream.
    TypeScriptFrontend(const TypeScriptFrontend&) = delete;
    TypeScriptFrontend& operator=(const TypeScriptFrontend&) = delete;

    /**
     * @brief Emits a 1D table: `export const NAME: readonly T[] = [...];`.
     */
    template <typename T, std::size_t N>
    void EmitArray(std::string_view name, const std::array<T, N>& values) {
        Separate();
        Write("export const {}: readonly {}[] = [", name, TypeName<T>());
        for (std::size_t i = 0; i < N; ++i) {
            Write("{}{},", i == 0 ? "\n    " : " ", FormatScalar(values[i]));
        }
        Write("\n];\n");
    }

    /**
     * @brief Emits a 2D table:
     *        `export const NAME: readonly (readonly T[])[] = [...];`.
     */
    template <typename T, std::size_t N, std::size_t M>
    void EmitArray(std::string_view name, const std::array<std::array<T, N>, M>& values) {
        Separate();
        Write("export const {}: readonly (readonly {}[])[] = [", name, TypeName<T>());
        for (std::size_t i = 0; i < M; ++i) {
            Write("\n    [");
            for (std::size_t j = 0; j < N; ++j) {
                Write("{}{}", FormatScalar(values[i][j]), j + 1 < N ? ", " : "");
            }
            Write("],");
        }
        Write("\n];\n");
    }

   private:
    // libc++ does not provide the std::print(std::ostream&, ...) overload, so
    // format into the stream by hand. Keeps the format-string call sites tidy.
    template <typename... Args>
    void Write(std::format_string<Args...> fmt, Args&&... args) {
        out_ << std::format(fmt, std::forward<Args>(args)...);
    }

    // Inserts a blank line between consecutive tables (but not before the first,
    // so the file does not open with a leading blank line).
    void Separate() {
        if (wrote_any_) {
            Write("\n");
        }
        wrote_any_ = true;
    }

    // Spelling of the TypeScript element type written into the readonly T[]
    // declaration. Every numeric width collapses to `number`: TypeScript has a
    // single double-precision number type, which exactly represents the u8/u16
    // lookup tables emitted here.
    template <typename T>
    static constexpr std::string_view TypeName() {
        if constexpr (std::is_arithmetic_v<T>) {
            return "number";
        } else if constexpr (StringLike<T>) {
            return "string";
        } else {
            static_assert(kAlwaysFalse<T>, "TypeScriptFrontend: unsupported array element type");
        }
    }

    // Renders one element as a TypeScript literal. All numeric types print as a
    // bare number literal (no width suffix exists); strings are quoted.
    template <typename T>
    static std::string FormatScalar(const T& value) {
        if constexpr (StringLike<T>) {
            return QuoteString(value);
        } else {
            // std::format's default float formatting yields the shortest
            // round-tripping decimal; integers print as plain decimal (and this
            // also avoids operator<< rendering std::uint8_t as a character).
            return std::format("{}", value);
        }
    }

    // Anything that converts to std::string_view: std::string_view itself,
    // std::string, const char*, and string literals.
    template <typename T>
    static constexpr bool StringLike = std::convertible_to<T, std::string_view>;

    // Wraps @p s in double quotes, escaping the characters that can't appear
    // verbatim inside a TypeScript string literal (the escapes match C++'s).
    static std::string QuoteString(std::string_view s) {
        std::string out;
        out.reserve(s.size() + 2);
        out.push_back('"');
        for (const char c : s) {
            switch (c) {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                default:
                    out.push_back(c);
            }
        }
        out.push_back('"');
        return out;
    }

    template <typename>
    static constexpr bool kAlwaysFalse = false;
};

}  // namespace pushfold
