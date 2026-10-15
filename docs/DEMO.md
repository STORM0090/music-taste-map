# How the live demo was built

The demo at https://storm0090.github.io/music-taste-map/ contains no personal
playlist. It is a seeded random sample of a public dataset, run through the
same pipeline as real playlists (minus the Spotify fetch).

## Data

- Source: "Almost Million Songs Dataset 2025 (16 Features)" on Kaggle,
  https://www.kaggle.com/datasets/anantsinghal786/almost-million-songs-dataset-2025-16-features
  (`tracks.csv`, 899,702 rows). Check the dataset page for its license before
  reusing the sample.
- Filters: title and artist non-empty and ASCII, all 11 audio features
  numeric, tempo above 0, popularity 60 or higher, unique track id and unique
  title and artist pair. 3,428 candidates remain.
- Sampling: shuffle with `random.Random(42)`, then take the first 250
  candidates whose title and artist pass a short word blocklist (explicit or
  sensitive words, so the demo is safe to show at work). 9 candidates were
  skipped while filling the sample.
- Audio features come straight from the dataset, so no Spotify API call is
  involved.

## Steps

From the repository root, with `data/tracks.csv` in place and the C++ core
built (the path of the binary depends on your CMake generator):

```sh
python -I docs/make_demo.py data/tracks.csv . demo_out
mkdir demo_out/web
cd demo_out
../cpp_core/build/music_taste_core demo_tracks.csv 5 web/map.json
python ../name_clusters.py        # needs GEMINI_API_KEY in ../.env
cp web/map.json web/names.json ../docs/
```

`name_clusters.py` reads `web/map.json` and `music.db` relative to the working
directory, which is why it runs inside `demo_out`. The names and the summary
paragraph in `docs/names.json` come from one Gemini run (the
`gemini-flash-latest` alias) on 2026-10-15. They differ between runs.

## What the files contain

`docs/map.json` has track id, title, artists, 2D coordinates and cluster id for
250 tracks, plus cluster sizes, centroids and the two PCA axes. The 5 clusters
hold 37, 18, 39, 95 and 61 tracks, and the two axes explain 24.1% and 13.2% of
the variance. Regenerating the CSV and `map.json` on the same machine gave
byte-identical files.
