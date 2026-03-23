#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$DOCS_DIR/.." && pwd)"

STRICT=0

while (($# > 0)); do
  case "$1" in
    --strict)
      STRICT=1
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
  shift
done

cd "$REPO_ROOT"

echo "[1/4] Generate RST API docs"
python3 docs/scripts/generate_api_docs.py

echo "[2/4] Generate Doxygen artifacts"
rm -rf docs/_build/doxygen
if command -v doxygen >/dev/null 2>&1; then
  doxygen docs/Doxyfile
else
  echo "Warning: doxygen not found; C API HTML/XML will be skipped"
fi

echo "[3/4] Build Sphinx site"
mkdir -p docs/_build/html
if [ "$STRICT" -eq 1 ]; then
  python3 -m sphinx -W --keep-going -b html docs docs/_build/html
else
  python3 -m sphinx -b html docs docs/_build/html
fi

echo "[4/4] Finalize site assets"
bash docs/scripts/postprocess_docs.sh
touch docs/_build/html/.nojekyll

echo "Documentation build complete: docs/_build/html/index.html"
