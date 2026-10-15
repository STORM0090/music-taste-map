"""Build the demo input: a seeded random sample of the public dataset.

Uses the project's own db.py (schema, upsert, export) so the CSV has the
exact format the C++ core reads, and the sample DB carries genres for the
naming step. Output goes to an isolated directory, never the real music.db.

Usage, from the repository root (see docs/DEMO.md for the full recipe):
  python -I docs/make_demo.py data/tracks.csv . demo_out
"""
import csv
import os
import random
import re
import sys

sys.dont_write_bytecode = True

dataset, project_dir, out_dir = sys.argv[1:4]
sys.path.insert(0, project_dir)
import db  # noqa: E402  (project module, stdlib-only)

MIN_POPULARITY = 60
SAMPLE_SIZE = 250
SEED = 42

FEATURES = db.FEATURE_COLUMNS

# Tracks whose title or artist contains these substrings, or these whole
# words, are dropped so the demo is safe to show in a work setting.
TITLE_SUBSTRINGS = ["sex", "shit", "fuck", "bitch", "nigg", "pussy", "culo",
                    "suicid", "genocide", "daddy issues", "rape", "whore",
                    "slut", "porn", "cocaine", "murder", "kill", "deepthroat",
                    "orgasm", "naked", "nude", "boob", "vagina", "penis"]
TITLE_WORDS = {"ass", "azz", "hoe", "hoes", "rip", "dick", "cock", "tits", "cum"}


def blocked(title):
    # Undo common letter substitutions, e.g. "$uicide" or "b*tch".
    lowered = title.lower().translate(str.maketrans({"$": "s", "@": "a", "*": ""}))
    words = set(re.findall(r"[a-z0-9]+", lowered))
    return (any(s in lowered for s in TITLE_SUBSTRINGS)
            or bool(words & TITLE_WORDS))


def usable(row):
    name = (row.get("name") or "").strip()
    artists = (row.get("track_artists") or "").strip()
    if not name or not artists:
        return None
    if not (name.isascii() and name.isprintable()
            and artists.isascii() and artists.isprintable()):
        return None
    try:
        values = {c: float(row[c]) for c in FEATURES}
        popularity = float(row["popularity"])
    except (TypeError, ValueError, KeyError):
        return None
    if values["tempo"] <= 0 or popularity < MIN_POPULARITY:
        return None
    return name, artists, values, int(popularity)


candidates = []
seen_ids = set()
seen_titles = set()
with open(dataset, encoding="utf-8", newline="") as f:
    for row in csv.DictReader(f):
        parsed = usable(row)
        if parsed is None:
            continue
        name, artists, values, popularity = parsed
        title_key = (name.lower(), artists.lower())
        if row["track_id"] in seen_ids or title_key in seen_titles:
            continue
        seen_ids.add(row["track_id"])
        seen_titles.add(title_key)
        candidates.append((row, name, artists, values, popularity))

print(f"candidates after filters and de-duplication: {len(candidates)}")

# Shuffle once with a fixed seed, then take the first SAMPLE_SIZE candidates
# whose title passes the blocklist. Extending the blocklist later therefore
# only swaps individual tracks instead of reshuffling the whole sample.
random.Random(SEED).shuffle(candidates)
sample = []
skipped = []
for candidate in candidates:
    if blocked(candidate[1]) or blocked(candidate[2]):
        skipped.append(candidate[1] + " / " + candidate[2])
        continue
    sample.append(candidate)
    if len(sample) == SAMPLE_SIZE:
        break
print(f"skipped by title blocklist while filling the sample: {len(skipped)}")
for title in skipped:
    print("  skipped:", title)

os.makedirs(out_dir, exist_ok=True)
db_path = os.path.join(out_dir, "music.db")
if os.path.exists(db_path):
    os.remove(db_path)
con = db.connect(db_path)

db.upsert_tracks(con, [
    {"id": row["track_id"], "name": name, "artists": artists, "album": "",
     "release_date": "", "popularity": popularity, "duration_ms": None}
    for row, name, artists, values, popularity in sample
])
db.save_playlist(con, "demo-sample", "Demo sample",
                 [row["track_id"] for row, *_ in sample])
db.set_features(con, [
    {**values, "id": row["track_id"], "genres": row.get("genres") or None}
    for row, name, artists, values, popularity in sample
])

csv_path = os.path.join(out_dir, "demo_tracks.csv")
exported = db.export_playlist_csv(con, "demo-sample", csv_path)
con.close()
print(f"exported {exported} tracks to {csv_path}")
