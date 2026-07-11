#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "music_taste/feature_vector.hpp"

namespace music_taste {

// Thrown when the CSV cannot be loaded: missing file, unexpected
// header, corrupted row.
class CsvLoadError : public std::runtime_error {
public:
    CsvLoadError(const std::string& message) : std::runtime_error(message) {}
};

// Reads the CSV produced by get_playlist_features.py. Handles quoted
// fields: titles may contain commas and escaped quotes.
std::vector<FeatureVector> load_tracks_csv(const std::string& path);

}  // namespace music_taste
