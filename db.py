"""SQLite storage for the pipeline.

One tracks row per unique track (metadata + audio features + genres),
one playlists row per fetched playlist, and playlist_tracks linking
the two - so many playlists can share tracks without duplicating
them, and features matched once are reused forever.

The C++ core keeps reading CSV: `export` writes the same file format
get_playlist_features.py used to produce.

Usage:
  python db.py stats
  python db.py export <playlist id or URL> [output.csv]
"""

import csv
import sqlite3
import sys
from datetime import datetime, timezone

DB_PATH = "music.db"

# Must stay in the same order as the C++ core's kFeatureNames.
FEATURE_COLUMNS = [
    "danceability", "energy", "valence", "tempo", "acousticness",
    "instrumentalness", "speechiness", "liveness", "key", "mode",
    "time_signature",
]

BASE_COLUMNS = ["name", "artists", "album", "release_date", "popularity",
                "duration_ms", "id"]

# NULL in a feature column means "not matched yet" - the SQL
# equivalent of has_features == false on the C++ side.
SCHEMA = """
CREATE TABLE IF NOT EXISTS tracks (
    id             TEXT PRIMARY KEY,   -- Spotify track id
    name           TEXT NOT NULL,
    artists        TEXT NOT NULL,
    album          TEXT,
    release_date   TEXT,
    popularity     INTEGER,
    duration_ms    INTEGER,
    danceability   REAL,
    energy         REAL,
    valence        REAL,
    tempo          REAL,
    acousticness   REAL,
    instrumentalness REAL,
    speechiness    REAL,
    liveness       REAL,
    key            REAL,
    mode           REAL,
    time_signature REAL,
    genres         TEXT                -- as stored in the dataset: "['pop', ...]"
);

CREATE TABLE IF NOT EXISTS playlists (
    id         INTEGER PRIMARY KEY,
    spotify_id TEXT NOT NULL UNIQUE,
    name       TEXT,
    fetched_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS playlist_tracks (
    playlist_id INTEGER NOT NULL REFERENCES playlists(id),
    track_id    TEXT    NOT NULL REFERENCES tracks(id),
    position    INTEGER NOT NULL,
    PRIMARY KEY (playlist_id, track_id)
);
"""


def connect(path=DB_PATH):
    con = sqlite3.connect(path)
    con.row_factory = sqlite3.Row  # rows behave like dicts: row["name"]
    con.execute("PRAGMA foreign_keys = ON")
    con.executescript(SCHEMA)  # idempotent thanks to IF NOT EXISTS
    return con


def canonical_playlist_id(raw):
    """Accept a bare id, an open.spotify.com URL or a spotify: URI."""
    if "/playlist/" in raw:
        raw = raw.split("/playlist/")[1]
    if raw.startswith("spotify:playlist:"):
        raw = raw.split(":")[-1]
    return raw.split("?")[0]


def upsert_tracks(con, tracks):
    """Insert new tracks or refresh metadata of known ones.

    Feature columns are deliberately untouched: a re-fetch must not
    wipe features that were already matched.
    """
    con.executemany(
        """
        INSERT INTO tracks (id, name, artists, album, release_date,
                            popularity, duration_ms)
        VALUES (:id, :name, :artists, :album, :release_date,
                :popularity, :duration_ms)
        ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            artists = excluded.artists,
            album = excluded.album,
            release_date = excluded.release_date,
            popularity = excluded.popularity,
            duration_ms = excluded.duration_ms
        """,
        tracks,
    )


def save_playlist(con, spotify_id, name, track_ids):
    """Record a playlist snapshot: metadata + ordered track links."""
    row = con.execute(
        """
        INSERT INTO playlists (spotify_id, name, fetched_at)
        VALUES (?, ?, ?)
        ON CONFLICT(spotify_id) DO UPDATE SET
            name = excluded.name,
            fetched_at = excluded.fetched_at
        RETURNING id
        """,
        (spotify_id, name, datetime.now(timezone.utc).isoformat()),
    ).fetchone()
    playlist_id = row["id"]

    # Replace the previous snapshot of this playlist's contents.
    con.execute("DELETE FROM playlist_tracks WHERE playlist_id = ?",
                (playlist_id,))
    con.executemany(
        "INSERT INTO playlist_tracks (playlist_id, track_id, position) "
        "VALUES (?, ?, ?)",
        [(playlist_id, tid, pos) for pos, tid in enumerate(track_ids)],
    )
    con.commit()


def tracks_missing_features(con):
    return con.execute(
        "SELECT id, name, artists FROM tracks WHERE danceability IS NULL"
    ).fetchall()


def set_features(con, rows):
    """rows: dicts with id, the 11 feature values and genres."""
    con.executemany(
        """
        UPDATE tracks SET
            danceability = :danceability, energy = :energy,
            valence = :valence, tempo = :tempo,
            acousticness = :acousticness,
            instrumentalness = :instrumentalness,
            speechiness = :speechiness, liveness = :liveness,
            key = :key, mode = :mode, time_signature = :time_signature,
            genres = :genres
        WHERE id = :id
        """,
        rows,
    )
    con.commit()


def export_playlist_csv(con, spotify_id, out_path):
    """DB -> the CSV format the C++ core expects. Returns row count."""
    rows = con.execute(
        """
        SELECT t.*
        FROM tracks t
        JOIN playlist_tracks pt ON pt.track_id = t.id
        JOIN playlists p ON p.id = pt.playlist_id
        WHERE p.spotify_id = ?
        ORDER BY pt.position
        """,
        (spotify_id,),
    ).fetchall()

    with open(out_path, "w", newline="", encoding="utf-8-sig") as f:
        writer = csv.writer(f)
        writer.writerow(BASE_COLUMNS + FEATURE_COLUMNS)
        for r in rows:
            # NULL becomes an empty cell - exactly how the C++ loader
            # recognizes "features not computed".
            cell = lambda v: "" if v is None else v
            writer.writerow([cell(r[c]) for c in BASE_COLUMNS] +
                            [cell(r[c]) for c in FEATURE_COLUMNS])
    return len(rows)


def print_stats(con):
    total = con.execute("SELECT COUNT(*) AS n FROM tracks").fetchone()["n"]
    matched = con.execute(
        "SELECT COUNT(*) AS n FROM tracks WHERE danceability IS NOT NULL"
    ).fetchone()["n"]
    print(f"Tracks: {total} total, {matched} with features "
          f"({100 * matched / total:.1f}% coverage)" if total else "Tracks: 0")

    print("Playlists:")
    for row in con.execute(
        """
        SELECT p.name, p.spotify_id, p.fetched_at, COUNT(pt.track_id) AS n
        FROM playlists p
        LEFT JOIN playlist_tracks pt ON pt.playlist_id = p.id
        GROUP BY p.id
        ORDER BY p.name
        """
    ):
        print(f'  {row["name"]} ({row["spotify_id"]}): {row["n"]} tracks, '
              f'fetched {row["fetched_at"][:16]}')

    print("Top artists:")
    for row in con.execute(
        """
        SELECT artists, COUNT(*) AS n
        FROM tracks
        GROUP BY artists
        ORDER BY n DESC
        LIMIT 5
        """
    ):
        print(f'  {row["artists"]}: {row["n"]} tracks')


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    args = sys.argv[1:]
    if not args or args[0] not in ("stats", "export"):
        sys.exit("Usage:\n"
                 "  python db.py stats\n"
                 "  python db.py export <playlist id or URL> [output.csv]")

    con = connect()
    if args[0] == "stats":
        print_stats(con)
    else:
        if len(args) < 2:
            sys.exit("export needs a playlist id or URL")
        out = args[2] if len(args) > 2 else "playlist_features.csv"
        n = export_playlist_csv(con, canonical_playlist_id(args[1]), out)
        print(f"Exported {n} tracks to {out}")


if __name__ == "__main__":
    main()
