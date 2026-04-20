#!/usr/bin/env python3
"""
Build zh_CN .po/.mo from gettext .pot files + docs/locale/zh_CN/translation_data.py

Requires: Babel (usually installed with Sphinx). Requires docs/_build/gettext/*.pot
(run: sphinx-build -b gettext docs docs/_build/gettext).
"""
from __future__ import annotations

import io
import sys
from pathlib import Path

try:
    from babel.messages.mofile import write_mo
    from babel.messages.pofile import read_po, write_po
except ImportError as e:
    print("Babel is required (pip install Babel).", file=sys.stderr)
    raise SystemExit(1) from e


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    docs_dir = script_dir.parent
    repo_root = docs_dir.parent
    gettext_dir = docs_dir / "_build" / "gettext"
    locale_msgs = docs_dir / "locale" / "zh_CN" / "LC_MESSAGES"
    locale_msgs.mkdir(parents=True, exist_ok=True)

    sys.path.insert(0, str(docs_dir / "locale" / "zh_CN"))
    from translation_data import TRANSLATIONS_BY_CATALOG  # type: ignore

    if not gettext_dir.is_dir():
        print(f"Missing {gettext_dir}; run sphinx-build -b gettext first.", file=sys.stderr)
        return 1

    for pot_path in sorted(gettext_dir.glob("*.pot")):
        name = pot_path.stem
        catalog = read_po(io.open(pot_path, encoding="utf-8"))
        catalog.locale = "zh_CN"
        catalog.fuzzy = False

        trans = TRANSLATIONS_BY_CATALOG.get(name, {})
        for msg in catalog:
            if not msg.id:
                continue
            if isinstance(msg.id, (list, tuple)):
                continue
            if msg.id in trans:
                msg.string = trans[msg.id]

        out_po = locale_msgs / f"{name}.po"
        buf_po = io.BytesIO()
        write_po(buf_po, catalog, omit_header=False, width=79)
        out_po.write_text(buf_po.getvalue().decode("utf-8"), encoding="utf-8")

        buf = io.BytesIO()
        write_mo(buf, catalog)
        mo_path = locale_msgs / f"{name}.mo"
        mo_path.write_bytes(buf.getvalue())
        print(f"  wrote {out_po.relative_to(repo_root)} + .mo")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
