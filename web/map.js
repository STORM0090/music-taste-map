// Star map: reads map.json produced by the C++ core and draws every
// track as a star, colored by cluster. All the math (normalization,
// clustering, 2D projection) already happened in C++ - this file only
// scales ready-made coordinates to pixels and handles interaction.

const SVG_NS = "http://www.w3.org/2000/svg";

// Cluster colors, picked to stay readable on the dark sky.
const CLUSTER_COLORS = [
  "#ffd166", "#4cc9f0", "#f072b6", "#80ed99",
  "#c77dff", "#ff8a5c", "#9bf6ff", "#fdffb6",
];

const svg = document.getElementById("map");
const WIDTH = 900;
const HEIGHT = 600;
const PADDING = 40;

async function main() {
  const response = await fetch("map.json");
  if (!response.ok) {
    throw new Error("map.json not found - generate it with the C++ core");
  }
  const data = await response.json();

  const pctX = (data.axes.x.explained * 100).toFixed(0);
  const pctY = (data.axes.y.explained * 100).toFixed(0);
  document.getElementById("subtitle").textContent =
    `${data.mapped_count} of ${data.track_count} tracks on the map · ` +
    `X axis holds ${pctX}% of the variance, Y axis ${pctY}%`;

  // Data coordinates live in PCA units (roughly -4..+4); the SVG wants
  // pixels. Build two linear scales from the actual data ranges.
  const xs = data.tracks.map(t => t.x);
  const ys = data.tracks.map(t => t.y);
  const minX = Math.min(...xs), maxX = Math.max(...xs);
  const minY = Math.min(...ys), maxY = Math.max(...ys);

  const scaleX = x =>
    PADDING + ((x - minX) / (maxX - minX)) * (WIDTH - 2 * PADDING);
  // SVG's y axis grows downward; flip it so "more PC2" points up.
  const scaleY = y =>
    HEIGHT - PADDING - ((y - minY) / (maxY - minY)) * (HEIGHT - 2 * PADDING);

  drawAxisLabels(axisDescription(data.axes.x), axisDescription(data.axes.y));

  for (const track of data.tracks) {
    const star = document.createElementNS(SVG_NS, "circle");
    star.setAttribute("cx", scaleX(track.x));
    star.setAttribute("cy", scaleY(track.y));
    star.setAttribute("r", 3);
    star.setAttribute("fill",
        CLUSTER_COLORS[track.cluster % CLUSTER_COLORS.length]);
    star.setAttribute("fill-opacity", "0.85");
    star.classList.add("star");
    star.addEventListener("mouseenter", () => showTrack(track));
    svg.appendChild(star);
  }

  buildLegend(data.clusters);
}

// "energy / acousticness" - the two features that pull this axis the
// hardest, taken from the PCA loadings.
function axisDescription(axis) {
  const entries = Object.entries(axis.loadings);
  entries.sort((a, b) => Math.abs(b[1]) - Math.abs(a[1]));
  return `${entries[0][0]} / ${entries[1][0]}`;
}

function drawAxisLabels(xText, yText) {
  const make = (x, y, text, rotated) => {
    const label = document.createElementNS(SVG_NS, "text");
    label.setAttribute("x", x);
    label.setAttribute("y", y);
    label.setAttribute("text-anchor", "middle");
    if (rotated) {
      label.setAttribute("transform", `rotate(-90 ${x} ${y})`);
    }
    label.classList.add("axis-label");
    label.textContent = text;
    svg.appendChild(label);
  };
  make(WIDTH / 2, HEIGHT - 12, `← ${xText} →`, false);
  make(16, HEIGHT / 2, `← ${yText} →`, true);
}

function showTrack(track) {
  document.getElementById("panel-title").textContent = track.name;
  document.getElementById("panel-artists").textContent = track.artists;
  document.getElementById("panel-cluster").textContent =
    `Cluster ${track.cluster}`;
  document.getElementById("panel-axes").textContent =
    `map position: x ${track.x.toFixed(2)}, y ${track.y.toFixed(2)}`;
}

function buildLegend(clusters) {
  const legend = document.getElementById("legend");
  for (const cluster of clusters) {
    const row = document.createElement("div");
    row.className = "legend-row";
    const dot = document.createElement("span");
    dot.className = "legend-dot";
    dot.style.background =
        CLUSTER_COLORS[cluster.id % CLUSTER_COLORS.length];
    row.appendChild(dot);
    row.append(`Cluster ${cluster.id} — ${cluster.size} tracks`);
    legend.appendChild(row);
  }
}

main().catch(err => {
  document.getElementById("subtitle").textContent = err.message;
});
