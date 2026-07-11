#include "music_taste/kmeans.hpp"

#include <limits>
#include <random>
#include <stdexcept>

#include "music_taste/distance.hpp"

namespace music_taste {

// Index of the centroid nearest to the given point.
std::size_t nearest_centroid(
    const std::array<double, kFeatureCount>& point,
    const std::vector<std::array<double, kFeatureCount>>& centroids) {
    std::size_t best = 0;
    double best_dist = std::numeric_limits<double>::max();
    for (std::size_t c = 0; c < centroids.size(); ++c) {
        const double d = squared_euclidean_distance(point, centroids[c]);
        if (d < best_dist) {
            best_dist = d;
            best = c;
        }
    }
    return best;
}

KMeansResult run_kmeans(const std::vector<FeatureVector>& tracks,
                        std::size_t k, unsigned seed,
                        std::size_t max_iterations) {
    // Only tracks with features are clustered.
    std::vector<std::size_t> points;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].has_features) {
            points.push_back(i);
        }
    }
    if (k == 0 || k > points.size()) {
        throw std::runtime_error(
            "run_kmeans: k must be between 1 and the number of tracks "
            "with features");
    }

    std::mt19937 rng(seed);

    // --- k-means++ seeding -------------------------------------------
    // Plain random starting centroids often land in the same dense area
    // and produce a poor local optimum. k-means++ picks each next
    // centroid with probability proportional to the squared distance
    // from the already chosen ones - spreading the starts out.
    std::vector<std::array<double, kFeatureCount>> centroids;
    centroids.reserve(k);
    {
        std::uniform_int_distribution<std::size_t> uniform(0, points.size() - 1);
        centroids.push_back(tracks[points[uniform(rng)]].features);
    }
    std::vector<double> dist2(points.size(), 0.0);
    while (centroids.size() < k) {
        double total = 0.0;
        for (std::size_t p = 0; p < points.size(); ++p) {
            double best = std::numeric_limits<double>::max();
            for (const auto& c : centroids) {
                const double d =
                    squared_euclidean_distance(tracks[points[p]].features, c);
                if (d < best) {
                    best = d;
                }
            }
            dist2[p] = best;
            total += best;
        }
        std::uniform_real_distribution<double> pick(0.0, total);
        double r = pick(rng);
        std::size_t chosen = points.size() - 1;
        double acc = 0.0;
        for (std::size_t p = 0; p < points.size(); ++p) {
            acc += dist2[p];
            if (acc >= r) {
                chosen = p;
                break;
            }
        }
        centroids.push_back(tracks[points[chosen]].features);
    }

    // --- Lloyd iterations --------------------------------------------
    // Repeat two steps until stable: assign every point to its nearest
    // centroid, then move every centroid to the mean of its points.
    std::vector<int> assign(points.size(), -1);
    int iterations = 0;

    for (std::size_t it = 0; it < max_iterations; ++it) {
        ++iterations;

        bool changed = false;
        for (std::size_t p = 0; p < points.size(); ++p) {
            const int c = static_cast<int>(
                nearest_centroid(tracks[points[p]].features, centroids));
            if (c != assign[p]) {
                assign[p] = c;
                changed = true;
            }
        }
        if (!changed) {
            break;  // assignments stable - converged
        }

        std::vector<std::array<double, kFeatureCount>> sums(
            k, std::array<double, kFeatureCount>{});
        std::vector<std::size_t> counts(k, 0);
        for (std::size_t p = 0; p < points.size(); ++p) {
            const auto& f = tracks[points[p]].features;
            auto& s = sums[static_cast<std::size_t>(assign[p])];
            for (std::size_t i = 0; i < kFeatureCount; ++i) {
                s[i] += f[i];
            }
            ++counts[static_cast<std::size_t>(assign[p])];
        }
        for (std::size_t c = 0; c < k; ++c) {
            if (counts[c] == 0) {
                // A cluster lost all its points (rare with k-means++).
                // Restart it at the point that fits its current cluster
                // worst - the farthest one from its own centroid.
                std::size_t worst = 0;
                double worst_dist = -1.0;
                for (std::size_t p = 0; p < points.size(); ++p) {
                    const double d = squared_euclidean_distance(
                        tracks[points[p]].features,
                        centroids[static_cast<std::size_t>(assign[p])]);
                    if (d > worst_dist) {
                        worst_dist = d;
                        worst = p;
                    }
                }
                centroids[c] = tracks[points[worst]].features;
                continue;
            }
            for (std::size_t i = 0; i < kFeatureCount; ++i) {
                centroids[c][i] =
                    sums[c][i] / static_cast<double>(counts[c]);
            }
        }
    }

    // --- Result --------------------------------------------------------
    KMeansResult result;
    result.centroids = centroids;
    result.iterations = iterations;
    result.cluster_of.assign(tracks.size(), -1);
    for (std::size_t p = 0; p < points.size(); ++p) {
        result.cluster_of[points[p]] = assign[p];
        result.inertia += squared_euclidean_distance(
            tracks[points[p]].features,
            centroids[static_cast<std::size_t>(assign[p])]);
    }
    return result;
}

}  // namespace music_taste
