#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

DOC_BUILD_DIR="docs/_build/html"
DOXYGEN_DIR="${DOC_BUILD_DIR}/doxygen"
ASSETS_DIR="${DOC_BUILD_DIR}/assets"

mkdir -p "$DOC_BUILD_DIR" "$ASSETS_DIR"

# Create Doxyfile if it doesn't exist
if [ ! -f Doxyfile ]; then
  cat <<'EOF' > Doxyfile
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
fi

# Doxygen and graphviz should be installed by the CI workflow
if ! command -v doxygen >/dev/null 2>&1; then
  echo "Warning: doxygen not found, Doxygen API docs will be skipped"
fi

# Create doxygen output directory (separate from Sphinx api/)
rm -rf "$DOXYGEN_DIR"
mkdir -p "$DOXYGEN_DIR"

# Run doxygen if available
if command -v doxygen >/dev/null 2>&1; then
  doxygen Doxyfile
  
  # Copy doxygen output to the doxygen directory
  if [ -d docs/doxygen_output/html ]; then
    cp -r docs/doxygen_output/html/. "$DOXYGEN_DIR"/
  fi
fi

# Create fallback doxygen index if no output was generated
if [ ! -f "$DOXYGEN_DIR/index.html" ]; then
  cat <<'EOF' > "$DOXYGEN_DIR/index.html"
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Doxygen API Reference</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 2rem; line-height: 1.6; }
    a { color: #0052cc; text-decoration: none; }
    a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <h1>Doxygen API Reference</h1>
  <p>Doxygen documentation was not generated. Please check the build logs.</p>
  <p><a href="../index.html">← Back to Documentation</a></p>
</body>
</html>
EOF
fi

# Create custom CSS for styling
cat <<'EOF' > "$ASSETS_DIR/espidf.css"
:root { --bg:#fff; --text:#1f2328; --accent:#0052cc; --muted:#6a737d; --border:#e1e4e8; --code-bg:#f6f8fa; }
body { background:var(--bg); color:var(--text); font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,"Noto Sans",sans-serif; }
a { color:var(--accent); text-decoration:none; } a:hover { text-decoration:underline; }
pre, code { background:var(--code-bg); border:1px solid var(--border); border-radius:4px; padding:.25rem .5rem; }
.header,.headertitle,.navpath,.footer,.memitem,.memdoc,.memberdecls,.directory { border-color:var(--border)!important; }
.memname { font-weight:600; } .mdescLeft,.mdescRight,.qindex { color:var(--muted); }
EOF

# Copy CSS to doxygen directory if it exists
if [ -d "$DOXYGEN_DIR" ]; then
  cp "$ASSETS_DIR/espidf.css" "$DOXYGEN_DIR/espidf.css"
  
  # Inject custom CSS into all doxygen HTML files
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
echo "  - Sphinx docs: $DOC_BUILD_DIR/"
echo "  - Doxygen docs: $DOXYGEN_DIR/"
