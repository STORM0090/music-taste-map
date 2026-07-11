#pragma once

#include <array>
#include <vector>

#include "music_taste/feature_vector.hpp"

namespace music_taste {

// Z-score standardization. Features live in very different ranges
// (tempo 50-200 vs. danceability 0-1); without rescaling, distance
// metrics would be dominated by the widest-range feature.
//
// fit() learns per-feature mean/stddev, transform() applies them.
// The split allows learning statistics on one track set and applying
// them to another, keeping both in the same coordinate space.
class Normalizer {
public:
    // Uses only tracks with has_features == true. Throws
    // std::runtime_error if there are none.
    void fit(const std::vector<FeatureVector>& tracks);

    // Maps features to (value - mean) / stddev. Throws
    // std::runtime_error if fit() has not been called.
    void transform(std::vector<FeatureVector>& tracks) const;

    const std::array<double, kFeatureCount>& means() const { return means_; }
    const std::array<double, kFeatureCount>& stddevs() const { return stddevs_; }

private:
    std::array<double, kFeatureCount> means_{};
    std::array<double, kFeatureCount> stddevs_{};
    bool fitted_ = false;
};

}  // namespace music_taste
