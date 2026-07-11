#include "music_taste/distance.hpp"

#include <cmath>

namespace music_taste {

double squared_euclidean_distance(const std::array<double, kFeatureCount>& a,
                                  const std::array<double, kFeatureCount>& b) {
    double sum = 0.0;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        const double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

double euclidean_distance(const std::array<double, kFeatureCount>& a,
                          const std::array<double, kFeatureCount>& b) {
    return std::sqrt(squared_euclidean_distance(a, b));
}

double cosine_similarity(const std::array<double, kFeatureCount>& a,
                         const std::array<double, kFeatureCount>& b) {
    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    // A zero vector has no direction; define its similarity as 0.
    // norm_a/norm_b hold squared lengths, hence the squared epsilon.
    if (norm_a < 1e-24 || norm_b < 1e-24) {
        return 0.0;
    }
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

}  // namespace music_taste
