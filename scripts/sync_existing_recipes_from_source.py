# -*- coding: utf-8 -*-
"""保留现有菜谱，按MDB原料字段补齐用料；未知食材显式待核验，绝不近似替代。"""
from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
from pathlib import Path

from rebuild_recipes_from_mdb import (
    FoodMatcher, canonical_source, display_name, ensure_reference_foods,
    extract_ingredients, mark_micro_ingredients, merge_same_food, nutrition, scale_to_serving,
)

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / "data" / "diet.db"
RAW = ROOT / "食谱数据" / "mdb_recipes_raw.json"


def placeholder_food(conn: sqlite3.Connection, source: str) -> dict:
    canonical = canonical_source(source) or source.strip()
    name = f"待精确核验：{canonical}"
    row = conn.execute("SELECT * FROM foods WHERE name=?", (name,)).fetchone()
    if row:
        return dict(row)
    digest = int(hashlib.sha1(canonical.encode("utf-8")).hexdigest()[:7], 16)
    source_id = -10000000 - digest
    while conn.execute("SELECT 1 FROM foods WHERE source_id=?", (source_id,)).fetchone():
        source_id -= 1
    cur = conn.execute(
        "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source) "
        "VALUES(?,?,NULL,NULL,NULL,NULL,'100g','待精确检索：禁止近似替代')",
        (source_id, name),
    )
    return dict(conn.execute("SELECT * FROM foods WHERE id=?", (cur.lastrowid,)).fetchone())


def ignored_micro_food(conn: sqlite3.Connection, source: str) -> dict:
    canonical = canonical_source(source) or source.strip()
    name = f"微量调料：{canonical}"
    row = conn.execute("SELECT * FROM foods WHERE name=?", (name,)).fetchone()
    if row:
        return dict(row)
    digest = int(hashlib.sha1(("micro:" + canonical).encode("utf-8")).hexdigest()[:7], 16)
    source_id = -20000000 - digest
    while conn.execute("SELECT 1 FROM foods WHERE source_id=?", (source_id,)).fetchone():
        source_id -= 1
    cur = conn.execute(
        "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source) "
        "VALUES(?,?,0,0,0,0,'100g','微量香料/调味料：按规则忽略营养贡献')",
        (source_id, name),
    )
    return dict(conn.execute("SELECT * FROM foods WHERE id=?", (cur.lastrowid,)).fetchone())


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", type=Path, default=DB)
    parser.add_argument("--recipe-id", type=int, action="append",
                        help="仅重建指定的本地 recipe id；可重复传入")
    args = parser.parse_args()
    requested_ids = set(args.recipe_id or [])
    payload = json.loads(RAW.read_text(encoding="utf-8-sig"))
    raw_by_id = {int(x.get("菜谱ID") or 0): x for x in payload["recipes"]}
    conn = sqlite3.connect(args.db)
    conn.row_factory = sqlite3.Row
    ensure_reference_foods(conn)
    matcher = FoodMatcher(conn)
    complete = incomplete = 0
    unresolved_names = set()

    for recipe in conn.execute("SELECT id,name,dish_role,source_ref FROM recipes ORDER BY id").fetchall():
        if requested_ids and int(recipe["id"]) not in requested_ids:
            continue
        ref = recipe["source_ref"] or ""
        if not ref.startswith("MDB:"):
            continue
        raw = raw_by_id.get(int(ref.split(":", 1)[1]))
        if not raw:
            continue
        source_items = extract_ingredients(str(raw.get("原料") or ""))
        mapped, unresolved = [], []
        for item in source_items:
            food = matcher.match(item["source"])
            if food is None:
                canonical = canonical_source(item["source"]) or item["source"]
                if item["quantity"] <= 12 and any(x in canonical for x in ("盐","味精","鸡精","花椒","大料","八角","桂皮","胡椒","香料")):
                    food = ignored_micro_food(conn, item["source"])
                else:
                    food = placeholder_food(conn, item["source"])
                    unresolved.append(canonical)
                    unresolved_names.add(canonical)
            mapped.append({"food": food, "source": item["source"],
                           "display": canonical_source(item["source"]) or display_name(food["name"], item["source"]),
                           "quantity": float(item["quantity"]),
                           "raw_text": item.get("raw_text", item["source"]),
                           "estimated_from_count": item.get("estimated_from_count", False)})
        mapped = merge_same_food(mapped)
        mark_micro_ingredients(mapped)
        if not mapped:
            continue
        scale_to_serving(mapped, recipe["dish_role"] or "mixed")
        for item in mapped:
            item["quantity"] = round(item["quantity"], 1)
        conn.execute("DELETE FROM recipe_foods WHERE recipe_id=?", (recipe["id"],))
        for item in mapped:
            source_text = item.get("raw_text", item["source"])
            if item.get("estimated_from_count"):
                source_text += "【按计数单位估重】"
            if item.get("ignore_nutrition"):
                source_text += "【营养忽略】"
            conn.execute(
                "INSERT INTO recipe_foods(recipe_id,food_id,quantity,display_name,source_text) VALUES(?,?,?,?,?)",
                (recipe["id"], item["food"]["id"], item["quantity"], item["display"],
                 source_text),
            )
        if unresolved:
            incomplete += 1
            conn.execute(
                "UPDATE recipes SET total_weight=?,total_calories=NULL,total_protein=NULL,total_fat=NULL,"
                "total_carbs=NULL,per100_calories=NULL,per100_protein=NULL,per100_fat=NULL,per100_carbs=NULL,"
                "nutrition_verified_at=? WHERE id=?",
                (round(sum(x["quantity"] for x in mapped), 1),
                 "待精确检索：" + "、".join(sorted(set(unresolved))), recipe["id"]),
            )
        else:
            complete += 1
            n = nutrition(mapped)
            conn.execute(
                "UPDATE recipes SET total_weight=?,total_calories=?,total_protein=?,total_fat=?,total_carbs=?,"
                "per100_calories=?,per100_protein=?,per100_fat=?,per100_carbs=?,nutrition_verified_at='精确匹配已核验' "
                "WHERE id=?",
                (round(n["weight"],1), round(n["totals"]["calories"],1), round(n["totals"]["protein"],1),
                 round(n["totals"]["fat"],1), round(n["totals"]["carbs"],1),
                 round(n["per100"]["calories"],1), round(n["per100"]["protein"],1),
                 round(n["per100"]["fat"],1), round(n["per100"]["carbs"],1), recipe["id"]),
            )
    conn.commit()
    oil = conn.execute(
        "SELECT r.name,rf.display_name,rf.quantity,f.name FROM recipes r JOIN recipe_foods rf ON rf.recipe_id=r.id "
        "JOIN foods f ON f.id=rf.food_id WHERE r.source_ref='MDB:3514' ORDER BY rf.rowid"
    ).fetchall()
    print(json.dumps({"complete": complete, "incomplete_retained": incomplete,
                      "unresolved_foods": sorted(unresolved_names),
                      "oil_soaked_fish": [tuple(x) for x in oil]}, ensure_ascii=True, indent=2))
    conn.close()


if __name__ == "__main__":
    main()
