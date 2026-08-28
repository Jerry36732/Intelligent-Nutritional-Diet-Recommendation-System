# -*- coding: utf-8 -*-
import json
import sqlite3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
conn = sqlite3.connect(ROOT / "data" / "diet.db")
conn.row_factory = sqlite3.Row

def rows(sql, params=()):
    return [dict(x) for x in conn.execute(sql, params).fetchall()]

result = {
    "recipe_count": conn.execute("SELECT COUNT(*) FROM recipes").fetchone()[0],
    "role_counts": rows("SELECT dish_role,COUNT(*) count FROM recipes GROUP BY dish_role ORDER BY dish_role"),
    "malformed_names": rows(
        "SELECT r.id,r.name,rf.display_name,rf.source_text FROM recipe_foods rf "
        "JOIN recipes r ON r.id=rf.recipe_id WHERE rf.display_name LIKE '%克。%' "
        "OR rf.display_name LIKE '%克.%' OR rf.display_name LIKE '%克精盐%'"
    ),
    "five_spice_fish": rows(
        "SELECT r.id,r.name,rf.display_name,rf.quantity,rf.source_text,f.name food_match "
        "FROM recipe_foods rf JOIN recipes r ON r.id=rf.recipe_id JOIN foods f ON f.id=rf.food_id "
        "WHERE r.name='五香鱼' ORDER BY rf.rowid"
    ),
    "chicken_leg": rows(
        "SELECT name,calories,protein,fat,carbs,source FROM foods WHERE name='鸡腿（官方）'"
    ),
    "extreme_quantities": rows(
        "SELECT r.id,r.name,rf.display_name,rf.quantity FROM recipe_foods rf "
        "JOIN recipes r ON r.id=rf.recipe_id WHERE rf.quantity>650 OR rf.quantity<0.2 ORDER BY rf.quantity DESC LIMIT 30"
    ),
    "bad_totals": rows(
        "SELECT id,name,total_weight,total_calories,total_protein,total_fat FROM recipes "
        "WHERE total_weight<50 OR total_weight>700 OR total_calories<5 OR total_calories>1000 "
        "OR total_protein>90 OR total_fat>80"
    ),
}
print(json.dumps(result, ensure_ascii=True, indent=2))
