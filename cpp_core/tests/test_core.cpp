// Minimal dependency-free test runner: every CHECK reports a line on
// failure, main() prints a summary and returns non-zero if anything
// failed (so ctest can pick it up).
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "music_taste/csv_loader.hpp"
#include "music_taste/distance.hpp"
#include "music_taste/kmeans.hpp"
#include "music_taste/normalizer.hpp"
#include "music_taste/pca.hpp"

int checks_run = 0;
int checks_failed = 0;

#define CHECK(condition)                                                     \
    do {                                                                     \
        ++checks_run;                                                        \
        if (!(condition)) {                                                  \
            ++checks_failed;                                                 \
            std::cerr << "FAILED: " << #condition << "\n  at " << __FILE__   \
                       << ":" << __LINE__ << "\n";                           \
        }                                                                    \
    } while (false)

#define CHECK_THROWS(expression, ExceptionType)                              \
    do {                                                                     \
        ++checks_run;                                                        \
        bool threw = false;                                                  \
        try {                                                                \
            (void)(expression);                                              \
        } catch (const ExceptionType&) {                                     \
            threw = true;                                                    \
        }                                                                    \
        if (!threw) {                                                        \
            ++checks_failed;                                                 \
            std::cerr << "FAILED: expected " << #expression << " to throw "  \
                       << #ExceptionType << "\n  at " << __FILE__ << ":"     \
                       << __LINE__ << "\n";                                  \
        }                                                                    \
    } while (false)

bool approx(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

// Must match the columns written by get_playlist_features.py; if the
// format changes, these tests are meant to break.
const std::string kHeader =
    "name,artists,album,release_date,popularity,duration_ms,id,"
    "danceability,energy,valence,tempo,acousticness,instrumentalness,"
    "speechiness,liveness,key,mode,time_signature";

void test_loader() {
    const std::string path = "test_tmp.csv";

    // BOM, CRLF line endings, quoted comma, escaped quotes, and a row
    // with all feature cells empty.
    write_file(path,
               "\xEF\xBB\xBF" + kHeader + "\r\n" +
                   "\"Song, with comma\",\"Artist \"\"X\"\"\",Album,"
                   "2020-01-01,50,200000,id1,"
                   "0.5,0.6,0.7,120,0.1,0,0.05,0.15,5,1,4\r\n" +
                   "Plain Song,Artist2,Album2,2021,,180000,id2,"
                   ",,,,,,,,,,\r\n");

    std::vector<music_taste::FeatureVector> tracks =
        music_taste::load_tracks_csv(path);

    CHECK(tracks.size() == 2);
    CHECK(tracks[0].name == "Song, with comma");
    CHECK(tracks[0].artists == "Artist \"X\"");
    CHECK(tracks[0].id == "id1");
    CHECK(tracks[0].has_features);
    CHECK(approx(tracks[0].features[0], 0.5));    // danceability
    CHECK(approx(tracks[0].features[3], 120.0));  // tempo
    CHECK(approx(tracks[0].features[8], 5.0));    // key
    CHECK(!tracks[1].has_features);
    CHECK(approx(tracks[1].features[0], 0.0));

    // Unexpected header -> error.
    write_file(path, "totally,wrong,header\n1,2,3\n");
    CHECK_THROWS(music_taste::load_tracks_csv(path), music_taste::CsvLoadError);

    // Row with the wrong number of fields -> error.
    write_file(path, kHeader + "\nonly,three,fields\n");
    CHECK_THROWS(music_taste::load_tracks_csv(path), music_taste::CsvLoadError);

    // Missing file -> error.
    CHECK_THROWS(music_taste::load_tracks_csv("no_such_file.csv"),
                 music_taste::CsvLoadError);

    std::remove(path.c_str());
}

music_taste::FeatureVector make_track(const std::string& id, double value) {
    music_taste::FeatureVector t;
    t.id = id;
    t.name = id;
    t.has_features = true;
    t.features.fill(value);
    return t;
}

void test_normalizer() {
    // Two tracks with every feature 1 and 3: mean 2, stddev 1, so the
    // normalized values must be exactly -1 and +1.
    std::vector<music_taste::FeatureVector> tracks = {make_track("a", 1.0),
                                                      make_track("b", 3.0)};

    // Feature 0 is constant across tracks: the zero-variance guard
    // must map it to 0 instead of dividing by zero.
    tracks[0].features[0] = 5.0;
    tracks[1].features[0] = 5.0;

    // A featureless track must not affect statistics and must stay
    // untouched by transform().
    music_taste::FeatureVector ghost;
    ghost.id = "ghost";
    ghost.features.fill(100.0);
    ghost.has_features = false;
    tracks.push_back(ghost);

    music_taste::Normalizer norm;
    norm.fit(tracks);

    CHECK(approx(norm.means()[1], 2.0));
    CHECK(approx(norm.stddevs()[1], 1.0));
    CHECK(approx(norm.means()[0], 5.0));
    CHECK(approx(norm.stddevs()[0], 1.0));  // zero-variance guard

    norm.transform(tracks);

    CHECK(approx(tracks[0].features[1], -1.0));
    CHECK(approx(tracks[1].features[1], 1.0));
    CHECK(approx(tracks[0].features[0], 0.0));    // constant feature
    CHECK(approx(tracks[2].features[5], 100.0));  // ghost untouched

    // No tracks with features -> fit refuses.
    std::vector<music_taste::FeatureVector> empty;
    music_taste::Normalizer norm2;
    CHECK_THROWS(norm2.fit(empty), std::runtime_error);

    // transform before fit -> error.
    music_taste::Normalizer norm3;
    CHECK_THROWS(norm3.transform(tracks), std::runtime_error);
}

void test_distance() {
    std::array<double, music_taste::kFeatureCount> a{};
    std::array<double, music_taste::kFeatureCount> b{};

    // Identical vectors are at distance 0.
    a.fill(1.0);
    b.fill(1.0);
    CHECK(approx(music_taste::euclidean_distance(a, b), 0.0));

    // Classic 3-4-5 triangle in the first two dimensions.
    a.fill(0.0);
    b.fill(0.0);
    b[0] = 3.0;
    b[1] = 4.0;
    CHECK(approx(music_taste::euclidean_distance(a, b), 5.0));

    // Cosine: same direction -> 1, orthogonal -> 0, opposite -> -1.
    a.fill(0.0);
    b.fill(0.0);
    a[0] = 1.0;
    b[0] = 2.0;
    CHECK(approx(music_taste::cosine_similarity(a, b), 1.0));
    b[0] = 0.0;
    b[1] = 1.0;
    CHECK(approx(music_taste::cosine_similarity(a, b), 0.0));
    b[0] = -1.0;
    b[1] = 0.0;
    CHECK(approx(music_taste::cosine_similarity(a, b), -1.0));

    // Zero vector has no direction: similarity defined as 0.
    b.fill(0.0);
    CHECK(approx(music_taste::cosine_similarity(a, b), 0.0));

    // Squared distance: same 3-4-5 triangle without the square root.
    a.fill(0.0);
    b.fill(0.0);
    b[0] = 3.0;
    b[1] = 4.0;
    CHECK(approx(music_taste::squared_euclidean_distance(a, b), 25.0));
}

music_taste::FeatureVector point_track(const std::string& id, double x,
                                       double y) {
    music_taste::FeatureVector t;
    t.id = id;
    t.name = id;
    t.has_features = true;
    t.features.fill(0.0);
    t.features[0] = x;
    t.features[1] = y;
    return t;
}

void test_kmeans() {
    // Two tight groups far apart, plus a featureless track in between.
    std::vector<music_taste::FeatureVector> tracks;
    tracks.push_back(point_track("a1", 0.0, 0.0));
    tracks.push_back(point_track("a2", 0.1, 0.0));
    tracks.push_back(point_track("a3", 0.0, 0.1));
    music_taste::FeatureVector ghost;
    ghost.id = "ghost";
    ghost.has_features = false;
    tracks.push_back(ghost);
    tracks.push_back(point_track("b1", 10.0, 10.0));
    tracks.push_back(point_track("b2", 10.1, 10.0));
    tracks.push_back(point_track("b3", 10.0, 10.1));

    music_taste::KMeansResult res = music_taste::run_kmeans(tracks, 2);

    CHECK(res.cluster_of.size() == tracks.size());
    CHECK(res.cluster_of[3] == -1);  // ghost is not clustered

    // Each group ends up together, and the groups differ.
    CHECK(res.cluster_of[0] == res.cluster_of[1]);
    CHECK(res.cluster_of[1] == res.cluster_of[2]);
    CHECK(res.cluster_of[4] == res.cluster_of[5]);
    CHECK(res.cluster_of[5] == res.cluster_of[6]);
    CHECK(res.cluster_of[0] != res.cluster_of[4]);

    // Tight, well-separated groups: tiny inertia with k=2, huge with 1.
    CHECK(res.inertia < 0.1);
    music_taste::KMeansResult res1 = music_taste::run_kmeans(tracks, 1);
    CHECK(res1.inertia > 100.0);
    CHECK(res1.cluster_of[0] == 0);
    CHECK(res1.cluster_of[6] == 0);

    // Same seed - same result.
    music_taste::KMeansResult res_again = music_taste::run_kmeans(tracks, 2);
    CHECK(res_again.cluster_of == res.cluster_of);

    // Invalid k: zero, or more clusters than tracks with features.
    CHECK_THROWS(music_taste::run_kmeans(tracks, 0), std::runtime_error);
    CHECK_THROWS(music_taste::run_kmeans(tracks, 7), std::runtime_error);
}

void test_pca() {
    // Nearly all variance sits along feature 0, a sliver along
    // feature 1, none anywhere else.
    const double xs[] = {-10.0, -5.0, 0.0, 5.0, 10.0};
    const double ys[] = {0.1, -0.1, 0.0, 0.1, -0.1};
    std::vector<music_taste::FeatureVector> tracks;
    for (int i = 0; i < 5; ++i) {
        tracks.push_back(point_track("p" + std::to_string(i), xs[i], ys[i]));
    }
    music_taste::FeatureVector ghost;
    ghost.id = "ghost";
    ghost.has_features = false;
    tracks.push_back(ghost);

    music_taste::PcaResult res = music_taste::project_to_2d(tracks);

    CHECK(res.coords.size() == tracks.size());
    CHECK(approx(res.coords[5][0], 0.0));  // ghost sits at the origin
    CHECK(approx(res.coords[5][1], 0.0));

    // The first axis must align with feature 0 (sign is arbitrary).
    CHECK(std::fabs(res.axis1[0]) > 0.999);
    CHECK(std::fabs(res.axis1[1]) < 0.05);

    // Axes are unit length and orthogonal.
    double d = 0.0, len1 = 0.0, len2 = 0.0;
    for (std::size_t i = 0; i < music_taste::kFeatureCount; ++i) {
        d += res.axis1[i] * res.axis2[i];
        len1 += res.axis1[i] * res.axis1[i];
        len2 += res.axis2[i] * res.axis2[i];
    }
    CHECK(std::fabs(d) < 1e-6);
    CHECK(approx(len1, 1.0, 1e-6));
    CHECK(approx(len2, 1.0, 1e-6));

    // The x coordinates reproduce the spread along feature 0.
    CHECK(approx(std::fabs(res.coords[0][0]), 10.0, 0.01));
    CHECK(approx(std::fabs(res.coords[4][0]), 10.0, 0.01));

    // The first axis dominates the variance.
    CHECK(res.explained1 > 0.99);
    CHECK(res.explained2 >= 0.0);
    CHECK(res.explained1 >= res.explained2);

    // Deterministic.
    music_taste::PcaResult res2 = music_taste::project_to_2d(tracks);
    CHECK(res2.coords == res.coords);

    // No tracks with features -> error.
    std::vector<music_taste::FeatureVector> empty;
    CHECK_THROWS(music_taste::project_to_2d(empty), std::runtime_error);
}

int main() {
    test_loader();
    test_normalizer();
    test_distance();
    test_kmeans();
    test_pca();

    std::cout << checks_run << " checks, " << checks_failed << " failed\n";
    return checks_failed == 0 ? 0 : 1;
}
