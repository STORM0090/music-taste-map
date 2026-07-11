#pragma once

#include <string>
#include <vector>

#include "music_taste/feature_vector.hpp"
#include "music_taste/kmeans.hpp"
#include "music_taste/pca.hpp"

namespace music_taste {

// Writes everything the star-map frontend needs into one JSON file:
// the two PCA axes (explained variance + per-feature loadings), the
// clusters (size + centroid), and one entry per track that has
// features (metadata, 2D coordinates, cluster id).
//
// Throws std::runtime_error if the file cannot be written or if the
// three inputs do not belong to the same track list.
void write_map_json(const std::string& path,
                    const std::vector<FeatureVector>& tracks,
                    const KMeansResult& clusters,
                    const PcaResult& map);

}  // namespace music_taste
