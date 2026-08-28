# -*- coding: utf-8 -*-
import sqlite3
from pathlib import Path

DB = Path(r"C:\Users\ROG\Documents\System\data\diet.db")
c = sqlite3.connect(DB)
for kw in ["油", "豆腐", "鸡", "燕麦", "酸奶", "核桃", "蓝莓", "菠菜", "青椒", "花生", "生菜", "海带", "排骨", "茄子", "芹菜", "冬瓜", "香菇", "糙米", "虾", "鲈", "植物", "大豆油", "花生油"]:
    rows = c.execute("SELECT name FROM foods WHERE name LIKE ? LIMIT 5", (f"%{kw}%",)).fetchall()
    print(kw, [r[0] for r in rows])
print("recipe count", c.execute("SELECT COUNT(*) FROM recipes").fetchone())
