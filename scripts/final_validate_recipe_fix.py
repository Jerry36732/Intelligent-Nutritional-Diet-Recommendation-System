import os
import sqlite3

db_path = r"C:\Users\ROG\Documents\System\data\diet.db"
doc_path = r"C:\Users\ROG\Documents\System\食谱数据\智能营养膳食推荐系统_食谱数据手册.docx"

conn = sqlite3.connect(db_path)
print("recipes", conn.execute("SELECT COUNT(*) FROM recipes").fetchone()[0])
print("recipe_foods_schema", conn.execute("PRAGMA table_info(recipe_foods)").fetchall())
print("source_refs", conn.execute("SELECT source_ref FROM recipes LIMIT 5").fetchall())
print("pending", conn.execute("SELECT COUNT(*) FROM recipes WHERE total_calories IS NULL").fetchone()[0])
print(
    "fish",
    conn.execute(
        """
        SELECT r.name, rf.quantity, rf.display_name, f.name, f.source
        FROM recipes r
        JOIN recipe_foods rf ON rf.recipe_id = r.id
        LEFT JOIN foods f ON f.id = rf.food_id
        WHERE r.name = '油浸咸鱼'
        ORDER BY rf.rowid
        """
    ).fetchall(),
)
print("doc_size", os.path.getsize(doc_path))
