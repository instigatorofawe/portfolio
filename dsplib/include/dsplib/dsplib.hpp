#pragma once

#include <cmath>

// Header-only core of dsplib. Downstream code (the nanobind bindings, tests,
// benchmarks) includes this via the `dsplib::core` interface target.
namespace dsplib {

// Convert a gain expressed in decibels to a linear amplitude ratio.
inline double db_to_linear(double db) { return std::pow(10.0, db / 20.0); }

// Convert a linear amplitude ratio to a gain in decibels.
inline double linear_to_db(double linear) { return 20.0 * std::log10(linear); }

}  // namespace dsplib
