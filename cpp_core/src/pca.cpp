#include "music_taste/pca.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>

namespace music_taste {

using Vector = std::array<double, kFeatureCount>;
using Matrix = std::array<Vector, kFeatureCount>;

double dot(const Vector& a, const Vector& b) {
    double sum = 0.0;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

Vector multiply(const Matrix& m, const Vector& v) {
    Vector out{};
    for (std::size_t r = 0; r < kFeatureCount; ++r) {
        out[r] = dot(m[r], v);
    }
    return out;
}

// Dominant eigenvector of a symmetric matrix by power iteration:
// multiplying any vector by the matrix over and over rotates it
// towards the direction the matrix stretches the most. Returns that
// direction (unit length) and its eigenvalue - for a covariance
// matrix, the variance along it.
std::pair<Vector, double> power_iteration(const Matrix& m, std::mt19937& rng) {
    // Random unit start: a fixed start vector could happen to be
    // exactly perpendicular to the answer and would never rotate
    // towards it.
    std::normal_distribution<double> gauss(0.0, 1.0);
    Vector v;
    for (double& x : v) {
        x = gauss(rng);
    }
    const double start_len = std::sqrt(dot(v, v));
    for (double& x : v) {
        x /= start_len;
    }

    for (int it = 0; it < 1000; ++it) {
        Vector next = multiply(m, v);
        const double len = std::sqrt(dot(next, next));
        if (len < 1e-12) {
            // The matrix is (numerically) zero - no variance at all,
            // any direction is as good as any other.
            return {v, 0.0};
        }
        for (double& x : next) {
            x /= len;
        }
        // Cosine between successive directions; |cos| -> 1 means the
        // direction stopped changing (the sign may keep flipping).
        const double cosine = dot(next, v);
        v = next;
        if (std::fabs(cosine) > 1.0 - 1e-12) {
            break;
        }
    }

    // Rayleigh quotient: the variance of the data along v.
    return {v, dot(multiply(m, v), v)};
}

PcaResult project_to_2d(const std::vector<FeatureVector>& tracks) {
    std::vector<std::size_t> points;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].has_features) {
            points.push_back(i);
        }
    }
    if (points.empty()) {
        throw std::runtime_error(
            "project_to_2d: no track has audio features");
    }
    const double n = static_cast<double>(points.size());

    // Center the data: PCA measures variance around the mean, so the
    // mean must sit at the origin. (After z-score normalization it
    // already almost does; centering again keeps the function correct
    // on raw data too.)
    Vector mean{};
    for (std::size_t idx : points) {
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            mean[i] += tracks[idx].features[i];
        }
    }
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        mean[i] /= n;
    }

    // Covariance matrix: cov[a][b] = average co-movement of features
    // a and b around their means. Symmetric; its eigenvectors are the
    // directions of greatest variance.
    Matrix cov{};
    for (std::size_t idx : points) {
        Vector d;
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            d[i] = tracks[idx].features[i] - mean[i];
        }
        for (std::size_t a = 0; a < kFeatureCount; ++a) {
            for (std::size_t b = 0; b < kFeatureCount; ++b) {
                cov[a][b] += d[a] * d[b];
            }
        }
    }
    for (std::size_t a = 0; a < kFeatureCount; ++a) {
        for (std::size_t b = 0; b < kFeatureCount; ++b) {
            cov[a][b] /= n;
        }
    }

    // Total variance is the sum of the diagonal - the denominator for
    // the "explained" shares.
    double total_variance = 0.0;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        total_variance += cov[i][i];
    }

    std::mt19937 rng(42);
    auto [axis1, lambda1] = power_iteration(cov, rng);

    // Deflation: subtract the found component from the matrix, so the
    // next power iteration converges to the second-strongest
    // direction instead of the same one again.
    Matrix deflated = cov;
    for (std::size_t a = 0; a < kFeatureCount; ++a) {
        for (std::size_t b = 0; b < kFeatureCount; ++b) {
            deflated[a][b] -= lambda1 * axis1[a] * axis1[b];
        }
    }
    auto [axis2, lambda2] = power_iteration(deflated, rng);

    // Numerical insurance: in exact arithmetic axis2 is already
    // orthogonal to axis1; remove the tiny leftover overlap caused by
    // rounding and restore unit length.
    const double overlap = dot(axis2, axis1);
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        axis2[i] -= overlap * axis1[i];
    }
    const double len2 = std::sqrt(dot(axis2, axis2));
    if (len2 > 1e-12) {
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            axis2[i] /= len2;
        }
    }

    PcaResult result;
    result.axis1 = axis1;
    result.axis2 = axis2;
    if (total_variance > 1e-12) {
        result.explained1 = std::max(0.0, lambda1 / total_variance);
        result.explained2 = std::max(0.0, lambda2 / total_variance);
    }
    result.coords.assign(tracks.size(), {0.0, 0.0});
    for (std::size_t idx : points) {
        Vector d;
        for (std::size_t i = 0; i < kFeatureCount; ++i) {
            d[i] = tracks[idx].features[i] - mean[i];
        }
        result.coords[idx] = {dot(d, axis1), dot(d, axis2)};
    }
    return result;
}

}  // namespace music_taste
