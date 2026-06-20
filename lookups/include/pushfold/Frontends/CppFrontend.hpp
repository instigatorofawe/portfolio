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
 * @brief Emits generated C++ source containing lookup tables as
 *        `inline constexpr std::array<>` definitions.
 *
 * The output is bracketed by the object's lifetime (RAII): the constructor
 * writes the header preamble and opens the namespace; the destructor closes the
 * namespace. Each EmitArray call appends one table definition in between. When
 * constructed from a filepath the object owns the std::ofstream and closes the
 * file on destruction; when constructed from a borrowed std::ostream& (handy for
 * testing against a std::ostringstream) the caller retains ownership.
 */
class CppFrontend {
    std::ofstream owned_;  // backing store when we open the file ourselves
    std::ostream& out_;    // every write goes here (owned_ or a borrowed stream)
    std::string namespace_;

   public:
    /**
     * @brief Opens @p filepath for writing and emits the file preamble.
     */
    explicit CppFrontend(const std::string& filepath, std::string ns = "pushfold")
        : owned_(filepath), out_(owned_), namespace_(std::move(ns)) {
        WritePreamble();
    }

    /**
     * @brief Writes to a caller-owned stream and emits the file preamble. Lets a
     *        test capture output in a std::ostringstream instead of a file; the
     *        stream must outlive this object.
     */
    explicit CppFrontend(std::ostream& out, std::string ns = "pushfold") : out_(out), namespace_(std::move(ns)) {
        WritePreamble();
    }

    /**
     * @brief Closes the open namespace; if we own the file, the ofstream member
     *        then flushes and closes it.
     */
    ~CppFrontend() { Write("\n}}  // namespace {}\n", namespace_); }

    // Non-copyable / non-movable: it holds a reference to its output stream and
    // the closing namespace bracket is tied to this exact object's destruction.
    CppFrontend(const CppFrontend&) = delete;
    CppFrontend& operator=(const CppFrontend&) = delete;

    /**
     * @brief Emits a 1D table: `inline constexpr std::array<T, N> name = {...};`.
     */
    template <typename T, std::size_t N>
    void EmitArray(std::string_view name, const std::array<T, N>& values) {
        Write("\ninline constexpr std::array<{}, {}> {} = {{", TypeName<T>(), N, name);
        for (std::size_t i = 0; i < N; ++i) {
            Write("{}{},", i == 0 ? "\n    " : " ", FormatScalar(values[i]));
        }
        Write("\n}};\n");
    }

    /**
     * @brief Emits a 2D table:
     *        `inline constexpr std::array<std::array<T, N>, M> name = {{...}};`.
     */
    template <typename T, std::size_t N, std::size_t M>
    void EmitArray(std::string_view name, const std::array<std::array<T, N>, M>& values) {
        Write("\ninline constexpr std::array<std::array<{}, {}>, {}> {} = {{{{", TypeName<T>(), N, M, name);
        for (std::size_t i = 0; i < M; ++i) {
            Write("\n    {{");
            for (std::size_t j = 0; j < N; ++j) {
                Write("{}{}", FormatScalar(values[i][j]), j + 1 < N ? ", " : "");
            }
            Write("}},");
        }
        Write("\n}}}};\n");
    }

   private:
    // libc++ does not provide the std::print(std::ostream&, ...) overload, so
    // format into the stream by hand. Keeps the format-string call sites tidy.
    template <typename... Args>
    void Write(std::format_string<Args...> fmt, Args&&... args) {
        out_ << std::format(fmt, std::forward<Args>(args)...);
    }

    // Emits the union of headers our supported element types need; they are
    // cheap and an unused one is harmless, which keeps the preamble fixed rather
    // than dependent on which EmitArray calls follow.
    void WritePreamble() {
        Write(
            "#pragma once\n\n"
            "#include <array>\n"
            "#include <cstdint>\n"
            "#include <string_view>\n\n"
            "namespace {} {{\n",
            namespace_);
    }

    // Spelling of the C++ element type written into the std::array<> declaration.
    template <typename T>
    static constexpr std::string_view TypeName() {
        if constexpr (std::same_as<T, float>) {
            return "float";
        } else if constexpr (std::same_as<T, double>) {
            return "double";
        } else if constexpr (std::same_as<T, std::uint8_t>) {
            return "std::uint8_t";
        } else if constexpr (std::same_as<T, std::uint16_t>) {
            return "std::uint16_t";
        } else if constexpr (std::same_as<T, std::uint32_t>) {
            return "std::uint32_t";
        } else if constexpr (std::same_as<T, std::uint64_t>) {
            return "std::uint64_t";
        } else if constexpr (std::same_as<T, std::int32_t>) {
            return "std::int32_t";
        } else if constexpr (std::same_as<T, std::int64_t>) {
            return "std::int64_t";
        } else if constexpr (StringLike<T>) {
            return "std::string_view";
        } else {
            static_assert(kAlwaysFalse<T>, "CppFrontend: unsupported array element type");
        }
    }

    // Renders one element as a C++ literal of its exact element type.
    template <typename T>
    static std::string FormatScalar(const T& value) {
        if constexpr (std::same_as<T, float>) {
            // The alternate form (#) forces a decimal point and the `f` suffix
            // makes a float literal, so reading the text back reproduces the
            // exact value with no narrowing conversion in the aggregate init.
            return std::format("{:#}f", value);
        } else if constexpr (std::floating_point<T>) {
            return std::format("{:#}", value);
        } else if constexpr (StringLike<T>) {
            // A quoted string literal converts to std::string_view in the
            // aggregate init, so no sv suffix is needed.
            return QuoteString(value);
        } else {
            // Format integers as decimal. Routing through std::format also avoids
            // the operator<< quirk of printing std::uint8_t as a character.
            return std::format("{}", value);
        }
    }

    // Anything that converts to std::string_view: std::string_view itself,
    // std::string, const char*, and string literals.
    template <typename T>
    static constexpr bool StringLike = std::convertible_to<T, std::string_view>;

    // Wraps @p s in double quotes, escaping the characters that can't appear
    // verbatim inside a C++ string literal.
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
