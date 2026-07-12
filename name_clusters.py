"""Name the star-map constellations with an LLM.

Reads web/map.json (produced by the C++ core) and data/genres.csv
(produced by match_features.py), builds a compact profile of every
cluster - size, distinctive audio features, top genres, typical
tracks - and asks Gemini for a creative name and a one-line
description per cluster, plus one overall paragraph about the whole
library. The math stays on our side; the model only puts words on
structure that is already computed.

Gemini is used for its free tier - the task is a single tiny request,
well within free quota. The provider is an implementation detail:
everything except the ~20-line API call is provider-agnostic.

Writes web/names.json. The frontend picks it up when present and
falls back to "Cluster N" labels when not.

Usage:
  python name_clusters.py            # calls the API (needs GEMINI_API_KEY in .env)
  python name_clusters.py --dry-run  # print the prompt and exit, no API call
"""

import ast
import json
import os
import sys

from dotenv import load_dotenv

# Windows consoles often default to a legacy code page that cannot
# print non-Latin track names; force UTF-8 output.
sys.stdout.reconfigure(encoding="utf-8")

MAP_PATH = "web/map.json"
OUT_PATH = "web/names.json"

# The response must follow this exact shape - structured output mode
# makes the API guarantee valid JSON matching this schema.
RESPONSE_SCHEMA = {
    "type": "object",
    "properties": {
        "overall": {
            "type": "string",
            "description": "2-3 sentences characterizing the whole library",
        },
        "clusters": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "id": {"type": "integer"},
                    "name": {
                        "type": "string",
                        "description": "Evocative constellation name, 1-3 words",
                    },
                    "description": {
                        "type": "string",
                        "description": "One sentence about the cluster's character",
                    },
                },
                "required": ["id", "name", "description"],
            },
        },
    },
    "required": ["overall", "clusters"],
}


def load_genres():
    """id -> list of genre strings, from the database.

    The dataset stores genres as a Python-list-looking string
    ("['pop', 'rock']"); ast.literal_eval parses exactly that, safely.
    Genres are a nice-to-have: any failure just means less context in
    the prompt.
    """
    genres = {}
    try:
        import db
        con = db.connect()
        for row in con.execute(
                "SELECT id, genres FROM tracks WHERE genres IS NOT NULL"):
            try:
                parsed = ast.literal_eval(row["genres"])
            except (ValueError, SyntaxError):
                parsed = [row["genres"]]
            if isinstance(parsed, list):
                genres[row["id"]] = [str(g) for g in parsed]
    except Exception:
        pass
    return genres


def cluster_profile(cluster, tracks, genres_by_id):
    """One text block describing a cluster for the prompt."""
    members = [t for t in tracks if t["cluster"] == cluster["id"]]

    # The most distinctive features: largest |z| in the centroid.
    features = sorted(cluster["centroid"].items(),
                      key=lambda kv: abs(kv[1]), reverse=True)[:3]
    feature_text = ", ".join(f"{name} {value:+.2f}" for name, value in features)

    # Genre counts across members.
    counts = {}
    for t in members:
        for g in genres_by_id.get(t["id"], []):
            counts[g] = counts.get(g, 0) + 1
    top_genres = sorted(counts.items(), key=lambda kv: kv[1], reverse=True)[:5]
    genre_text = (", ".join(f"{g} ({n})" for g, n in top_genres)
                  if top_genres else "no genre data")

    # Typical tracks: nearest to the cluster's center on the 2D map.
    cx = sum(t["x"] for t in members) / len(members)
    cy = sum(t["y"] for t in members) / len(members)
    members.sort(key=lambda t: (t["x"] - cx) ** 2 + (t["y"] - cy) ** 2)
    samples = ", ".join(f'"{t["name"]}" by {t["artists"]}' for t in members[:5])

    return (f'Cluster {cluster["id"]}: {len(members)} tracks\n'
            f"  Distinctive audio profile (z-scores vs library average): "
            f"{feature_text}\n"
            f"  Top genres: {genre_text}\n"
            f"  Typical tracks: {samples}")


def build_prompt(data, genres_by_id):
    profiles = "\n\n".join(cluster_profile(c, data["tracks"], genres_by_id)
                           for c in data["clusters"] if c["size"] > 0)
    return f"""I clustered my music library ({data["mapped_count"]} tracks) by audio features \
(energy, valence, tempo, danceability, acousticness, ...). The map is styled \
as a night sky where every cluster is a constellation.

Audio features are z-scores: 0 means "average for this library", +1/-1 means \
one standard deviation above/below it.

{profiles}

Name each constellation and give it a one-sentence description. Names should \
be evocative and atmospheric (like "Rainy Evening" for low energy + high \
acousticness), 1-3 words, in English, all distinct from each other. Ground \
every name in the cluster's actual profile - features, genres and tracks - \
not in generic space imagery. Also write 2-3 sentences ("overall") \
characterizing the taste of this library as a whole."""


def main():
    load_dotenv()  # ANTHROPIC_API_KEY lives in .env, next to the Spotify keys

    with open(MAP_PATH, encoding="utf-8") as f:
        data = json.load(f)
    genres_by_id = load_genres()
    prompt = build_prompt(data, genres_by_id)

    if "--dry-run" in sys.argv:
        print(prompt)
        return

    api_key = os.getenv("GEMINI_API_KEY")
    if not api_key:
        sys.exit("Error: set GEMINI_API_KEY in .env "
                 "(free key, no card needed: aistudio.google.com/apikey)")

    from google import genai
    from google.genai import errors, types

    client = genai.Client(api_key=api_key)
    try:
        # The "-latest" alias tracks the current flash model, so the
        # script survives Google's model version rotations.
        response = client.models.generate_content(
            model="gemini-flash-latest",
            contents=prompt,
            config=types.GenerateContentConfig(
                response_mime_type="application/json",
                response_schema=RESPONSE_SCHEMA,
            ),
        )
    except errors.APIError as e:
        sys.exit(f"Error: Gemini API request failed ({e.code}): {e.message}")

    # Structured output mode guarantees response.text is valid JSON.
    names = json.loads(response.text)

    with open(OUT_PATH, "w", encoding="utf-8") as f:
        json.dump(names, f, ensure_ascii=False, indent=2)

    print(f"Wrote {OUT_PATH}")
    for cluster in names["clusters"]:
        print(f'  {cluster["id"]}: {cluster["name"]} - {cluster["description"]}')


if __name__ == "__main__":
    main()
