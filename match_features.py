"""Fill audio features in playlist_features.csv from an offline dataset.

Spotify no longer serves /audio-features for new apps, so the features
come from a Kaggle dataset instead. Two matching passes:
  1. exact Spotify track id - always correct;
  2. normalized "name + first artist" - catches re-releases of the
     same song under a different id (picks the most popular candidate).
Tracks not found in either pass keep empty feature cells; they are
listed in data/unmatched.csv for a later audio-analysis fallback.

Genres for matched tracks (present in the dataset, useful for cluster
naming later) are saved separately to data/genres.csv.

Usage:
  python match_features.py [playlist_csv] [dataset_csv]
  (defaults: playlist_features.csv and data/tracks.csv)
"""

import csv
import re
import sys

PLAYLIST_CSV = sys.argv[1] if len(sys.argv) > 1 else "playlist_features.csv"
DATASET_CSV = sys.argv[2] if len(sys.argv) > 2 else "data/tracks.csv"

# Our schema column -> dataset column (names happen to be identical).
FEATURE_COLUMNS = [
    "danceability", "energy", "valence", "tempo", "acousticness",
    "instrumentalness", "speechiness", "liveness", "key", "mode",
    "time_signature",
]

BASE_COLUMNS = ["name", "artists", "album", "release_date", "popularity",
                "duration_ms", "id"]


def norm(s):
    """Collapse a title/artist to a crude comparison key."""
    s = s.lower()
    s = re.sub(r"\(.*?\)|\[.*?\]", " ", s)  # drop (feat. ...) / [remix]
    s = s.split(" - ")[0]                    # drop " - Remastered 2011" etc.
    s = re.sub(r"[^a-z0-9]+", " ", s)        # keep latin letters and digits
    return " ".join(s.split())


def key_of(name, artists):
    """Comparison key for the name+artist pass, or None if unusable.

    Non-latin titles (e.g. Cyrillic) normalize to an empty string; if
    they took part in matching, every such track would collide on the
    ("", "") key and receive the features of one random dataset row.
    Those tracks are id-match only.
    """
    n = norm(name)
    a = norm(artists.split(",")[0])
    if not n or not a:
        return None
    return (n, a)


def popularity_of(row):
    try:
        return float(row.get("popularity") or 0)
    except ValueError:
        return 0.0


with open(PLAYLIST_CSV, encoding="utf-8-sig") as f:
    playlist = list(csv.DictReader(f))
print(f"Playlist tracks: {len(playlist)}")

wanted_ids = {t["id"] for t in playlist}
wanted_keys = set()
for t in playlist:
    key = key_of(t["name"], t["artists"])
    if key is not None:
        wanted_keys.add(key)

# One streaming pass over the big dataset: remember only rows we need.
by_id = {}
by_key = {}
with open(DATASET_CSV, encoding="utf-8") as f:
    for row in csv.DictReader(f):
        tid = row["track_id"]
        if tid in wanted_ids:
            by_id[tid] = row
        key = key_of(row["name"], row["track_artists"])
        if key is not None and key in wanted_keys:
            # Several dataset rows can share one key (re-releases);
            # keep the most popular version.
            kept = by_key.get(key)
            if kept is None or popularity_of(row) > popularity_of(kept):
                by_key[key] = row

matched_by_id = 0
matched_by_name = 0
unmatched = []
genres = {}

for t in playlist:
    row = by_id.get(t["id"])
    if row is not None:
        matched_by_id += 1
    else:
        key = key_of(t["name"], t["artists"])
        row = by_key.get(key) if key is not None else None
        if row is not None:
            matched_by_name += 1
    if row is None:
        # Clear the feature cells so re-runs never leave stale values
        # from a previous (possibly buggy) pass.
        for col in FEATURE_COLUMNS:
            t[col] = ""
        unmatched.append(t)
        continue
    for col in FEATURE_COLUMNS:
        t[col] = row[col]
    if not t.get("popularity"):
        t["popularity"] = row.get("popularity", "")
    if row.get("genres"):
        genres[t["id"]] = row["genres"]

with open(PLAYLIST_CSV, "w", newline="", encoding="utf-8-sig") as f:
    writer = csv.DictWriter(f, fieldnames=BASE_COLUMNS + FEATURE_COLUMNS,
                            extrasaction="ignore")
    writer.writeheader()
    writer.writerows(playlist)

with open("data/genres.csv", "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(["id", "genres"])
    for tid, g in genres.items():
        writer.writerow([tid, g])

with open("data/unmatched.csv", "w", newline="", encoding="utf-8") as f:
    writer = csv.DictWriter(f, fieldnames=["id", "name", "artists"],
                            extrasaction="ignore")
    writer.writeheader()
    writer.writerows(unmatched)

total = matched_by_id + matched_by_name
print(f"Matched by track id:    {matched_by_id}")
print(f"Matched by name+artist: {matched_by_name}")
print(f"Unmatched:              {len(unmatched)} (data/unmatched.csv)")
print(f"Coverage: {total}/{len(playlist)} = {100 * total / len(playlist):.1f}%")
print(f"Updated {PLAYLIST_CSV}; genres saved to data/genres.csv")
