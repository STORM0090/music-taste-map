#include <iomanip>
#include <iostream>
#include <string>

#include "music_taste/csv_loader.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-tracks.csv>\n";
        return 1;
    }

    try {
        const auto tracks = music_taste::load_tracks_csv(argv[1]);

        std::size_t with_features = 0;
        for (const auto& track : tracks) {
            if (track.has_features) {
                ++with_features;
            }
        }

        std::cout << "Loaded tracks: " << tracks.size() << '\n';
        std::cout << "With a complete set of audio features: " << with_features
                   << '\n';

        std::cout << "\nFirst tracks:\n";
        std::cout << std::fixed << std::setprecision(3);
        for (std::size_t i = 0; i < tracks.size() && i < 5; ++i) {
            const auto& t = tracks[i];
            std::cout << "  " << t.name << " -- " << t.artists;
            if (t.has_features) {
                std::cout << " (energy=" << t.features[1]
                           << ", valence=" << t.features[2] << ")";
            } else {
                std::cout << " (no audio features)";
            }
            std::cout << '\n';
        }
    } catch (const music_taste::CsvLoadError& e) {
        std::cerr << "Failed to load CSV: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
