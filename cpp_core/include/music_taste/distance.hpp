#pragma once

#include <array>

#include "music_taste/feature_vector.hpp"

namespace music_taste {

// Straight-line distance in feature space. Assumes the features are
// already normalized (see Normalizer), otherwise wide-range features
// dominate the result.
double euclidean_distance(const std::array<double, kFeatureCount>& a,
                          const std::array<double, kFeatureCount>& b);

// Same without the square root - cheaper, and sufficient wherever
// distances are only compared with each other (e.g. k-means).
double squared_euclidean_distance(const std::array<double, kFeatureCount>& a,
                                  const std::array<double, kFeatureCount>& b);

// Cosine of the angle between two vectors: 1 = same direction,
// 0 = unrelated, -1 = opposite. Returns 0 when either vector has zero
// length (the angle is undefined there).
double cosine_similarity(const std::array<double, kFeatureCount>& a,
                         const std::array<double, kFeatureCount>& b);

}  // namespace music_taste
