#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BUILD_API_RST=1
BUILD_DOXYGEN=1
DOC_BUILDERS="${DOC_BUILDERS:-html}"
DOC_BUILD_ROOT="docs/_build/html"
ESP_DOCS_SOURCE_ROOT="docs/_build/esp_docs_src"
ESP_DOCS_BUILD_ROOT="docs/_build/esp_docs"
DOXYGEN_DIR="${DOC_BUILD_ROOT}/doxygen"
DOXYFILE_PATH="docs/_build/Doxyfile"

if [[ -n "${DOCLANG:-}" ]]; then
  DOC_LANGS=("$DOCLANG")
else
  DOC_LANGS=("en" "zh_CN")
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --builders)
      DOC_BUILDERS="$2"
      shift
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

stage_docs_source() {
  local lang="$1"
  local dst="${ESP_DOCS_SOURCE_ROOT}/${lang}"

  rm -rf "$dst"
  mkdir -p "$dst"

  cp -a docs/conf.py "$dst/"
  cp -a docs/*.rst "$dst/"
  cp -a docs/api "$dst/"
  cp -a docs/_static "$dst/"
  cp -a docs/_templates "$dst/"
  cp -a docs/locale "$dst/"
}

copy_esp_docs_html() {
  local lang="$1"
  local html_dir

  html_dir="$(find "${ESP_DOCS_BUILD_ROOT}/${lang}" -mindepth 2 -maxdepth 2 -type d -name html 2>/dev/null | head -n 1)"
  if [[ -z "$html_dir" || ! -d "$html_dir" ]]; then
    echo "Cannot find esp-docs HTML output for language: ${lang}" >&2
    return 1
  fi

  rm -rf "${DOC_BUILD_ROOT:?}/${lang}"
  mkdir -p "${DOC_BUILD_ROOT}/${lang}"
  cp -a "${html_dir}/." "${DOC_BUILD_ROOT}/${lang}/"
}

write_landing_page() {
  mkdir -p "$DOC_BUILD_ROOT"
  cat <<'EOF' > "${DOC_BUILD_ROOT}/index.html"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP Emote GFX Documentation</title>
  <meta http-equiv="refresh" content="0; url=en/index.html">
  <style>
    :root { --accent: #e7352c; --ink: #1f2933; --muted: #64748b; --line: #d9dee7; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at 20% 10%, rgba(231, 53, 44, .12), transparent 28rem),
        linear-gradient(135deg, #fff 0%, #f5f7fb 100%);
      display: grid;
      place-items: center;
    }
    main {
      width: min(42rem, calc(100vw - 2rem));
      background: rgba(255, 255, 255, .88);
      border: 1px solid var(--line);
      border-radius: 16px;
      box-shadow: 0 24px 80px rgba(15, 23, 42, .10);
      padding: 2rem;
    }
    h1 { margin: 0 0 .5rem; font-size: clamp(1.8rem, 4vw, 2.6rem); }
    p { color: var(--muted); line-height: 1.6; }
    .links { display: flex; flex-wrap: wrap; gap: .75rem; margin-top: 1.5rem; }
    a {
      color: #fff;
      background: var(--accent);
      border-radius: 999px;
      padding: .75rem 1rem;
      text-decoration: none;
      font-weight: 700;
    }
    a.secondary { color: var(--accent); background: #fff; border: 1px solid rgba(231, 53, 44, .28); }
  </style>
</head>
<body>
  <main>
    <h1>ESP Emote GFX</h1>
    <p>Choose documentation language / 选择文档语言。The page will redirect to English automatically.</p>
    <div class="links">
      <a href="en/index.html">English</a>
      <a class="secondary" href="zh_CN/index.html">简体中文</a>
    </div>
  </main>
</body>
</html>
EOF
}

build_doxygen() {
  mkdir -p "$(dirname "$DOXYFILE_PATH")"
  cat <<'EOF' > "$DOXYFILE_PATH"
PROJECT_NAME           = esp_emote_gfx
OUTPUT_DIRECTORY       = docs/doxygen_output
GENERATE_HTML          = YES
HTML_OUTPUT            = html
INPUT                  = src include
FILE_PATTERNS          = *.h *.hpp *.c *.cpp
RECURSIVE              = YES
EXTRACT_ALL            = YES
FULL_PATH_NAMES        = NO
GENERATE_LATEX         = NO
WARN_IF_UNDOCUMENTED   = NO
WARN_IF_DOC_ERROR      = NO
WARN_NO_PARAMDOC       = NO
EXCLUDE_PATTERNS       = */_build/* */components/*
HAVE_DOT               = NO
CLASS_DIAGRAMS         = NO
CLASS_GRAPH            = NO
COLLABORATION_GRAPH    = NO
GROUP_GRAPHS           = NO
INCLUDE_GRAPH          = NO
INCLUDED_BY_GRAPH      = NO
CALL_GRAPH             = NO
CALLER_GRAPH           = NO
QUIET                  = YES
EOF

  if [[ "$BUILD_DOXYGEN" -eq 1 ]] && ! command -v doxygen >/dev/null 2>&1; then
    echo "Warning: doxygen not found, Doxygen API docs will be skipped"
  fi

  rm -rf "$DOXYGEN_DIR" docs/doxygen_output
  mkdir -p "$DOXYGEN_DIR"

  if [[ "$BUILD_DOXYGEN" -eq 1 ]] && command -v doxygen >/dev/null 2>&1; then
    doxygen "$DOXYFILE_PATH"
    if [[ -d docs/doxygen_output/html ]]; then
      cp -a docs/doxygen_output/html/. "$DOXYGEN_DIR"/
    fi
  fi

  if [[ ! -f "$DOXYGEN_DIR/index.html" ]]; then
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

  if [[ -d "$DOXYGEN_DIR" ]]; then
    python3 <<'PY'
import os, io
root = os.path.join("docs", "_build", "html", "doxygen")
css = '<link rel="stylesheet" href="../en/_static/esp_emote_gfx.css" />'
if not os.path.isdir(root):
    raise SystemExit(0)
for dirpath, _, files in os.walk(root):
    for name in files:
        if not name.endswith(".html"):
            continue
        path = os.path.join(dirpath, name)
        with io.open(path, "r", encoding="utf-8", errors="ignore") as fh:
            html = fh.read()
        if "esp_emote_gfx.css" in html:
            continue
        html = html.replace("</head>", css + "</head>", 1) if "</head>" in html else css + html
        with io.open(path, "w", encoding="utf-8") as fh:
            fh.write(html)
PY
  fi
}

if [[ "$BUILD_API_RST" -eq 1 ]]; then
  echo "[docs] Generating API RST sources..."
  python3 docs/scripts/generate_api_docs.py --output-dir docs --quiet
fi

echo "[docs] Extracting gettext messages..."
rm -rf docs/_build/gettext docs/_build/doctrees-gettext
python3 -m sphinx -b gettext -d docs/_build/doctrees-gettext docs docs/_build/gettext
echo "[docs] Building zh_CN message catalogs (.po/.mo)..."
python3 docs/scripts/sync_locale_zh.py

rm -rf "$ESP_DOCS_SOURCE_ROOT" "$ESP_DOCS_BUILD_ROOT"
for lang in "${DOC_LANGS[@]}"; do
  stage_docs_source "$lang"
done

for lang in "${DOC_LANGS[@]}"; do
  echo "[docs] build-docs language=${lang} builders=${DOC_BUILDERS}"
  build-docs -bs "$DOC_BUILDERS" -l "$lang" -s "$ESP_DOCS_SOURCE_ROOT" -b "$ESP_DOCS_BUILD_ROOT" build
  if [[ "$DOC_BUILDERS" == *html* ]]; then
    copy_esp_docs_html "$lang"
  fi
done

write_landing_page
build_doxygen

echo "Documentation build complete."
echo "  - Sphinx EN:  ${DOC_BUILD_ROOT}/en/"
echo "  - Sphinx ZH:  ${DOC_BUILD_ROOT}/zh_CN/"
echo "  - Landing:    ${DOC_BUILD_ROOT}/index.html"
echo "  - Doxygen:    ${DOXYGEN_DIR}/"
