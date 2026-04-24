const fs = require("fs");
const path = require("path");

const OUT_JSON = path.join(__dirname, "rig_path_actions_demo.json");
const OUT_SVG = path.join(__dirname, "rig_path_actions_demo_preview.svg");

const FRAME_TIMES = [0, 20, 40, 60, 80, 100];
const MARKERS = [
  { tm: 0, cm: "active_idle|活动待机", dr: 20 },
  { tm: 20, cm: "active_open|活动展开", dr: 20 },
  { tm: 40, cm: "active_step|活动摆动", dr: 20 },
  { tm: 60, cm: "offline_alert|断网警觉", dr: 20 },
  { tm: 80, cm: "offline_drop|断网下垂", dr: 20 },
  { tm: 100, cm: "offline_hold|断网静止", dr: 20 },
];

function round(n) {
  return Math.round(n * 10) / 10;
}

function zeroTangents(points) {
  return points.map(() => [0, 0]);
}

function clonePoints(points) {
  return points.map(([x, y]) => [x, y]);
}

function ellipsePoints(cx, cy, rx, ry, count = 12) {
  const out = [];
  for (let i = 0; i < count; i++) {
    const t = (Math.PI * 2 * i) / count - Math.PI / 2;
    out.push([round(cx + Math.cos(t) * rx), round(cy + Math.sin(t) * ry)]);
  }
  return out;
}

function transformPoints(points, transform = {}) {
  const {
    tx = 0,
    ty = 0,
    rot = 0,
    sx = 1,
    sy = 1,
    cx = 0,
    cy = 0,
  } = transform;
  const rad = (rot * Math.PI) / 180;
  const cr = Math.cos(rad);
  const sr = Math.sin(rad);
  return points.map(([x0, y0]) => {
    const x1 = (x0 - cx) * sx;
    const y1 = (y0 - cy) * sy;
    const xr = x1 * cr - y1 * sr;
    const yr = x1 * sr + y1 * cr;
    return [round(xr + cx + tx), round(yr + cy + ty)];
  });
}

function toShape(points, closed) {
  const v = clonePoints(points);
  return {
    c: !!closed,
    v,
    i: zeroTangents(v),
    o: zeroTangents(v),
  };
}

function svgPath(points, closed) {
  if (!points.length) return "";
  const cmd = [`M ${round(points[0][0])},${round(points[0][1])}`];
  for (let i = 1; i < points.length; i++) {
    cmd.push(`L ${round(points[i][0])},${round(points[i][1])}`);
  }
  if (closed) cmd.push("Z");
  return cmd.join(" ");
}

const PARTS = [
  {
    name: "shell_left",
    kind: "BEZIER_LOOP",
    stroke: 4,
    color: "#22d3ee",
    closed: true,
    points: [
      [125.3, 207.5], [107.3, 211.9], [95.6, 212.5], [96.7, 192.4], [121.0, 166.7],
      [153.6, 190.3], [148.2, 235.0], [117.8, 247.1], [100.7, 232.3], [102.5, 223.7],
    ],
    poses: [
      {},
      { tx: -4, ty: -2, rot: -8, cx: 125, cy: 206 },
      { tx: -8, ty: 3, rot: -14, cx: 123, cy: 208 },
      { tx: -2, ty: 0, rot: -5, cx: 125, cy: 206 },
      { tx: 4, ty: 8, rot: 4, sx: 0.97, sy: 0.98, cx: 126, cy: 208 },
      { tx: 8, ty: 12, rot: 10, sx: 0.94, sy: 0.95, cx: 128, cy: 210 },
    ],
  },
  {
    name: "abdomen",
    kind: "BEZIER_LOOP",
    stroke: 4,
    color: "#84cc16",
    closed: true,
    points: [
      [183.4, 264.0], [165.8, 270.9], [154.3, 273.1], [153.8, 253.0], [175.9, 224.0],
      [210.2, 243.0], [208.3, 288.2], [179.0, 304.4], [160.9, 292.1], [161.9, 283.3],
    ],
    poses: [
      {},
      { tx: -2, ty: -4, rot: -5, cx: 181, cy: 267 },
      { tx: 5, ty: 2, rot: 8, cx: 180, cy: 267 },
      { tx: 1, ty: 1, rot: 2, cx: 181, cy: 267 },
      { tx: 8, ty: 10, rot: 11, sx: 0.97, sy: 0.95, cx: 182, cy: 268 },
      { tx: 12, ty: 14, rot: 16, sx: 0.94, sy: 0.92, cx: 184, cy: 269 },
    ],
  },
  {
    name: "head",
    kind: "BEZIER_LOOP",
    stroke: 4,
    color: "#ff6b6b",
    closed: true,
    points: ellipsePoints(191.6, 197.8, 48.3, 64.3, 12),
    poses: [
      {},
      { ty: -4, rot: -2, cx: 191.6, cy: 197.8 },
      { tx: 2, ty: -1, rot: 2, cx: 191.6, cy: 197.8 },
      { tx: 1, ty: 1, rot: 1, cx: 191.6, cy: 197.8 },
      { tx: 5, ty: 10, rot: 6, sx: 0.98, sy: 0.95, cx: 191.6, cy: 197.8 },
      { tx: 8, ty: 15, rot: 9, sx: 0.96, sy: 0.92, cx: 191.6, cy: 197.8 },
    ],
  },
  {
    name: "wing_outer",
    kind: "BEZIER_LOOP",
    stroke: 4,
    color: "#ff595e",
    closed: true,
    points: [
      [252.8, 109.4], [252.8, 104.8], [261.9, 85.2], [269.4, 82.7], [289.9, 115.8],
      [301.0, 145.5], [294.9, 157.0], [280.0, 157.5], [275.9, 154.9], [266.7, 131.6],
    ],
    poses: [
      {},
      { tx: 1, ty: -8, rot: 12, cx: 252.8, cy: 109.4 },
      { tx: 4, ty: 2, rot: -8, cx: 252.8, cy: 109.4 },
      { tx: -2, ty: -2, rot: 4, cx: 252.8, cy: 109.4 },
      { tx: -6, ty: 10, rot: 24, sx: 0.96, sy: 0.96, cx: 252.8, cy: 109.4 },
      { tx: -8, ty: 14, rot: 30, sx: 0.94, sy: 0.94, cx: 252.8, cy: 109.4 },
    ],
  },
  {
    name: "wing_mid",
    kind: "BEZIER_LOOP",
    stroke: 4,
    color: "#3b82f6",
    closed: true,
    points: [
      [235.9, 124.9], [236.8, 122.8], [248.1, 116.3], [257.9, 123.9], [271.3, 150.4],
      [271.1, 159.3], [262.4, 169.3], [259.0, 169.2], [248.8, 145.0],
    ],
    poses: [
      {},
      { ty: -5, rot: 9, cx: 236.0, cy: 125.0 },
      { tx: 3, ty: 3, rot: -6, cx: 236.0, cy: 125.0 },
      { tx: -1, ty: -1, rot: 4, cx: 236.0, cy: 125.0 },
      { tx: -4, ty: 8, rot: 16, sx: 0.97, sy: 0.97, cx: 236.0, cy: 125.0 },
      { tx: -6, ty: 11, rot: 20, sx: 0.95, sy: 0.95, cx: 236.0, cy: 125.0 },
    ],
  },
  {
    name: "wing_inner",
    kind: "BEZIER_LOOP",
    stroke: 4,
    color: "#a855f7",
    closed: true,
    points: [
      [216.6, 137.8], [229.0, 128.9], [235.5, 129.5], [246.8, 146.8], [256.8, 171.7],
      [253.7, 178.5], [243.2, 192.9], [242.1, 190.2], [234.8, 161.7], [216.2, 139.5],
    ],
    poses: [
      {},
      { tx: -2, ty: -3, rot: 7, cx: 216.5, cy: 138.0 },
      { tx: 2, ty: 4, rot: -5, cx: 216.5, cy: 138.0 },
      { tx: 0, ty: -1, rot: 3, cx: 216.5, cy: 138.0 },
      { tx: -5, ty: 6, rot: 14, sx: 0.98, sy: 0.96, cx: 216.5, cy: 138.0 },
      { tx: -7, ty: 10, rot: 18, sx: 0.96, sy: 0.95, cx: 216.5, cy: 138.0 },
    ],
  },
  {
    name: "antenna_right",
    kind: "BEZIER_STRIP",
    stroke: 3,
    color: "#ec4899",
    closed: false,
    points: [[201.5, 101.4], [191.8, 114.6], [188.5, 142.3]],
    poses: [
      {},
      { tx: 0, ty: -8, rot: -10, cx: 188.5, cy: 142.3 },
      { tx: 3, ty: -4, rot: 8, cx: 188.5, cy: 142.3 },
      { tx: 1, ty: -2, rot: -4, cx: 188.5, cy: 142.3 },
      { tx: 4, ty: 6, rot: 18, sx: 1.0, sy: 0.95, cx: 188.5, cy: 142.3 },
      { tx: 7, ty: 10, rot: 28, sx: 0.98, sy: 0.92, cx: 188.5, cy: 142.3 },
    ],
  },
  {
    name: "antenna_left",
    kind: "BEZIER_STRIP",
    stroke: 3,
    color: "#34d399",
    closed: false,
    points: [[172.1, 101.4], [162.4, 114.6], [159.1, 142.3]],
    poses: [
      {},
      { tx: -1, ty: -8, rot: 10, cx: 159.1, cy: 142.3 },
      { tx: -4, ty: -4, rot: -8, cx: 159.1, cy: 142.3 },
      { tx: -1, ty: -2, rot: 4, cx: 159.1, cy: 142.3 },
      { tx: -6, ty: 6, rot: -18, sx: 1.0, sy: 0.95, cx: 159.1, cy: 142.3 },
      { tx: -9, ty: 10, rot: -28, sx: 0.98, sy: 0.92, cx: 159.1, cy: 142.3 },
    ],
  },
  {
    name: "mouth_right",
    kind: "BEZIER_LOOP",
    stroke: 3,
    color: "#fb923c",
    closed: true,
    points: [[181.6, 142.3], [187.7, 137.7], [195.3, 142.3]],
    poses: [
      {},
      { ty: -1, rot: -4, cx: 188.0, cy: 141.0 },
      { ty: 1, rot: 5, cx: 188.0, cy: 141.0 },
      { ty: 1, rot: -2, cx: 188.0, cy: 141.0 },
      { tx: 2, ty: 5, rot: 12, cx: 188.0, cy: 141.0 },
      { tx: 3, ty: 7, rot: 18, cx: 188.0, cy: 141.0 },
    ],
  },
  {
    name: "mouth_left",
    kind: "BEZIER_LOOP",
    stroke: 3,
    color: "#c084fc",
    closed: true,
    points: [[153.0, 142.3], [159.2, 137.7], [167.0, 142.3]],
    poses: [
      {},
      { ty: -1, rot: 4, cx: 160.0, cy: 141.0 },
      { ty: 1, rot: -5, cx: 160.0, cy: 141.0 },
      { ty: 1, rot: 2, cx: 160.0, cy: 141.0 },
      { tx: -1, ty: 5, rot: -12, cx: 160.0, cy: 141.0 },
      { tx: -2, ty: 7, rot: -18, cx: 160.0, cy: 141.0 },
    ],
  },
  {
    name: "eye_right",
    kind: "BEZIER_LOOP",
    stroke: 2,
    color: "#81d4fa",
    closed: true,
    points: ellipsePoints(188.3, 159.2, 10.9, 21.0, 10),
    poses: [
      {},
      { ty: -1, sy: 1.05, cx: 188.3, cy: 159.2 },
      { ty: 1, sx: 0.98, sy: 0.95, cx: 188.3, cy: 159.2 },
      { sx: 0.98, sy: 1.02, cx: 188.3, cy: 159.2 },
      { tx: 2, ty: 6, rot: 8, sy: 0.72, cx: 188.3, cy: 159.2 },
      { tx: 3, ty: 8, rot: 10, sy: 0.6, cx: 188.3, cy: 159.2 },
    ],
  },
  {
    name: "pupil_right",
    kind: "BEZIER_LOOP",
    stroke: 2,
    color: "#4fc3f7",
    closed: true,
    points: ellipsePoints(182.6, 164.2, 2.9, 3.9, 8),
    poses: [
      {},
      { ty: -1, cx: 182.6, cy: 164.2 },
      { tx: 1, cy: 164.2 },
      { ty: 1, cx: 182.6, cy: 164.2 },
      { tx: 2, ty: 7, cx: 182.6, cy: 164.2 },
      { tx: 4, ty: 10, cx: 182.6, cy: 164.2 },
    ],
  },
  {
    name: "eye_left",
    kind: "BEZIER_LOOP",
    stroke: 2,
    color: "#81d4fa",
    closed: true,
    points: ellipsePoints(160.0, 159.2, 10.9, 21.0, 10),
    poses: [
      {},
      { ty: -1, sy: 1.05, cx: 160.0, cy: 159.2 },
      { ty: 1, sx: 0.98, sy: 0.95, cx: 160.0, cy: 159.2 },
      { sx: 0.98, sy: 1.02, cx: 160.0, cy: 159.2 },
      { tx: 1, ty: 6, rot: 5, sy: 0.72, cx: 160.0, cy: 159.2 },
      { tx: 2, ty: 8, rot: 7, sy: 0.6, cx: 160.0, cy: 159.2 },
    ],
  },
  {
    name: "pupil_left",
    kind: "BEZIER_LOOP",
    stroke: 2,
    color: "#4fc3f7",
    closed: true,
    points: ellipsePoints(153.0, 162.5, 2.9, 3.9, 8),
    poses: [
      {},
      { ty: -1, cx: 153.0, cy: 162.5 },
      { tx: 1, ty: 1, cx: 153.0, cy: 162.5 },
      { ty: 1, cx: 153.0, cy: 162.5 },
      { tx: 1, ty: 7, cx: 153.0, cy: 162.5 },
      { tx: 2, ty: 10, cx: 153.0, cy: 162.5 },
    ],
  },
  {
    name: "foot_1",
    kind: "BEZIER_LOOP",
    stroke: 3,
    color: "#ffd54f",
    closed: true,
    points: ellipsePoints(246.8, 216.8, 5.4, 7.4, 8),
    poses: [
      {},
      { tx: 0, ty: -3, cx: 246.8, cy: 216.8 },
      { tx: -3, ty: -8, cx: 246.8, cy: 216.8 },
      { tx: 1, ty: 1, cx: 246.8, cy: 216.8 },
      { tx: -6, ty: 9, cx: 246.8, cy: 216.8 },
      { tx: -8, ty: 13, cx: 246.8, cy: 216.8 },
    ],
  },
  {
    name: "foot_2",
    kind: "BEZIER_LOOP",
    stroke: 3,
    color: "#ffb74d",
    closed: true,
    points: ellipsePoints(242.6, 233.9, 5.4, 7.4, 8),
    poses: [
      {},
      { tx: -1, ty: 1, cx: 242.6, cy: 233.9 },
      { tx: 1, ty: -2, cx: 242.6, cy: 233.9 },
      { tx: 1, ty: 1, cx: 242.6, cy: 233.9 },
      { tx: -5, ty: 9, cx: 242.6, cy: 233.9 },
      { tx: -6, ty: 13, cx: 242.6, cy: 233.9 },
    ],
  },
  {
    name: "foot_3",
    kind: "BEZIER_LOOP",
    stroke: 3,
    color: "#f06292",
    closed: true,
    points: ellipsePoints(233.6, 247.2, 5.4, 7.4, 8),
    poses: [
      {},
      { tx: 1, ty: 3, cx: 233.6, cy: 247.2 },
      { tx: 3, ty: 7, cx: 233.6, cy: 247.2 },
      { tx: 0, ty: 2, cx: 233.6, cy: 247.2 },
      { tx: -4, ty: 11, cx: 233.6, cy: 247.2 },
      { tx: -6, ty: 15, cx: 233.6, cy: 247.2 },
    ],
  },
];

function buildLayer(part, index) {
  const keyframes = FRAME_TIMES.map((t, poseIndex) => ({
    t,
    h: 1,
    s: [
      toShape(
        transformPoints(part.points, part.poses[poseIndex] || {}),
        part.closed
      ),
    ],
  }));

  return {
    ddd: 0,
    ind: index + 1,
    ty: 4,
    nm: part.name,
    sr: 1,
    ks: {
      o: { a: 0, k: 100 },
      r: { a: 0, k: 0 },
      p: { a: 0, k: [0, 0, 0] },
      a: { a: 0, k: [0, 0, 0] },
      s: { a: 0, k: [100, 100, 100] },
    },
    ao: 0,
    ip: 0,
    op: 120,
    st: 0,
    shapes: [
      {
        ty: "sh",
        d: 1,
        ks: { a: 1, k: keyframes },
      },
      {
        ty: "st",
        c: { a: 0, k: [0, 0, 0, 1] },
        o: { a: 0, k: 100 },
        w: { a: 0, k: part.stroke },
        lc: 2,
        lj: 2,
      },
    ],
  };
}

function buildJson() {
  return {
    v: "5.7.4",
    fr: 30,
    ip: 0,
    op: 120,
    w: 390,
    h: 390,
    nm: "rig_path_actions_demo",
    ddd: 0,
    __gfx_sm: {
      schema_version: 2,
      prefix: "s_rig_path",
      name: "rig_path_scene",
      meta: { viewbox: [0, 0, 390, 390] },
      layout: {
        stroke_width: 4,
        mirror_x: 0,
        ground_y: 330,
        timer_period_ms: 33,
        damping_div: 4,
      },
      segments: PARTS.map((part) => ({
        layer: part.name,
        kind: part.kind,
        stroke_width: part.stroke,
      })),
    },
    markers: MARKERS,
    layers: PARTS.map(buildLayer),
  };
}

function buildPreview() {
  const poseXs = [80, 230, 380, 530, 680, 830];
  const labelY = 28;
  const colors = new Map(PARTS.map((part) => [part.name, part.color]));
  const width = 910;
  const height = 390;
  const items = [];
  items.push(`<rect x="0" y="0" width="${width}" height="${height}" fill="#f5efe5"/>`);

  FRAME_TIMES.forEach((_, poseIndex) => {
    const dx = poseXs[poseIndex] - 195;
    items.push(`<g transform="translate(${dx},0)">`);
    items.push(`<text x="195" y="${labelY}" font-size="14" fill="#5b4636" text-anchor="middle" font-family="sans-serif">${MARKERS[poseIndex].cm.split("|")[1]}</text>`);
    items.push(`<line x1="195" y1="42" x2="195" y2="355" stroke="#d7cec2" stroke-width="1" stroke-dasharray="4,4"/>`);
    items.push(`<line x1="50" y1="195" x2="340" y2="195" stroke="#d7cec2" stroke-width="1" stroke-dasharray="4,4"/>`);

    for (const part of PARTS) {
      const pts = transformPoints(part.points, part.poses[poseIndex] || {});
      items.push(
        `<path d="${svgPath(pts, part.closed)}" fill="none" stroke="${colors.get(part.name)}" stroke-width="${part.stroke}" stroke-linecap="round" stroke-linejoin="round"/>`
      );
    }
    items.push(`</g>`);
  });

  return [
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${width} ${height}" width="${width}" height="${height}">`,
    ...items,
    `</svg>`,
    "",
  ].join("\n");
}

fs.writeFileSync(OUT_JSON, JSON.stringify(buildJson(), null, 2) + "\n");
fs.writeFileSync(OUT_SVG, buildPreview());

console.log(`wrote ${OUT_JSON}`);
console.log(`wrote ${OUT_SVG}`);
