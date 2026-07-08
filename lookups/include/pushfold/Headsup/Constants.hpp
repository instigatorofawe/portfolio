#pragma once

#include <cstddef>

#include "pushfold/Constants.hpp"

namespace pushfold::headsup {

// Storage sizes for the heads-up (pairwise) lookup tables. Both tables are
// symmetric 169x169 matrices stored as a flattened triangle: the equity table
// keeps the strict upper triangle (the diagonal is a known 0.5 and the lower
// triangle is the zero-sum complement), while the matchup table keeps the upper
// triangle including the diagonal (its diagonal carries real deal counts).
inline constexpr size_t kNumEquityMatchups = kNumInfosets * (kNumInfosets - 1) / 2;  // Size of equity lookup table
inline constexpr size_t kNumMatchupEntries = kNumInfosets * (kNumInfosets + 1) / 2;  // Size of matchup lookup table

}  // namespace pushfold::headsup
