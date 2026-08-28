# -*- coding: utf-8 -*-
"""规范现有食谱标题，并输出变更清单。"""
from __future__ import annotations

import sqlite3
from pathlib import Path

from rebuild_recipes_from_mdb import normalize_recipe_name

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / "data" / "diet.db"


def main() -> None:
    conn = sqlite3.connect(DB)
    rows = conn.execute("SELECT id,name FROM recipes ORDER BY id").fetchall()
    changes = []
    for recipe_id, old_name in rows:
        new_name = normalize_recipe_name(old_name)
        if not new_name or new_name == old_name:
            continue
        conn.execute("UPDATE recipes SET name=? WHERE id=?", (new_name, recipe_id))
        changes.append((recipe_id, old_name, new_name))
    conn.commit()
    remaining = [
        (recipe_id, name)
        for recipe_id, name in conn.execute("SELECT id,name FROM recipes ORDER BY id")
        if normalize_recipe_name(name) != name
    ]
    dash_names = [
        (recipe_id, name)
        for recipe_id, name in conn.execute("SELECT id,name FROM recipes ORDER BY id")
        if any(mark in name for mark in ("-", "－", "—", "_"))
    ]
    conn.close()
    print(f"renamed={len(changes)}")
    for item in changes:
        print(f"{item[0]}\t{item[1]}\t=>\t{item[2]}")
    print(f"remaining_non_normalized={len(remaining)}")
    print(f"remaining_dash_or_underscore={len(dash_names)}")
    for item in dash_names:
        print(f"DASH\t{item[0]}\t{item[1]}")


if __name__ == "__main__":
    main()
