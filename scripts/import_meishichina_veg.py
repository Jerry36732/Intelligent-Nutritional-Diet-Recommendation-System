# -*- coding: utf-8 -*-
"""补充抓取素菜/凉菜分类。"""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "mc", Path(__file__).with_name("import_meishichina.py")
)
mc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mc)

import sqlite3
import time

conn = sqlite3.connect(mc.DB_PATH)
mc.ensure_columns(conn)
food_index = mc.load_food_index(conn)
seen = {
    r[0].split("recipe-")[-1].split(".")[0]
    for (r,) in conn.execute(
        "SELECT source_url FROM recipes WHERE ifnull(source_url,'')!=''"
    ).fetchall()
    if "recipe-" in (r or "")
}

scraped = []
for cat, meal, role in [("sucai", "晚餐", "vegetable"), ("liangcai", "晚餐", "vegetable")]:
    for page in range(1, 6):
        ids = mc.list_recipe_ids(cat, page)
        print(f"[{cat} p{page}] {len(ids)}", flush=True)
        time.sleep(0.12)
        for sid in ids:
            if sid in seen:
                continue
            seen.add(sid)
            recipe = mc.parse_recipe(sid, meal, role)
            time.sleep(0.12)
            if recipe:
                recipe.role = "vegetable"
                recipe.meal = "晚餐"
                scraped.append(recipe)
                print(" +", recipe.name, flush=True)
            if len(scraped) >= 50:
                break
        if len(scraped) >= 50:
            break
    if len(scraped) >= 50:
        break

ok = 0
for recipe in scraped:
    if mc.upsert_recipe(conn, recipe, food_index):
        ok += 1
conn.commit()
print("upserted", ok, "total", conn.execute("select count(*) from recipes").fetchone()[0], flush=True)
print(
    conn.execute(
        "select ifnull(dish_role,'?'), count(*) from recipes group by 1 order by 2 desc"
    ).fetchall(),
    flush=True,
)
conn.close()
