#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "music_taste/feature_vector.hpp"

namespace music_taste {

struct PcaResult {
    // 2D map coordinates aligned with the input: coords[i] belongs to
    // tracks[i]. Tracks without features sit at (0, 0) - filter them
    // out with has_features on the consumer side.
    std::vector<std::array<double, 2>> coords;

    // The two directions in feature space the data is projected onto
    // (unit length, mutually orthogonal). Their entries show how much
    // each original feature contributes to a map axis.
    std::array<double, kFeatureCount> axis1{};
    std::array<double, kFeatureCount> axis2{};

    // Share of total variance captured by each axis, in [0, 1]. High
    // values mean the flat map is a faithful picture of the
    // 11-dimensional cloud.
    double explained1 = 0.0;
    double explained2 = 0.0;
};

// Principal component analysis via power iteration: projects the
// normalized feature vectors onto the two directions of greatest
// variance. Deterministic. Throws std::runtime_error if no track has
// features.
PcaResult project_to_2d(const std::vector<FeatureVector>& tracks);

}  // namespace music_taste
