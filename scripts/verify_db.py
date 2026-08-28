# -*- coding: utf-8 -*-
import hashlib
import sqlite3

c = sqlite3.connect(r"C:\Users\ROG\Documents\System\build\msvc_debug\data\diet.db")
print("foods", c.execute("SELECT COUNT(*) FROM foods").fetchone()[0])
print("recipes", c.execute("SELECT COUNT(*) FROM recipes").fetchone()[0])
print("by meal", list(c.execute("SELECT category, COUNT(*) FROM recipes GROUP BY category")))
u = c.execute("SELECT name, password_hash FROM users WHERE name=?", ("张明",)).fetchone()
print("user", u[0] if u else None, "hash_set", bool(u and u[1]))
h = hashlib.sha256(("smartdiet" + "123456").encode()).hexdigest()
print("pwd_ok", u and u[1] == "smartdiet:" + h)
