#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace music_taste {

// Audio feature names, in the same order as the CSV columns produced
// by get_playlist_features.py.
// loudness is intentionally absent: the offline feature datasets do
// not provide it, and it strongly correlates with energy anyway.
inline constexpr std::array<const char*, 11> kFeatureNames = {
    "danceability", "energy",           "valence",     "tempo",
    "acousticness", "instrumentalness", "speechiness", "liveness",
    "key",          "mode",             "time_signature",
};

inline constexpr std::size_t kFeatureCount = kFeatureNames.size();

// One playlist track: metadata + numeric audio features.
struct FeatureVector {
    std::string id;
    std::string name;
    std::string artists;

    std::array<double, kFeatureCount> features{};

    // Distinguishes "features not computed" from "all features are 0".
    bool has_features = false;
};

}  // namespace music_taste
