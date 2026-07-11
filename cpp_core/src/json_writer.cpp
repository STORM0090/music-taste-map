#include "music_taste/json_writer.hpp"

#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace music_taste {

// JSON string escaping. Only quotes, backslashes and control
// characters need it; UTF-8 text (Cyrillic, Japanese, ...) is valid
// in JSON as-is and passes through untouched.
std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Remaining control characters must be written as
                    // \u00XX escapes to keep the JSON valid.
                    std::ostringstream hex;
                    hex << "\\u" << std::hex << std::setw(4)
                        << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                    out += hex.str();
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

// {"explained": 0.24, "loadings": {"danceability": 0.12, ...}}
void write_axis(std::ostream& out, const char* name,
                const std::array<double, kFeatureCount>& axis,
                double explained) {
    out << "    \"" << name << "\": {\"explained\": " << explained
        << ", \"loadings\": {";
    for (std::size_t f = 0; f < kFeatureCount; ++f) {
        if (f > 0) {
            out << ", ";
        }
        out << "\"" << kFeatureNames[f] << "\": " << axis[f];
    }
    out << "}}";
}

void write_map_json(const std::string& path,
                    const std::vector<FeatureVector>& tracks,
                    const KMeansResult& clusters,
                    const PcaResult& map) {
    // All three inputs must describe the same track list; a mismatch
    // means the caller mixed results from different runs.
    if (clusters.cluster_of.size() != tracks.size() ||
        map.coords.size() != tracks.size()) {
        throw std::runtime_error(
            "write_map_json: tracks, clusters and map sizes do not match");
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("write_map_json: cannot open " + path);
    }
    out << std::fixed << std::setprecision(4);

    std::size_t mapped = 0;
    for (const FeatureVector& t : tracks) {
        if (t.has_features) {
            ++mapped;
        }
    }

    out << "{\n";
    out << "  \"track_count\": " << tracks.size() << ",\n";
    out << "  \"mapped_count\": " << mapped << ",\n";

    out << "  \"axes\": {\n";
    write_axis(out, "x", map.axis1, map.explained1);
    out << ",\n";
    write_axis(out, "y", map.axis2, map.explained2);
    out << "\n  },\n";

    out << "  \"clusters\": [\n";
    for (std::size_t c = 0; c < clusters.centroids.size(); ++c) {
        std::size_t size = 0;
        for (int assigned : clusters.cluster_of) {
            if (assigned == static_cast<int>(c)) {
                ++size;
            }
        }
        out << "    {\"id\": " << c << ", \"size\": " << size
            << ", \"centroid\": {";
        for (std::size_t f = 0; f < kFeatureCount; ++f) {
            if (f > 0) {
                out << ", ";
            }
            out << "\"" << kFeatureNames[f]
                << "\": " << clusters.centroids[c][f];
        }
        out << "}}";
        out << (c + 1 < clusters.centroids.size() ? ",\n" : "\n");
    }
    out << "  ],\n";

    // Tracks without features have no coordinates and no cluster -
    // they are simply not part of the map.
    out << "  \"tracks\": [\n";
    bool first = true;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (!tracks[i].has_features) {
            continue;
        }
        if (!first) {
            out << ",\n";
        }
        first = false;
        out << "    {\"id\": \"" << escape_json(tracks[i].id) << "\", "
            << "\"name\": \"" << escape_json(tracks[i].name) << "\", "
            << "\"artists\": \"" << escape_json(tracks[i].artists) << "\", "
            << "\"x\": " << map.coords[i][0] << ", "
            << "\"y\": " << map.coords[i][1] << ", "
            << "\"cluster\": " << clusters.cluster_of[i] << "}";
    }
    out << "\n  ]\n";
    out << "}\n";

    // Catches disk-full and similar failures that individual << calls
    // silently record in the stream state.
    if (!out) {
        throw std::runtime_error("write_map_json: write failed for " + path);
    }
}

}  // namespace music_taste
