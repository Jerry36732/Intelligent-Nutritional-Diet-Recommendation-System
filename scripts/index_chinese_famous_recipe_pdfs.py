# -*- coding: utf-8 -*-
"""用17册《中国名菜谱》为MDB菜谱补充二次来源索引。"""
from __future__ import annotations

import re
import sqlite3
from pathlib import Path

from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[1]
PDF_DIR = ROOT / "食谱数据" / "中国名菜谱"
DB = ROOT / "data" / "diet.db"


def compact(value: str) -> str:
    return re.sub(r"\s+", "", value or "")


def label(path: Path) -> str:
    match = re.search(r"中国名菜谱\s*([^（(]+)", path.stem)
    return (match.group(1).strip() if match else path.stem).replace("风味", "风味卷")


def main() -> None:
    pdfs = sorted(PDF_DIR.glob("*.pdf"))
    indices: list[tuple[Path, str]] = []
    for path in pdfs:
        reader = PdfReader(str(path))
        text = compact("".join((page.extract_text() or "") for page in reader.pages))
        indices.append((path, text))
        print(f"indexed {path.name}: {len(reader.pages)} pages, {len(text)} chars")

    conn = sqlite3.connect(DB)
    matched = 0
    by_pdf = {path.name: 0 for path in pdfs}
    for rid, name, source in conn.execute("SELECT id,name,COALESCE(source,'') FROM recipes ORDER BY id"):
        key = compact(name)
        if len(key) < 3:
            continue
        for path, text in indices:
            if key in text:
                suffix = f"；中国名菜谱（PDF复核：{label(path)}）"
                if suffix not in source:
                    conn.execute("UPDATE recipes SET source=? WHERE id=?", (source + suffix, rid))
                matched += 1
                by_pdf[path.name] += 1
                break
    conn.commit()
    conn.close()
    print(f"pdf_count={len(pdfs)} matched_recipes={matched}")
    for name, count in by_pdf.items():
        print(f"{name}: {count}")


if __name__ == "__main__":
    main()
