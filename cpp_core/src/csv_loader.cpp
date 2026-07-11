#include "music_taste/csv_loader.hpp"

#include <fstream>
#include <sstream>

namespace music_taste {

std::vector<std::string> expected_header() {
    std::vector<std::string> header = {"name",         "artists",
                                        "album",        "release_date",
                                        "popularity",   "duration_ms",
                                        "id"};
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        header.push_back(kFeatureNames[i]);
    }
    return header;
}

// Character-by-character CSV parser. A naive comma split would break
// on quoted fields: titles may contain commas, and a literal quote is
// encoded as "" inside a quoted field (RFC 4180).
std::vector<std::vector<std::string>> parse_csv(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> current_row;
    std::string current_field;
    bool in_quotes = false;

    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        if (in_quotes) {
            if (c != '"') {
                current_field += c;
            } else if (i + 1 < text.size() && text[i + 1] == '"') {
                current_field += '"';  // escaped quote
                ++i;
            } else {
                in_quotes = false;
            }
            continue;
        }

        switch (c) {
            case '"':
                in_quotes = true;
                break;
            case ',':
                current_row.push_back(current_field);
                current_field.clear();
                break;
            case '\r':
                break;
            case '\n':
                current_row.push_back(current_field);
                current_field.clear();
                rows.push_back(current_row);
                current_row.clear();
                break;
            default:
                current_field += c;
                break;
        }
    }

    // A file that does not end with a newline still has a pending row.
    if (!current_field.empty() || !current_row.empty()) {
        current_row.push_back(current_field);
        rows.push_back(current_row);
    }

    return rows;
}

// Empty cells mean "feature not computed" and are not an error.
bool try_parse_double(const std::string& cell, double& out) {
    if (cell.empty()) {
        return false;
    }
    try {
        std::size_t consumed = 0;
        out = std::stod(cell, &consumed);
        // stod("12abc") parses the prefix without throwing - require
        // the whole cell to be consumed.
        return consumed == cell.size();
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<FeatureVector> load_tracks_csv(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw CsvLoadError("Failed to open file: " + path);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Strip the UTF-8 BOM written by Python's utf-8-sig encoding;
    // otherwise it corrupts the first header field.
    if (content.size() >= 3 && content.substr(0, 3) == "\xEF\xBB\xBF") {
        content = content.substr(3);
    }

    std::vector<std::vector<std::string>> rows = parse_csv(content);

    if (rows.empty()) {
        throw CsvLoadError("File is empty: " + path);
    }

    std::vector<std::string> header = expected_header();
    if (rows[0] != header) {
        throw CsvLoadError(
            "CSV header does not match the expected format (was this file "
            "produced by get_playlist_features.py?): " + path);
    }

    const std::size_t kFirstFeatureColumn = 7;

    std::vector<FeatureVector> tracks;

    for (std::size_t row_index = 1; row_index < rows.size(); ++row_index) {
        const std::vector<std::string>& row = rows[row_index];

        if (row.size() == 1 && row[0].empty()) {
            continue;  // stray blank line
        }

        // Fail loudly instead of silently shifting columns.
        if (row.size() != header.size()) {
            throw CsvLoadError("Row " + std::to_string(row_index + 1) +
                                " in " + path + " has the wrong number of fields");
        }

        FeatureVector track;
        track.name = row[0];
        track.artists = row[1];
        track.id = row[6];

        bool all_features_present = true;
        for (std::size_t f = 0; f < kFeatureCount; ++f) {
            double value = 0.0;
            bool parsed = try_parse_double(row[kFirstFeatureColumn + f], value);
            if (parsed) {
                track.features[f] = value;
            } else {
                track.features[f] = 0.0;
                all_features_present = false;
            }
        }
        track.has_features = all_features_present;

        tracks.push_back(track);
    }

    return tracks;
}

}  // namespace music_taste
