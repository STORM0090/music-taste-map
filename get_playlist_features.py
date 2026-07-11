"""Fetch all tracks of a Spotify playlist and export them to CSV.

Usage:
  python get_playlist_features.py <playlist ID or URL>
  (or set PLAYLIST_ID in .env and run without arguments)
"""

import csv
import os
import sys

import spotipy
from dotenv import load_dotenv
from spotipy.oauth2 import SpotifyOAuth

load_dotenv()

CLIENT_ID = os.getenv("SPOTIFY_CLIENT_ID")
CLIENT_SECRET = os.getenv("SPOTIFY_CLIENT_SECRET")
REDIRECT_URI = os.getenv("SPOTIFY_REDIRECT_URI", "http://127.0.0.1:8888/callback")

if not CLIENT_ID or not CLIENT_SECRET:
    sys.exit("Error: set SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET in .env")

playlist = sys.argv[1] if len(sys.argv) > 1 else os.getenv("PLAYLIST_ID")
if not playlist:
    sys.exit(
        "Error: no playlist given.\n"
        "Either: python get_playlist_features.py <ID or URL>\n"
        "Or: set PLAYLIST_ID in .env"
    )

# Authorization Code flow - required for private playlists. The token
# is cached in .cache after the first login.
auth = SpotifyOAuth(
    client_id=CLIENT_ID,
    client_secret=CLIENT_SECRET,
    redirect_uri=REDIRECT_URI,
    scope="playlist-read-private playlist-read-collaborative",
)
sp = spotipy.Spotify(auth_manager=auth)

print("Fetching playlist tracks...")

tracks = []
page = sp.playlist_items(playlist)
while page:
    for item in page["items"]:
        # The track object lives under "track" (old API format) or
        # "item" (newer unified format).
        track = item.get("track") or item.get("item")
        # Removed tracks and local files have no Spotify ID.
        if track is None or track.get("id") is None:
            continue
        tracks.append(
            {
                "id": track["id"],
                "name": track["name"],
                "artists": ", ".join(a["name"] for a in track.get("artists", [])),
                "album": track.get("album", {}).get("name", ""),
                "release_date": track.get("album", {}).get("release_date", ""),
                # popularity is absent in the newer response format
                "popularity": track.get("popularity", ""),
                "duration_ms": track.get("duration_ms", ""),
            }
        )
    page = sp.next(page) if page["next"] else None

print(f"Tracks found: {len(tracks)}")
if not tracks:
    sys.exit("The playlist has no tracks - nothing to save.")

print("Fetching audio features...")

features_by_id = {}
try:
    # /audio-features accepts at most 100 IDs per request
    for start in range(0, len(tracks), 100):
        chunk_ids = [t["id"] for t in tracks[start : start + 100]]
        for f in sp.audio_features(chunk_ids):
            if f is not None:
                features_by_id[f["id"]] = f
except spotipy.SpotifyException as e:
    if e.http_status == 403:
        # Spotify closed /audio-features for apps created after
        # Nov 27, 2024 - nothing to be done client-side.
        print(
            "\nWARNING: Spotify returned 403 for audio features.\n"
            "Saving the CSV with track metadata only.\n"
        )
    else:
        raise

# loudness is intentionally absent: the offline feature datasets used
# to fill these columns do not provide it (see match_features.py).
FEATURE_COLUMNS = [
    "danceability",
    "energy",
    "valence",
    "tempo",
    "acousticness",
    "instrumentalness",
    "speechiness",
    "liveness",
    "key",
    "mode",
    "time_signature",
]

BASE_COLUMNS = ["name", "artists", "album", "release_date", "popularity", "duration_ms", "id"]

OUTPUT_FILE = "playlist_features.csv"

# utf-8-sig so Excel handles non-ASCII characters correctly
with open(OUTPUT_FILE, "w", newline="", encoding="utf-8-sig") as f:
    writer = csv.DictWriter(f, fieldnames=BASE_COLUMNS + FEATURE_COLUMNS)
    writer.writeheader()
    for t in tracks:
        row = dict(t)
        feats = features_by_id.get(t["id"], {})
        for col in FEATURE_COLUMNS:
            row[col] = feats.get(col, "")
        writer.writerow(row)

print(f"Done! Saved {len(tracks)} tracks to {OUTPUT_FILE}")
if features_by_id:
    print(f"Audio features fetched for {len(features_by_id)} tracks.")
