#pragma once

#include <Eigen/Core>

#include <dsplib/tf/Dimension.hpp>

namespace dsplib {

/**
 * @brief Enum for encoding time domain in type information
 */
enum class TimeDomain { Discrete, Continuous };

/**
 * @brief A rational polynomial transfer function
 *
 * @tparam Scalar Type of scalar (float, double)
 * @tparam TimeDomain Discrete or continuous
 * @tparam NumeratorDim Dimensionality of the transfer function numerator
 * @tparam DenominatorDim Dimensionality of the transfer function denominator
 */
template <typename Scalar, TimeDomain Domain, int NumeratorDim = Eigen::Dynamic, int DenominatorDim = Eigen::Dynamic>
struct TransferFunction {
    Eigen::Vector<Scalar, NumeratorDim> numerator;
    Eigen::Vector<Scalar, DenominatorDim> denominator;

    /**
     * @brief Computes the reciprocol of the transfer function
     */
    TransferFunction<Scalar, Domain, DenominatorDim, NumeratorDim> Inverse() {
        return TransferFunction<Scalar, Domain, DenominatorDim, NumeratorDim>{.numerator = denominator,
                                                                              .denominator = numerator};
    }
};

}  // namespace dsplib
