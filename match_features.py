"""Fill audio features of tracks in the database from an offline dataset.

Spotify no longer serves /audio-features for new apps, so the features
come from a Kaggle dataset instead. Only tracks that still miss
features are considered - re-runs and new playlists reuse everything
matched before. Two matching passes:
  1. exact Spotify track id - always correct;
  2. normalized "name + first artist" - catches re-releases of the
     same song under a different id (picks the most popular candidate).
Genres come along for the ride (used later for constellation naming).

Usage:
  python match_features.py [dataset_csv]   (default: data/tracks.csv)
"""

import csv
import re
import sys

import db

DATASET_CSV = sys.argv[1] if len(sys.argv) > 1 else "data/tracks.csv"

FEATURE_COLUMNS = db.FEATURE_COLUMNS


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


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    con = db.connect()

    missing = db.tracks_missing_features(con)
    print(f"Tracks without features: {len(missing)}")
    if not missing:
        print("Nothing to do.")
        return

    wanted_ids = {t["id"] for t in missing}
    wanted_keys = set()
    for t in missing:
        key = key_of(t["name"], t["artists"])
        if key is not None:
            wanted_keys.add(key)

    # One streaming pass over the big dataset: remember only rows we need.
    by_id = {}
    by_key = {}
    with open(DATASET_CSV, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row["track_id"] in wanted_ids:
                by_id[row["track_id"]] = row
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
    updates = []
    for t in missing:
        row = by_id.get(t["id"])
        if row is not None:
            matched_by_id += 1
        else:
            key = key_of(t["name"], t["artists"])
            row = by_key.get(key) if key is not None else None
            if row is not None:
                matched_by_name += 1
        if row is None:
            unmatched.append(t)
            continue
        update = {col: float(row[col]) for col in FEATURE_COLUMNS}
        update["id"] = t["id"]
        update["genres"] = row.get("genres") or None
        updates.append(update)

    db.set_features(con, updates)

    total = matched_by_id + matched_by_name
    print(f"Matched by track id:    {matched_by_id}")
    print(f"Matched by name+artist: {matched_by_name}")
    print(f"Still unmatched:        {len(unmatched)}")
    for t in unmatched[:5]:
        print(f'  e.g. "{t["name"]}" by {t["artists"]}')

    covered = con.execute(
        "SELECT COUNT(*) AS n FROM tracks WHERE danceability IS NOT NULL"
    ).fetchone()["n"]
    all_tracks = con.execute("SELECT COUNT(*) AS n FROM tracks").fetchone()["n"]
    print(f"Database coverage now: {covered}/{all_tracks} tracks")
    print("Re-export playlists with: python db.py export <playlist>")


if __name__ == "__main__":
    main()
