#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "music_taste/csv_loader.hpp"
#include "music_taste/distance.hpp"
#include "music_taste/kmeans.hpp"
#include "music_taste/normalizer.hpp"

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0]
                   << " <path-to-tracks.csv> [clusters]\n";
        return 1;
    }
    std::size_t requested_k = 5;
    if (argc == 3) {
        requested_k = static_cast<std::size_t>(std::stoul(argv[2]));
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

        // Most and least similar pair across all tracks with features.
        std::size_t best_i = 0, best_j = 0, worst_i = 0, worst_j = 0;
        double best = -2.0;
        double worst = 2.0;
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            if (!tracks[i].has_features) {
                continue;
            }
            for (std::size_t j = i + 1; j < tracks.size(); ++j) {
                if (!tracks[j].has_features) {
                    continue;
                }
                const double s = music_taste::cosine_similarity(
                    tracks[i].features, tracks[j].features);
                if (s > best) {
                    best = s;
                    best_i = i;
                    best_j = j;
                }
                if (s < worst) {
                    worst = s;
                    worst_i = i;
                    worst_j = j;
                }
            }
        }
        if (best >= -1.0) {
            std::cout << "\nMost similar:  " << tracks[best_i].name << "  <->  "
                       << tracks[best_j].name << "  (cosine " << best << ")\n";
            std::cout << "Least similar: " << tracks[worst_i].name << "  <->  "
                       << tracks[worst_j].name << "  (cosine " << worst << ")\n";
        }

        // --- Clustering ------------------------------------------------
        if (with_features < 2) {
            std::cout << "\nClustering skipped: not enough tracks.\n";
            return 0;
        }
        const std::size_t k = std::min(requested_k, with_features);

        // Elbow data: inertia always drops as k grows; the "elbow"
        // where it stops dropping fast is a reasonable k to pick.
        std::cout << "\nInertia by cluster count:\n";
        for (std::size_t trial = 2; trial <= std::min<std::size_t>(8, with_features);
             ++trial) {
            music_taste::KMeansResult r = music_taste::run_kmeans(tracks, trial);
            std::cout << "  k=" << trial << "  inertia=" << r.inertia << '\n';
        }

        music_taste::KMeansResult clusters = music_taste::run_kmeans(tracks, k);
        std::cout << "\nClusters (k=" << k << ", converged in "
                   << clusters.iterations << " iterations):\n";

        for (std::size_t c = 0; c < k; ++c) {
            // Members of this cluster, sorted by distance to centroid.
            std::vector<std::pair<double, std::size_t>> members;
            for (std::size_t i = 0; i < tracks.size(); ++i) {
                if (clusters.cluster_of[i] == static_cast<int>(c)) {
                    members.push_back({music_taste::euclidean_distance(
                                           tracks[i].features,
                                           clusters.centroids[c]),
                                       i});
                }
            }
            std::sort(members.begin(), members.end());

            // The two most pronounced centroid features (largest |z|)
            // describe what makes this cluster distinct.
            std::size_t f1 = 0, f2 = 0;
            double a1 = -1.0, a2 = -1.0;
            for (std::size_t f = 0; f < music_taste::kFeatureCount; ++f) {
                const double a = std::fabs(clusters.centroids[c][f]);
                if (a > a1) {
                    a2 = a1;
                    f2 = f1;
                    a1 = a;
                    f1 = f;
                } else if (a > a2) {
                    a2 = a;
                    f2 = f;
                }
            }

            std::cout << "\n  Cluster " << c << " - " << members.size()
                       << " tracks, defined by " << music_taste::kFeatureNames[f1]
                       << " " << std::showpos << clusters.centroids[c][f1]
                       << ", " << music_taste::kFeatureNames[f2] << " "
                       << clusters.centroids[c][f2] << std::noshowpos << '\n';
            for (std::size_t m = 0; m < members.size() && m < 3; ++m) {
                const auto& t = tracks[members[m].second];
                std::cout << "    " << t.name << " -- " << t.artists << '\n';
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
