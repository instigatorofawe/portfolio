#pragma once

#include <Eigen/Core>

namespace dsplib {

template <typename T>
concept EigenType = std::derived_from<T, Eigen::EigenBase<T>>;

/**
 * @brief Checks for dimension equality of two eigen expressions
 */
template <EigenType A, EigenType B>
constexpr bool EqualDims(const A& a, const B& b) {
    constexpr int AR = A::RowsAtCompileTime, AC = A::ColsAtCompileTime;
    constexpr int BR = B::RowsAtCompileTime, BC = B::ColsAtCompileTime;

    bool rows_ok, cols_ok;

    if constexpr (AR != Eigen::Dynamic && BR != Eigen::Dynamic) {
        rows_ok = (AR == BR);
    } else {
        rows_ok = (a.rows() == b.rows());
    }

    if constexpr (AC != Eigen::Dynamic && BC != Eigen::Dynamic) {
        cols_ok = (AC == BC);
    } else {
        cols_ok = (a.cols() == b.cols());
    }

    return rows_ok && cols_ok;
}

}  // namespace dsplib
