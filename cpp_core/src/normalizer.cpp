#include "music_taste/normalizer.hpp"

#include <cmath>
#include <stdexcept>

namespace music_taste {

void Normalizer::fit(const std::vector<FeatureVector>& tracks) {
    // Featureless tracks hold placeholder zeros; letting them in would
    // skew the statistics.
    std::size_t n = 0;
    for (const FeatureVector& track : tracks) {
        if (track.has_features) {
            ++n;
        }
    }
    if (n == 0) {
        throw std::runtime_error(
            "Normalizer::fit: no track has audio features");
    }

    means_.fill(0.0);
    for (const FeatureVector& track : tracks) {
        if (!track.has_features) {
            continue;
        }
        for (std::size_t f = 0; f < kFeatureCount; ++f) {
            means_[f] += track.features[f];
        }
    }
    for (std::size_t f = 0; f < kFeatureCount; ++f) {
        means_[f] /= static_cast<double>(n);
    }

    // Two-pass variance: numerically more stable than the single-pass
    // E[x^2] - (E[x])^2 form.
    stddevs_.fill(0.0);
    for (const FeatureVector& track : tracks) {
        if (!track.has_features) {
            continue;
        }
        for (std::size_t f = 0; f < kFeatureCount; ++f) {
            const double diff = track.features[f] - means_[f];
            stddevs_[f] += diff * diff;
        }
    }
    for (std::size_t f = 0; f < kFeatureCount; ++f) {
        stddevs_[f] = std::sqrt(stddevs_[f] / static_cast<double>(n));

        // A constant feature (zero variance) carries no information;
        // substituting stddev = 1 maps it to 0 for every track instead
        // of dividing by zero.
        if (stddevs_[f] < 1e-12) {
            stddevs_[f] = 1.0;
        }
    }

    fitted_ = true;
}

void Normalizer::transform(std::vector<FeatureVector>& tracks) const {
    if (!fitted_) {
        throw std::runtime_error(
            "Normalizer::transform: fit() must be called first");
    }

    for (FeatureVector& track : tracks) {
        if (!track.has_features) {
            continue;
        }
        for (std::size_t f = 0; f < kFeatureCount; ++f) {
            track.features[f] = (track.features[f] - means_[f]) / stddevs_[f];
        }
    }
}

}  // namespace music_taste
