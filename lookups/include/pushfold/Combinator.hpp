#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <ranges>

namespace pushfold {

/**
 * @brief A std::ranges view over every K-subset of {0, 1, ..., N-1}.
 *
 * Combinations are produced in lexicographic order of their (ascending) index
 * arrays, starting at {0, 1, ..., K-1} and ending at {N-K, ..., N-1}. The view
 * is stateless: each combination is materialized inside the iterator, so the
 * iterators stay valid even after the Combinator is destroyed (the view is a
 * borrowed_range).
 *
 * Degenerate shapes behave as the math dictates: K == 0 yields a single empty
 * combination, and K > N yields none.
 */
template <size_t N, size_t K>
class Combinator : public std::ranges::view_interface<Combinator<N, K>> {
   public:
    /**
     * @brief Forward iterator that walks combinations in lexicographic order.
     *
     * The current combination is stored in the iterator itself, so copies are
     * independent and the multipass (forward) guarantee holds. A default- or
     * end-constructed iterator is "past the end"; such iterators compare equal
     * to each other regardless of their stale buffer contents.
     */
    class Iterator {
       public:
        using value_type = std::array<size_t, K>;
        using reference = const value_type&;
        using pointer = const value_type*;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;

        /// Constructs a past-the-end iterator.
        Iterator() = default;

        reference operator*() const { return combination_; }
        pointer operator->() const { return &combination_; }

        Iterator& operator++() {
            Advance();
            return *this;
        }

        Iterator operator++(int) {
            Iterator previous = *this;
            Advance();
            return previous;
        }

        bool operator==(const Iterator& other) const {
            // A past-the-end iterator carries a stale combination_, so compare
            // the done_ flags first: two ended iterators are equal, and an ended
            // one is never equal to a live one. Only when both are live does the
            // actual combination decide equality.
            if (done_ || other.done_) {
                return done_ == other.done_;
            }
            return combination_ == other.combination_;
        }

       private:
        friend class Combinator;

        /// Builds the first combination {0, 1, ..., K-1}; `done` marks the range
        /// as already exhausted (used when there are no combinations to emit).
        explicit Iterator(bool done) : done_(done) {
            for (size_t i = 0; i < K; ++i) {
                combination_[i] = i;
            }
        }

        /// Advances to the next combination in lexicographic order, or marks the
        /// iterator past-the-end when the final combination has been passed.
        void Advance() {
            if (done_) {
                return;
            }
            if constexpr (K == 0) {
                // The lone empty combination has already been visited.
                done_ = true;
            } else {
                // Scan from the right for the last position that can still be
                // bumped. Position i tops out at N - K + i (so the positions to
                // its right have room to stay strictly increasing). K <= N is
                // guaranteed here: when K > N the range starts exhausted, so the
                // done_ check above returns first and N - K never underflows.
                for (size_t i = K; i-- > 0;) {
                    if (combination_[i] < N - K + i) {
                        ++combination_[i];
                        for (size_t j = i + 1; j < K; ++j) {
                            combination_[j] = combination_[j - 1] + 1;
                        }
                        return;
                    }
                }
                done_ = true;
            }
        }

        value_type combination_{};
        bool done_ = true;
    };

    Iterator begin() const { return Iterator(/*done=*/K > N); }
    Iterator end() const { return Iterator(/*done=*/true); }

    /// Number of combinations in the view, i.e. the binomial coefficient C(N, K)
    /// (zero when K > N). Makes Combinator a sized_range.
    static constexpr size_t size() {
        if constexpr (K > N) {
            return 0;
        } else {
            // Multiply by the symmetric, smaller factor count to limit the
            // running product, and divide step by step (each partial product is
            // an exact binomial coefficient, so the division never truncates).
            const size_t terms = K < N - K ? K : N - K;
            size_t result = 1;
            for (size_t i = 0; i < terms; ++i) {
                result = result * (N - i) / (i + 1);
            }
            return result;
        }
    }
};

}  // namespace pushfold

// The iterators own their state and never refer back to the Combinator, so a
// combination obtained from a temporary Combinator stays valid: it is a
// borrowed_range.
template <size_t N, size_t K>
inline constexpr bool std::ranges::enable_borrowed_range<pushfold::Combinator<N, K>> = true;
