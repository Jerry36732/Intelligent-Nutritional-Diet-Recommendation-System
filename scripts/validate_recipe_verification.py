import sqlite3
from pathlib import Path

db = Path(__file__).resolve().parents[1] / "data" / "diet.db"
c = sqlite3.connect(db)

diff_rows = []
for rid, saved_cal, saved_p, saved_f, saved_c in c.execute(
    "SELECT id,total_calories,total_protein,total_fat,total_carbs FROM recipes"
):
    totals = [0.0, 0.0, 0.0, 0.0]
    for cal, protein, fat, carbs, qty in c.execute(
        "SELECT COALESCE(f.calories,0),COALESCE(f.protein,0),COALESCE(f.fat,0),"
        "COALESCE(f.carbs,0),rf.quantity FROM recipe_foods rf JOIN foods f ON f.id=rf.food_id "
        "WHERE rf.recipe_id=? ORDER BY rf.rowid", (rid,)
    ):
        for i, value in enumerate((cal, protein, fat, carbs)):
            totals[i] += value * qty / 100.0
    calc = tuple(round(x, 1) for x in totals)
    saved = tuple(round(float(x or 0), 1) for x in (saved_cal, saved_p, saved_f, saved_c))
    # 浮点数求和顺序在 x.x5 边界可能产生 0.1 的舍入差；允许 0.1g/kcal 误差。
    if any(abs(a - b) > 0.11 for a, b in zip(calc, saved)):
        diff_rows.append((rid, *calc, *saved))

summary = {
    "recipes": c.execute("SELECT COUNT(*) FROM recipes").fetchone()[0],
    "linked": c.execute("SELECT COUNT(DISTINCT recipe_id) FROM recipe_foods").fetchone()[0],
    "nutrition_mismatches": len(diff_rows),
    "steps_under_4": c.execute(
        "SELECT COUNT(*) FROM recipes WHERE "
        "(length(steps)-length(replace(steps,char(10),''))+1)<4"
    ).fetchone()[0],
    "white_rice": c.execute(
        "SELECT name,dish_role,total_calories,total_protein,total_fat,total_carbs "
        "FROM recipes WHERE name='白米饭'"
    ).fetchall(),
}
print(summary)
if diff_rows:
    print("diff_samples", diff_rows[:10])
assert summary["recipes"] == summary["linked"]
assert summary["nutrition_mismatches"] == 0
assert summary["steps_under_4"] == 0
assert len(summary["white_rice"]) == 1
