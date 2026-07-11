#include <iomanip>
#include <iostream>
#include <string>

#include "music_taste/csv_loader.hpp"
#include "music_taste/normalizer.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-tracks.csv>\n";
        return 1;
    }

    try {
        auto tracks = music_taste::load_tracks_csv(argv[1]);

        std::size_t with_features = 0;
        for (const auto& track : tracks) {
            if (track.has_features) {
                ++with_features;
            }
        }

        std::cout << "Loaded tracks: " << tracks.size() << '\n';
        std::cout << "With a complete set of audio features: " << with_features
                   << '\n';

        if (with_features == 0) {
            std::cout << "\nNormalization skipped: no tracks with features.\n";
            return 0;
        }

        music_taste::Normalizer normalizer;
        normalizer.fit(tracks);

        std::cout << "\nFeature statistics before normalization:\n";
        std::cout << std::fixed << std::setprecision(3);
        for (std::size_t f = 0; f < music_taste::kFeatureCount; ++f) {
            std::cout << "  " << std::setw(16) << std::left
                       << music_taste::kFeatureNames[f]
                       << " mean = " << std::setw(9) << std::right
                       << normalizer.means()[f]
                       << "  stddev = " << normalizer.stddevs()[f] << '\n';
        }

        std::size_t demo = 0;
        while (demo < tracks.size() && !tracks[demo].has_features) {
            ++demo;
        }
        std::cout << "\nExample (" << tracks[demo].name << "):\n";
        std::cout << "  before: energy=" << tracks[demo].features[1]
                   << "  tempo=" << tracks[demo].features[3] << '\n';

        normalizer.transform(tracks);

        std::cout << "  after:  energy=" << tracks[demo].features[1]
                   << "  tempo=" << tracks[demo].features[3] << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
