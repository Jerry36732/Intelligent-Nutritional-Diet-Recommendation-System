import sqlite3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
s = (root / "src/engine/RecommendEngine.cpp").read_text(encoding="utf-8")
print("replacement_chars", s.count("\ufffd"))
start = s.index("MealSlot RecommendEngine::composeMulti")
end = s.index("RecommendResult RecommendEngine::generatePlan")
print(s[start:end])

c = sqlite3.connect(root / "data/diet.db")
print("recipes", c.execute("select count(*) from recipes").fetchone()[0])
print("linked", c.execute("select count(distinct recipe_id) from recipe_foods").fetchone()[0])
print("empty_steps", c.execute("select count(*) from recipes where steps is null or trim(steps)='' ").fetchone()[0])
print("detail_steps", c.execute("select count(*) from recipes where steps like '%详见原文%'").fetchone()[0])
print("roles", c.execute("select coalesce(dish_role,'null'),count(*) from recipes group by 1").fetchall())
print("rice", c.execute("select id,name,dish_role,total_calories from recipes where name like '%米饭%'").fetchall())
print("ingredient_count_dist", c.execute("select n,count(*) from (select recipe_id,count(*) n from recipe_foods group by recipe_id) group by n order by n").fetchall())
rows = c.execute("select id,name,source,steps from recipes order by id").fetchall()
bad = [r for r in rows if "�" in (r[1] or "") or "�" in (r[3] or "")]
print("bad_mojibake", len(bad))
print("bad_samples", [(r[0], r[1], r[2]) for r in bad[:20]])
print("sources", c.execute("select coalesce(source,'null'),count(*) from recipes group by 1").fetchall())
import json
print("rice_foods_json", json.dumps(c.execute("select id,name,calories,protein,fat,carbs from foods where name like '%米饭%' or name like '%粳米%' limit 30").fetchall(), ensure_ascii=True))
