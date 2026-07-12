"""Fetch all tracks of a Spotify playlist into the local database.

Track metadata goes into SQLite (music.db); audio features are filled
separately by match_features.py and survive re-fetches, so tracks the
database already knows arrive with their features "for free". After
saving, the playlist is exported to playlist_features.csv for the C++
core.

Usage:
  python get_playlist_features.py <playlist ID or URL>
  (or set PLAYLIST_ID in .env and run without arguments)
"""

import os
import sys

import spotipy
from dotenv import load_dotenv
from spotipy.oauth2 import SpotifyOAuth

import db

# Windows consoles often default to a legacy code page that cannot
# print non-Latin playlist or track names; force UTF-8 output.
sys.stdout.reconfigure(encoding="utf-8")

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

# Canonical id and display name of the playlist itself.
meta = sp.playlist(playlist, fields="id,name")

print(f'Fetching "{meta["name"]}"...')

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
                "popularity": track.get("popularity") or None,
                "duration_ms": track.get("duration_ms") or None,
            }
        )
    page = sp.next(page) if page["next"] else None

print(f"Tracks found: {len(tracks)}")
if not tracks:
    sys.exit("The playlist has no tracks - nothing to save.")

con = db.connect()
db.upsert_tracks(con, tracks)
db.save_playlist(con, meta["id"], meta["name"], [t["id"] for t in tracks])

with_features = con.execute(
    """
    SELECT COUNT(*) AS n
    FROM tracks t
    JOIN playlist_tracks pt ON pt.track_id = t.id
    JOIN playlists p ON p.id = pt.playlist_id
    WHERE p.spotify_id = ? AND t.danceability IS NOT NULL
    """,
    (meta["id"],),
).fetchone()["n"]

exported = db.export_playlist_csv(con, meta["id"], "playlist_features.csv")
print(f"Saved to music.db; exported {exported} tracks to playlist_features.csv")
print(f"{with_features} already have features; run match_features.py "
      f"to fill in the rest")
