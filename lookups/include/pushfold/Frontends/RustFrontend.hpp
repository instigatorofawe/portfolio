#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace pushfold {

/**
 * @brief Emits generated Rust source containing lookup tables as
 *        `pub static NAME: [T; N] = [...];` array definitions.
 *
 * Tables are emitted at the top level: a Rust file is itself a module, so there
 * is no wrapping `mod {}`. `static` (rather than `const`) gives each table a
 * single fixed address instead of inlining a fresh copy at every use site, which
 * suits large lookup tables.
 *
 * The output stream is managed RAII-style: constructed from a filepath, the
 * object owns the std::ofstream and closes the file on destruction; constructed
 * from a borrowed std::ostream& (handy for testing against a std::ostringstream)
 * the caller retains ownership.
 */
class RustFrontend {
    std::ofstream owned_;     // backing store when we open the file ourselves
    std::ostream& out_;       // every write goes here (owned_ or a borrowed stream)
    bool wrote_any_ = false;  // whether a table has already been emitted

   public:
    /**
     * @brief Opens @p filepath for writing.
     */
    explicit RustFrontend(const std::string& filepath) : owned_(filepath), out_(owned_) {}

    /**
     * @brief Writes to a caller-owned stream. Lets a test capture output in a
     *        std::ostringstream instead of a file; the stream must outlive this
     *        object.
     */
    explicit RustFrontend(std::ostream& out) : out_(out) {}

    // Non-copyable / non-movable: it holds a reference to its output stream.
    RustFrontend(const RustFrontend&) = delete;
    RustFrontend& operator=(const RustFrontend&) = delete;

    /**
     * @brief Emits a 1D table: `pub static NAME: [T; N] = [...];`.
     *
     * A non-empty @p attribute is written on its own line directly above the
     * static (e.g. "#[cfg(test)]" to keep a table out of release builds).
     */
    template <typename T, std::size_t N>
    void EmitArray(std::string_view name, const std::array<T, N>& values, std::string_view attribute = {}) {
        Separate();
        EmitStaticAttributes(attribute);
        Write("pub static {}: [{}; {}] = [", name, TypeName<T>(), N);
        for (std::size_t i = 0; i < N; ++i) {
            Write("{}{},", i == 0 ? "\n    " : " ", FormatScalar(values[i]));
        }
        Write("\n];\n");
    }

    /**
     * @brief Emits a 2D table: `pub static NAME: [[T; N]; M] = [...];`.
     *
     * A non-empty @p attribute is written on its own line directly above the
     * static (e.g. "#[cfg(test)]" to keep a table out of release builds).
     */
    template <typename T, std::size_t N, std::size_t M>
    void EmitArray(std::string_view name, const std::array<std::array<T, N>, M>& values,
                   std::string_view attribute = {}) {
        Separate();
        EmitStaticAttributes(attribute);
        Write("pub static {}: [[{}; {}]; {}] = [", name, TypeName<T>(), N, M);
        for (std::size_t i = 0; i < M; ++i) {
            Write("\n    [");
            for (std::size_t j = 0; j < N; ++j) {
                Write("{}{}", FormatScalar(values[i][j]), j + 1 < N ? ", " : "");
            }
            Write("],");
        }
        Write("\n];\n");
    }

    /**
     * @brief Emits a module declaration: `pub mod NAME;`.
     *
     * Generated tables live in their own files; a directory module needs a
     * `mod.rs` declaring each file as a submodule, which is what this emits. A
     * non-empty @p attribute is written on its own line directly above the
     * declaration (e.g. "#[cfg(test)]" to keep a test-only module out of release
     * builds). Unlike EmitArray, declarations are written contiguously with no
     * separating blank line so they read as a single module list.
     *
     * Each declaration also carries an #[rustfmt::skip]: rustfmt's
     * `reorder_modules` would otherwise alphabetize the `pub mod` lines, churning
     * this generated file whenever the emission order differs from alphabetical.
     * The skip goes on every declaration (not just the first) so the order is
     * pinned regardless of the order tables happen to be generated in. As with
     * the statics, an outer attribute is used rather than a file-level inner
     * #![rustfmt::skip] because custom inner attributes are unstable on the stable
     * toolchain.
     */
    void EmitModule(std::string_view name, std::string_view attribute = {}) {
        EmitAttribute(attribute);
        Write("#[rustfmt::skip]\n");
        Write("pub mod {};\n", name);
        wrote_any_ = true;
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

    // Writes an outer attribute on its own line above the next static; a no-op
    // for the empty attribute so unannotated tables are unaffected.
    void EmitAttribute(std::string_view attribute) {
        if (!attribute.empty()) {
            Write("{}\n", attribute);
        }
    }

    // Writes the attribute lines that sit directly above a generated static: the
    // optional caller @p attribute (e.g. "#[cfg(test)]") followed by an
    // #[rustfmt::skip]. The tables are emitted in a deterministic, column-packed
    // layout; the skip stops `cargo fmt` from reflowing them — pure churn on a
    // file that is regenerated from scratch. An outer attribute is used (rather
    // than a file-level inner #![rustfmt::skip]) because custom inner attributes
    // are unstable on the stable toolchain.
    void EmitStaticAttributes(std::string_view attribute) {
        EmitAttribute(attribute);
        Write("#[rustfmt::skip]\n");
    }

    // Spelling of the Rust element type written into the [T; N] declaration.
    template <typename T>
    static constexpr std::string_view TypeName() {
        if constexpr (std::same_as<T, float>) {
            return "f32";
        } else if constexpr (std::same_as<T, double>) {
            return "f64";
        } else if constexpr (std::same_as<T, std::uint8_t>) {
            return "u8";
        } else if constexpr (std::same_as<T, std::uint16_t>) {
            return "u16";
        } else if constexpr (std::same_as<T, std::uint32_t>) {
            return "u32";
        } else if constexpr (std::same_as<T, std::uint64_t>) {
            return "u64";
        } else if constexpr (std::same_as<T, std::int32_t>) {
            return "i32";
        } else if constexpr (std::same_as<T, std::int64_t>) {
            return "i64";
        } else if constexpr (StringLike<T>) {
            return "&str";
        } else {
            static_assert(kAlwaysFalse<T>, "RustFrontend: unsupported array element type");
        }
    }

    // Renders one element as a Rust literal. The element type is fixed by the
    // surrounding array declaration, so numeric literals need no type suffix.
    template <typename T>
    static std::string FormatScalar(const T& value) {
        if constexpr (std::floating_point<T>) {
            // Rust needs a fractional part to read a value as a float literal, so
            // ensure a decimal point: "1" -> "1.0", "0.5" stays. Rust parses the
            // literal directly at the target type, so no narrowing occurs.
            std::string text = std::format("{}", value);
            if (text.find_first_of(".eE") == std::string::npos) {
                text += ".0";
            }
            return text;
        } else if constexpr (StringLike<T>) {
            return QuoteString(value);
        } else {
            return std::format("{}", value);
        }
    }

    // Anything that converts to std::string_view: std::string_view itself,
    // std::string, const char*, and string literals.
    template <typename T>
    static constexpr bool StringLike = std::convertible_to<T, std::string_view>;

    // Wraps @p s in double quotes, escaping the characters that can't appear
    // verbatim inside a Rust string literal (the escapes match C++'s).
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
