# -*- coding: utf-8 -*-
"""重新播种 ≥30 道食谱，食材关键词适配中国食物成分表命名。"""
from __future__ import annotations

import sqlite3
from pathlib import Path

DB = Path(__file__).resolve().parent.parent / "data" / "diet.db"

# 关键词候选列表（按优先级）
ALIASES = {
    "油": ["花生油", "菜籽油", "大豆油", "色拉油", "调和油", "玉米油"],
    "豆腐": ["北豆腐", "南豆腐", "内酯豆腐", "豆腐(北)", "豆腐"],
    "鸡胸": ["鸡胸脯肉", "鸡胸肉", "鸡胸", "鸡腿"],
    "鸡": ["鸡", "鸡肉"],
    "燕麦": ["燕麦片", "燕麦"],
    "酸奶": ["酸奶", "酸牛奶"],
    "核桃": ["核桃", "核桃仁"],
    "大米": ["粳米", "籼米", "大米", "稻米"],
    "糙米": ["糙米", "粳米"],
    "鸡蛋": ["鸡蛋", "鸡全蛋"],
    "番茄": ["番茄", "西红柿"],
    "牛奶": ["牛乳", "牛奶", "纯牛奶"],
    "香蕉": ["香蕉"],
    "菠菜": ["菠菜"],
    "小米": ["小米"],
    "南瓜": ["南瓜"],
    "馒头": ["馒头"],
    "豆浆": ["豆浆", "豆奶"],
    "西兰花": ["绿菜花", "西兰花", "青花菜"],
    "猪肉": ["猪肉", "里脊", "猪瘦肉"],
    "青椒": ["柿子椒", "青椒", "甜椒", "辣椒"],
    "虾": ["虾", "对虾", "基围虾"],
    "黄瓜": ["黄瓜"],
    "牛肉": ["牛肉", "牛里脊", "牛腱"],
    "洋葱": ["洋葱"],
    "土豆": ["马铃薯", "土豆"],
    "胡萝卜": ["胡萝卜"],
    "茄子": ["茄子"],
    "花生": ["花生", "花生仁"],
    "鲈鱼": ["鲈鱼", "鲈"],
    "小白菜": ["小白菜", "青菜", "油菜"],
    "海带": ["海带"],
    "生菜": ["生菜", "叶用莴苣"],
    "紫薯": ["紫薯", "甘薯"],
    "玉米": ["玉米", "甜玉米"],
    "芹菜": ["芹菜", "旱芹"],
    "排骨": ["猪排骨", "排骨", "肋排"],
    "冬瓜": ["冬瓜"],
    "香菇": ["香菇"],
    "大白菜": ["大白菜", "白菜"],
    "麦片": ["燕麦片", "麦片"],
}


def find_food(conn, key):
    candidates = ALIASES.get(key, [key])
    for kw in candidates:
        rows = conn.execute(
            "SELECT id, name, calories, protein, fat, carbs FROM foods "
            "WHERE name LIKE ? AND calories IS NOT NULL "
            "ORDER BY CASE WHEN name LIKE ? THEN 0 ELSE 1 END, length(name) ASC LIMIT 1",
            (f"%{kw}%", f"{kw}%"),
        ).fetchall()
        if rows:
            return rows[0]
    return None


RECIPES = [
    ("燕麦酸奶碗", "早餐", 10, "orange",
     "燕麦用热水浸泡5分钟。\n拌入酸奶。\n撒上核桃碎即可。",
     [("燕麦", 50), ("酸奶", 150), ("核桃", 15)]),
    ("全蛋番茄早餐", "早餐", 10, "orange",
     "鸡蛋炒熟。\n番茄切块同炒。\n配馒头食用。",
     [("鸡蛋", 100), ("番茄", 120), ("馒头", 80), ("油", 5)]),
    ("牛奶燕麦", "早餐", 5, "orange",
     "燕麦倒入碗中。\n冲入温牛奶搅拌均匀。",
     [("燕麦", 45), ("牛奶", 220)]),
    ("豆腐青菜粥", "早餐", 25, "orange",
     "大米煮粥。\n加入豆腐丁与菠菜。\n少许盐调味。",
     [("大米", 40), ("豆腐", 80), ("菠菜", 50)]),
    ("南瓜小米粥", "早餐", 25, "orange",
     "小米煮开。\n加入南瓜块煮软。",
     [("小米", 45), ("南瓜", 120)]),
    ("馒头豆浆", "早餐", 5, "orange",
     "馒头加热。\n豆浆温热同食。",
     [("馒头", 100), ("豆浆", 250)]),
    ("香蕉燕麦奶", "早餐", 5, "orange",
     "燕麦与香蕉泥混合。\n冲入牛奶。",
     [("燕麦", 40), ("香蕉", 100), ("牛奶", 180)]),
    ("玉米鸡蛋饼", "早餐", 12, "orange",
     "鸡蛋打散加入玉米。\n平底锅摊饼。",
     [("鸡蛋", 100), ("玉米", 80), ("油", 5)]),
    ("紫薯燕麦碗", "早餐", 15, "orange",
     "紫薯蒸熟压泥。\n拌入燕麦与酸奶。",
     [("紫薯", 120), ("燕麦", 40), ("酸奶", 80)]),
    ("蒸蛋羹配牛奶", "早餐", 12, "orange",
     "鸡蛋加水搅匀蒸熟。\n配牛奶。",
     [("鸡蛋", 80), ("牛奶", 200), ("馒头", 60)]),
    ("鸡胸肉炒西兰花", "午餐", 20, "green",
     "鸡胸切片腌制。\n西兰花焯水。\n少油翻炒，配米饭。",
     [("鸡胸", 150), ("西兰花", 200), ("大米", 90), ("油", 8)]),
    ("番茄炒蛋盖饭", "午餐", 15, "green",
     "鸡蛋炒熟盛出。\n番茄炒出汁回锅鸡蛋。\n盖在米饭上。",
     [("鸡蛋", 100), ("番茄", 200), ("大米", 100), ("油", 10)]),
    ("青椒肉丝饭", "午餐", 18, "green",
     "猪肉切丝腌制。\n青椒切丝快炒。\n配米饭。",
     [("猪肉", 120), ("青椒", 150), ("大米", 90), ("油", 10)]),
    ("虾仁糙米饭", "午餐", 18, "green",
     "虾仁快炒。\n配糙米与黄瓜。",
     [("虾", 150), ("糙米", 90), ("黄瓜", 100), ("油", 8)]),
    ("牛肉洋葱饭", "午餐", 20, "green",
     "牛肉片腌制。\n洋葱切丝大火快炒。\n配米饭。",
     [("牛肉", 120), ("洋葱", 120), ("大米", 90), ("油", 10)]),
    ("麻婆豆腐饭", "午餐", 18, "green",
     "豆腐切块。\n少许肉末煸香焖煮。\n配米饭。",
     [("豆腐", 200), ("猪肉", 40), ("大米", 90), ("油", 8)]),
    ("土豆炖牛肉饭", "午餐", 40, "green",
     "牛肉焯水。\n与土豆胡萝卜同炖。\n配米饭。",
     [("牛肉", 150), ("土豆", 200), ("胡萝卜", 80), ("大米", 80)]),
    ("鱼香茄子饭", "午餐", 25, "green",
     "茄子切条略煎。\n加调味汁焖炒。\n配米饭。",
     [("茄子", 250), ("大米", 90), ("油", 12)]),
    ("宫保鸡丁饭", "午餐", 22, "green",
     "鸡丁腌制。\n青椒花生同炒。\n配米饭。",
     [("鸡", 140), ("青椒", 100), ("花生", 20), ("大米", 90), ("油", 10)]),
    ("芹菜炒肉丝饭", "午餐", 15, "green",
     "肉丝腌制。\n芹菜焯水同炒。\n配米饭。",
     [("猪肉", 100), ("芹菜", 150), ("大米", 90), ("油", 8)]),
    ("香菇青菜炒饭", "午餐", 15, "green",
     "香菇青菜翻炒。\n加入米饭与鸡蛋炒匀。",
     [("大米", 150), ("香菇", 50), ("小白菜", 80), ("鸡蛋", 50), ("油", 10)]),
    ("清蒸鲈鱼", "晚餐", 18, "blue",
     "鲈鱼铺姜片。\n大火蒸8—10分钟。\n配豆腐与番茄。",
     [("鲈鱼", 200), ("豆腐", 100), ("番茄", 120)]),
    ("蒜蓉西兰花配鸡胸", "晚餐", 15, "blue",
     "鸡胸煎熟切片。\n西兰花焯水拌蒜蓉。",
     [("鸡胸", 120), ("西兰花", 200), ("油", 6)]),
    ("蒸蛋配青菜", "晚餐", 15, "blue",
     "鸡蛋加水蒸熟。\n青菜焯水少油拌。",
     [("鸡蛋", 100), ("小白菜", 150), ("油", 5)]),
    ("豆腐海带汤饭", "晚餐", 20, "blue",
     "海带豆腐同煮成汤。\n配少量米饭。",
     [("豆腐", 150), ("海带", 50), ("大米", 60)]),
    ("清炒时蔬糙米饭", "晚餐", 12, "blue",
     "菠菜胡萝卜黄瓜少油快炒。\n配糙米饭。",
     [("菠菜", 100), ("胡萝卜", 80), ("黄瓜", 80), ("糙米", 70), ("油", 8)]),
    ("虾仁豆腐", "晚餐", 15, "blue",
     "豆腐切块。\n虾仁略炒后同焖。",
     [("虾", 120), ("豆腐", 180), ("油", 6)]),
    ("番茄牛肉汤馒头", "晚餐", 30, "blue",
     "牛肉焯水。\n番茄炖牛肉。\n配馒头。",
     [("牛肉", 100), ("番茄", 200), ("馒头", 60)]),
    ("杂粮饭煎鸡胸", "晚餐", 25, "blue",
     "鸡胸煎至金黄。\n配米饭玉米与生菜。",
     [("鸡胸", 130), ("大米", 50), ("玉米", 40), ("生菜", 80), ("油", 6)]),
    ("酸辣白菜豆腐", "晚餐", 12, "blue",
     "白菜切丝。\n豆腐煎黄同炒。",
     [("大白菜", 200), ("豆腐", 150), ("油", 8)]),
    ("冬瓜排骨汤饭", "晚餐", 45, "blue",
     "排骨焯水炖软。\n加冬瓜续炖。\n配米饭。",
     [("排骨", 120), ("冬瓜", 200), ("大米", 70)]),
    ("黄瓜鸡蛋汤配馒头", "晚餐", 12, "blue",
     "黄瓜切片。\n蛋花汤调味。\n配馒头。",
     [("黄瓜", 150), ("鸡蛋", 50), ("馒头", 80)]),
]


def main():
    conn = sqlite3.connect(DB)
    conn.execute("DELETE FROM recipe_foods")
    conn.execute("DELETE FROM favorites")
    conn.execute("DELETE FROM recipes")

    created = 0
    skipped = []
    for name, category, minutes, accent, steps, ingredients in RECIPES:
        matched = []
        missing = []
        for kw, qty in ingredients:
            food = find_food(conn, kw)
            if food:
                matched.append((food[0], qty, food))
            else:
                missing.append(kw)
        if missing:
            skipped.append((name, missing))
            continue
        # 合并同一食材用量（关键词可能命中同一条 foods 记录）
        merged = {}
        for fid, qty, food in matched:
            if fid in merged:
                merged[fid] = (merged[fid][0] + qty, food)
            else:
                merged[fid] = (qty, food)
        cur = conn.execute(
            "INSERT INTO recipes(name, category, steps, cook_minutes, accent) VALUES (?,?,?,?,?)",
            (name, category, steps, minutes, accent),
        )
        rid = cur.lastrowid
        tot_c = tot_p = tot_f = tot_carb = 0.0
        for fid, (qty, food) in merged.items():
            conn.execute(
                "INSERT INTO recipe_foods(recipe_id, food_id, quantity) VALUES (?,?,?)",
                (rid, fid, qty),
            )
            factor = qty / 100.0
            tot_c += (food[2] or 0) * factor
            tot_p += (food[3] or 0) * factor
            tot_f += (food[4] or 0) * factor
            tot_carb += (food[5] or 0) * factor
        conn.execute(
            "UPDATE recipes SET total_calories=?, total_protein=?, total_carbs=?, total_fat=? WHERE id=?",
            (round(tot_c, 1), round(tot_p, 1), round(tot_carb, 1), round(tot_f, 1), rid),
        )
        created += 1

    uid = conn.execute("SELECT id FROM users WHERE name='张明'").fetchone()
    if not uid:
        conn.execute(
            "INSERT INTO users(name,gender,goal,height,weight,calorie_target) VALUES ('张明','male','gain',175,70,2100)"
        )
        uid = conn.execute("SELECT id FROM users WHERE name='张明'").fetchone()
    sample = conn.execute("SELECT id FROM recipes WHERE name LIKE '%鸡胸%' LIMIT 1").fetchone()
    if sample and uid:
        conn.execute("INSERT OR IGNORE INTO favorites(user_id, recipe_id) VALUES (?,?)", (uid[0], sample[0]))

    conn.commit()
    print(f"created={created}, skipped={len(skipped)}")
    for s in skipped:
        print("SKIP", s[0], s[1])
    print("by meal:", list(conn.execute("SELECT category, COUNT(*) FROM recipes GROUP BY category")))
    conn.close()


if __name__ == "__main__":
    main()
