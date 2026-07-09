#pragma once

#include <cstddef>

#include "pushfold/Binomial.hpp"
#include "pushfold/Constants.hpp"

namespace pushfold::threeway {

// A three-way deal collapses to an unordered multiset of three infosets (players
// can share an infoset), so the lookup tables are indexed by the colex rank of
// that multiset (see Colex.hpp). There are C(kNumInfosets + 2, 3) such multisets.
inline constexpr size_t kNumMatchupEntries = static_cast<size_t>(BinomialCoefficient(kNumInfosets + 2, 3));

}  // namespace pushfold::threeway
