#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BUILD_SPHINX=1
BUILD_API_RST=1
BUILD_DOXYGEN=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-sphinx)
      BUILD_SPHINX=0
      ;;
    --skip-api-rst)
      BUILD_API_RST=0
      ;;
    --skip-doxygen)
      BUILD_DOXYGEN=0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
  shift
done

DOC_BUILD_ROOT="docs/_build/html"
DOXYGEN_DIR="${DOC_BUILD_ROOT}/doxygen"
ASSETS_DIR="${DOC_BUILD_ROOT}/assets"
DOXYFILE_PATH="docs/_build/Doxyfile"
mkdir -p "$DOC_BUILD_ROOT" "$ASSETS_DIR" "${DOC_BUILD_ROOT}/en" "${DOC_BUILD_ROOT}/zh_CN" "$(dirname "$DOXYFILE_PATH")"

if [[ "$BUILD_API_RST" -eq 1 ]]; then
  echo "[docs] Generating API RST sources..."
  python3 docs/scripts/generate_api_docs.py --output-dir docs --quiet
fi

if [[ "$BUILD_SPHINX" -eq 1 ]]; then
  echo "[docs] Extracting gettext messages..."
  python3 -m sphinx -b gettext -d docs/_build/doctrees-gettext docs docs/_build/gettext
  echo "[docs] Building zh_CN message catalogs (.po/.mo)..."
  python3 docs/scripts/sync_locale_zh.py

  echo "[docs] Building Sphinx HTML (en)..."
  python3 -m sphinx -b html -d docs/_build/doctrees-en -D language=en docs "${DOC_BUILD_ROOT}/en"

  echo "[docs] Building Sphinx HTML (zh_CN)..."
  python3 -m sphinx -b html -d docs/_build/doctrees-zh -D language=zh_CN docs "${DOC_BUILD_ROOT}/zh_CN"
fi

# Root landing: language hub (no mixed-language pages; pick EN or 中文)
cat <<'EOF' > "${DOC_BUILD_ROOT}/index.html"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP Emote GFX Documentation</title>
  <meta http-equiv="refresh" content="0; url=en/index.html">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
           margin: 2rem; line-height: 1.6; background: #f7f7f8; color: #1a1a1a; }
    a { color: #c41e1a; text-decoration: none; font-weight: 600; }
    a:hover { text-decoration: underline; }
    ul { padding-left: 1.2rem; }
  </style>
</head>
<body>
  <p><strong>ESP Emote GFX</strong> — choose documentation language / 选择文档语言：</p>
  <ul>
    <li><a href="en/index.html">English (EN)</a></li>
    <li><a href="zh_CN/index.html">简体中文 (ZH)</a></li>
  </ul>
  <p>Redirecting to English… / 正在跳转至英文版…</p>
</body>
</html>
EOF

# Create a build-local Doxyfile so docs generation does not touch the repo root.
cat <<'EOF' > "$DOXYFILE_PATH"
PROJECT_NAME           = esp_emote_gfx
OUTPUT_DIRECTORY       = docs/doxygen_output
GENERATE_HTML          = YES
HTML_OUTPUT            = html
INPUT                  = . src include components
FILE_PATTERNS          = *.h *.hpp *.c *.cpp
RECURSIVE              = YES
EXTRACT_ALL            = YES
FULL_PATH_NAMES        = NO
GENERATE_LATEX         = NO
WARN_IF_UNDOCUMENTED   = NO
QUIET                  = YES
EOF

if [[ "$BUILD_DOXYGEN" -eq 1 ]] && ! command -v doxygen >/dev/null 2>&1; then
  echo "Warning: doxygen not found, Doxygen API docs will be skipped"
fi

rm -rf "$DOXYGEN_DIR"
mkdir -p "$DOXYGEN_DIR"

if [[ "$BUILD_DOXYGEN" -eq 1 ]] && command -v doxygen >/dev/null 2>&1; then
  doxygen "$DOXYFILE_PATH"
  if [ -d docs/doxygen_output/html ]; then
    cp -r docs/doxygen_output/html/. "$DOXYGEN_DIR"/
  fi
fi

if [ ! -f "$DOXYGEN_DIR/index.html" ]; then
  cat <<'EOF' > "$DOXYGEN_DIR/index.html"
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Doxygen API Reference</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 2rem; line-height: 1.6; background: #f7f7f8; }
    a { color: #c41e1a; text-decoration: none; font-weight: 600; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <h1>Doxygen API Reference</h1>
  <p>Doxygen documentation was not generated. Please check the build logs.</p>
  <p><a href="../en/index.html">← English docs</a> · <a href="../zh_CN/index.html">← 中文文档</a></p>
</body>
</html>
EOF
fi

cat <<'EOF' > "$ASSETS_DIR/espidf.css"
:root { --bg:#f7f7f8; --text:#1a1a1a; --accent:#c41e1a; --muted:#6a737d; --border:#d0d4d8; --code-bg:#f0f2f4; }
body { background:var(--bg); color:var(--text); font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,"Noto Sans",sans-serif; }
a { color:var(--accent); text-decoration:none; font-weight:600; } a:hover { text-decoration:underline; }
pre, code { background:var(--code-bg); border:1px solid var(--border); border-radius:4px; padding:.25rem .5rem; }
.header,.headertitle,.navpath,.footer,.memitem,.memdoc,.memberdecls,.directory { border-color:var(--border)!important; }
.memname { font-weight:600; } .mdescLeft,.mdescRight,.qindex { color:var(--muted); }
EOF

if [ -d "$DOXYGEN_DIR" ]; then
  cp "$ASSETS_DIR/espidf.css" "$DOXYGEN_DIR/espidf.css"
  python3 <<'PY'
import os, io
root = os.path.join("docs", "_build", "html", "doxygen")
css = '<link rel="stylesheet" href="espidf.css" />'
if not os.path.isdir(root):
    raise SystemExit(0)
for dirpath, _, files in os.walk(root):
    for name in files:
        if not name.endswith(".html"):
            continue
        path = os.path.join(dirpath, name)
        with io.open(path, "r", encoding="utf-8", errors="ignore") as fh:
            html = fh.read()
        if "espidf.css" in html:
            continue
        html = html.replace("</head>", css + "</head>", 1) if "</head>" in html else css + html
        with io.open(path, "w", encoding="utf-8") as fh:
            fh.write(html)
PY
fi

echo "Documentation post-processing complete."
echo "  - Sphinx EN:  ${DOC_BUILD_ROOT}/en/"
echo "  - Sphinx ZH:  ${DOC_BUILD_ROOT}/zh_CN/"
echo "  - Landing:    ${DOC_BUILD_ROOT}/index.html"
echo "  - Doxygen:    ${DOXYGEN_DIR}/"
