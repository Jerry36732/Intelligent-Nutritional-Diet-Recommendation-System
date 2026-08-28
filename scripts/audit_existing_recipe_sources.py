# -*- coding: utf-8 -*-
import json
import sqlite3
from collections import Counter
from pathlib import Path

from rebuild_recipes_from_mdb import FoodMatcher, extract_ingredients, ensure_reference_foods

root = Path(__file__).resolve().parents[1]
payload = json.loads((root / "食谱数据" / "mdb_recipes_raw.json").read_text(encoding="utf-8-sig"))
raw_by_id = {int(x.get("菜谱ID") or 0): x for x in payload["recipes"]}
conn = sqlite3.connect(root / "data" / "diet.db")
conn.row_factory = sqlite3.Row
ensure_reference_foods(conn)
matcher = FoodMatcher(conn)
missing = Counter()
affected = []
rows = conn.execute("SELECT name,source_ref FROM recipes WHERE source_ref LIKE 'MDB:%'").fetchall()
for row in rows:
    source_id = int(row["source_ref"].split(":", 1)[1])
    raw = raw_by_id.get(source_id)
    if not raw:
        affected.append((row["name"], source_id, ["缺少源记录"]))
        continue
    items = extract_ingredients(str(raw.get("原料") or ""))
    unresolved = [x["source"] for x in items if matcher.match(x["source"]) is None]
    if unresolved:
        affected.append((row["name"], source_id, unresolved))
        missing.update(unresolved)
print(json.dumps({"existing_mdb_recipes": len(rows), "affected": len(affected),
                  "missing_frequency": missing.most_common(), "examples": affected[:100]},
                 ensure_ascii=False, indent=2))
