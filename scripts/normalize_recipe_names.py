# -*- coding: utf-8 -*-
"""规范化菜谱名称并补充早餐饮品。"""
import sqlite3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / "data" / "diet.db"


def normalize(name: str) -> str:
    name = (name or "").strip()
    if name.endswith("的做法"):
        name = name[:-len("的做法")].strip()
    if name.endswith("做法") and len(name) > 2:
        name = name[:-2].strip()
    return name


def main():
    conn = sqlite3.connect(DB)
    rows = conn.execute("SELECT id, name FROM recipes").fetchall()
    updated = 0
    for rid, name in rows:
        new_name = normalize(name)
        if new_name and new_name != name:
            conn.execute("UPDATE recipes SET name=? WHERE id=?", (new_name, rid))
            updated += 1

    # 按名称标记饮品
    drink_kw = ("汁", "牛奶", "豆浆", "酸奶", "拿铁", "咖啡", "奶茶", "米浆", "椰汁", "梨汁", "苹果汁", "橙汁", "柠檬")
    for rid, name in conn.execute("SELECT id, name FROM recipes"):
        if any(k in name for k in drink_kw):
            conn.execute(
                "UPDATE recipes SET dish_role='drink', category='早餐' WHERE id=? AND (dish_role IS NULL OR dish_role='mixed' OR dish_role='breakfast')",
                (rid,),
            )

    # 补充常见早餐饮品（若不存在）
    seeds = [
        ("苹果汁", 85, 0.2, 20.0, 0.1),
        ("梨子汁", 80, 0.2, 19.0, 0.1),
        ("橙汁", 90, 0.5, 21.0, 0.1),
        ("牛奶", 120, 6.0, 9.0, 6.5),
        ("豆浆", 95, 5.0, 8.0, 4.5),
        ("酸奶", 110, 4.0, 14.0, 3.5),
    ]
    for name, cal, p, c, f in seeds:
        exists = conn.execute("SELECT id FROM recipes WHERE name=?", (name,)).fetchone()
        if exists:
            conn.execute(
                "UPDATE recipes SET dish_role='drink', category='早餐', total_calories=?, total_protein=?, total_carbs=?, total_fat=? WHERE id=?",
                (cal, p, c, f, exists[0]),
            )
            continue
        conn.execute(
            """INSERT INTO recipes(name, category, steps, cook_minutes, accent, dish_role,
               total_calories, total_protein, total_carbs, total_fat, source)
               VALUES (?,?,?,?,?,?,?,?,?,?,?)""",
            (
                name,
                "早餐",
                f"将{name}作为早餐搭配饮品，适量饮用即可。",
                2,
                "teal",
                "drink",
                cal,
                p,
                c,
                f,
                "seed",
            ),
        )

    conn.commit()
    total = conn.execute("SELECT count(*) FROM recipes").fetchone()[0]
    drinks = conn.execute("SELECT count(*) FROM recipes WHERE dish_role='drink'").fetchone()[0]
    print(f"normalized_names={updated} total_recipes={total} drinks={drinks}")
    conn.close()


if __name__ == "__main__":
    main()
