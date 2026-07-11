#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "music_taste/feature_vector.hpp"

namespace music_taste {

struct KMeansResult {
    // cluster_of[i] is the cluster of tracks[i], or -1 for tracks
    // without features (they do not participate in clustering).
    std::vector<int> cluster_of;

    std::vector<std::array<double, kFeatureCount>> centroids;

    // Sum of squared distances from every point to its own centroid -
    // the standard "how tight are the clusters" measure. Comparing it
    // across different k values gives the elbow curve.
    double inertia = 0.0;

    int iterations = 0;  // assignment passes until convergence
};

// Lloyd's k-means with k-means++ seeding over normalized features.
// Deterministic for a fixed seed. Throws std::runtime_error when k is
// zero or exceeds the number of tracks that have features.
KMeansResult run_kmeans(const std::vector<FeatureVector>& tracks,
                        std::size_t k, unsigned seed = 42,
                        std::size_t max_iterations = 100);

}  // namespace music_taste
