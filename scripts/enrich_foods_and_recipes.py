# -*- coding: utf-8 -*-
"""
合并本地 USDA FoodData JSON + API 补充常见食材到 diet.db，并播种约 100 道中式食谱。
API Key 仅用于本机数据导入脚本，勿提交到公开仓库对外暴露。
"""
from __future__ import annotations

import hashlib
import json
import re
import sqlite3
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DB = ROOT / "data" / "diet.db"
FOOD_DIR = ROOT / "FoodData"
FOUNDATION = FOOD_DIR / "FoodData_Central_foundation_food_json_2026-04-30.json"
SR_LEGACY = FOOD_DIR / "FoodData_Central_sr_legacy_food_json_2018-04.json"
USDA_API_KEY = "ceOYBgClBGuUaTnxVSLTjQvQNrTwgTuFtqrblwqL"
USDA_SEARCH = "https://api.nal.usda.gov/fdc/v1/foods/search"

# Nutrient IDs
N_ENERGY = {1008, 2047, 2048}  # kcal
N_PROTEIN = {1003}
N_FAT = {1004}
N_CARB = {1005}

COMMON_API_QUERIES = [
    "chicken breast raw",
    "broccoli raw",
    "oats",
    "egg whole raw",
    "salmon Atlantic",
    "brown rice cooked",
    "tomato raw",
    "tofu firm",
    "beef loin",
    "yogurt plain nonfat",
    "spinach raw",
    "carrot raw",
    "potato flesh and skin",
    "apple raw",
    "banana raw",
    "milk whole",
    "peanut oil",
    "shrimp raw",
    "pork loin",
    "onion raw",
]


def nutrient_map(food_nutrients: list) -> dict[str, float | None]:
    out = {"calories": None, "protein": None, "fat": None, "carbs": None}
    for item in food_nutrients or []:
        nut = item.get("nutrient") or {}
        nid = nut.get("id")
        amount = item.get("amount")
        if amount is None:
            amount = item.get("value")
        if amount is None:
            continue
        try:
            val = float(amount)
        except (TypeError, ValueError):
            continue
        if nid in N_ENERGY:
            out["calories"] = val
        elif nid in N_PROTEIN:
            out["protein"] = val
        elif nid in N_FAT:
            out["fat"] = val
        elif nid in N_CARB:
            out["carbs"] = val
    return out


def category_name(food: dict) -> str:
    cat = food.get("foodCategory")
    if isinstance(cat, dict):
        return (cat.get("description") or "").strip()
    if isinstance(cat, str):
        return cat.strip()
    return ""


def upsert_food(conn: sqlite3.Connection, *, source_id: int, name: str, category: str,
                calories, protein, fat, carbs, source: str) -> bool:
    if not name or calories is None:
        return False
    # Use negative source_id space for USDA to avoid clash with chinanutri positive ids
    # Or use large offset
    sid = int(source_id)
    if sid < 10_000_000:
        sid = 10_000_000 + sid
    conn.execute(
        """
        INSERT INTO foods(source_id, name, category_one, category_two, calories, protein, fat, carbs, unit, source)
        VALUES (?, ?, NULL, NULL, ?, ?, ?, ?, '100g', ?)
        ON CONFLICT(source_id) DO UPDATE SET
            name=excluded.name,
            calories=excluded.calories,
            protein=excluded.protein,
            fat=excluded.fat,
            carbs=excluded.carbs,
            unit=excluded.unit,
            source=excluded.source,
            updated_at=datetime('now','localtime')
        """,
        (sid, name[:120], calories, protein, fat, carbs, source),
    )
    # store category text into dietary_fiber field? Better add category_label via ash unused?
    # Use cholesterol TEXT field? Prefer: keep category in name prefix if empty.
    # We'll put category into ash as temporary text label for UI mapping - NO that's wrong.
    # Check if foods has category text - schema uses category_one int.
    # Store USDA category in water field temporarily? Bad.
    # Add column category_label if missing.
    return True


def ensure_category_column(conn: sqlite3.Connection) -> None:
    cols = {r[1] for r in conn.execute("PRAGMA table_info(foods)")}
    if "category_label" not in cols:
        conn.execute("ALTER TABLE foods ADD COLUMN category_label TEXT")


def upsert_food_full(conn, *, source_id, name, category_label, calories, protein, fat, carbs, source):
    if not name or calories is None:
        return False
    sid = int(source_id)
    if sid < 10_000_000:
        sid = 10_000_000 + sid
    conn.execute(
        """
        INSERT INTO foods(source_id, name, category_label, calories, protein, fat, carbs, unit, source)
        VALUES (?, ?, ?, ?, ?, ?, ?, '100g', ?)
        ON CONFLICT(source_id) DO UPDATE SET
            name=excluded.name,
            category_label=excluded.category_label,
            calories=excluded.calories,
            protein=excluded.protein,
            fat=excluded.fat,
            carbs=excluded.carbs,
            source=excluded.source,
            updated_at=datetime('now','localtime')
        """,
        (sid, name[:120], category_label or "", calories, protein, fat, carbs, source),
    )
    return True


def import_foundation(conn: sqlite3.Connection) -> int:
    with FOUNDATION.open(encoding="utf-8") as f:
        data = json.load(f)
    foods = data.get("FoundationFoods") or []
    n = 0
    for food in foods:
        if not isinstance(food, dict):
            continue
        nm = nutrient_map(food.get("foodNutrients") or [])
        name = (food.get("description") or "").strip()
        if not name:
            continue
        # Prefer Chinese-friendly common items: keep English description
        display = f"[USDA] {name}"
        ok = upsert_food_full(
            conn,
            source_id=food.get("fdcId") or food.get("ndbNumber") or hash(name) % 900000,
            name=display,
            category_label=category_name(food),
            calories=nm["calories"],
            protein=nm["protein"],
            fat=nm["fat"],
            carbs=nm["carbs"],
            source="usda_foundation",
        )
        if ok:
            n += 1
    conn.commit()
    return n


def import_sr_legacy_filtered(conn: sqlite3.Connection, limit: int = 800) -> int:
    """流式读取 SR Legacy，只保留常见关键词食物，避免全量 201MB 入库噪声。"""
    if not SR_LEGACY.exists():
        return 0
    keywords = [
        "chicken", "beef", "pork", "egg", "milk", "yogurt", "rice", "oat", "wheat",
        "broccoli", "spinach", "tomato", "potato", "carrot", "apple", "banana",
        "salmon", "shrimp", "tofu", "soy", "peanut", "oil", "onion", "garlic",
        "cabbage", "lettuce", "cucumber", "bean", "lentil", "cheese", "bread",
        "pasta", "corn", "pepper", "mushroom", "avocado", "walnut", "almond",
    ]
    # Incremental JSON parse via regex chunks is fragile; load with json if memory allows.
    print("Loading SR Legacy (may take ~30s)...")
    with SR_LEGACY.open(encoding="utf-8") as f:
        data = json.load(f)
    foods = data.get("SRLegacyFoods") or []
    n = 0
    for food in foods:
        if not isinstance(food, dict):
            continue
        name = (food.get("description") or "").strip()
        low = name.lower()
        if not any(k in low for k in keywords):
            continue
        # skip branded / heavily processed when possible
        if "pillsbury" in low or "kraft" in low:
            continue
        nm = nutrient_map(food.get("foodNutrients") or [])
        if nm["calories"] is None:
            continue
        ok = upsert_food_full(
            conn,
            source_id=food.get("fdcId") or food.get("ndbNumber") or (hash(name) % 800000),
            name=f"[USDA] {name}",
            category_label=category_name(food),
            calories=nm["calories"],
            protein=nm["protein"],
            fat=nm["fat"],
            carbs=nm["carbs"],
            source="usda_sr_legacy",
        )
        if ok:
            n += 1
        if n >= limit:
            break
    conn.commit()
    return n


def api_search(query: str, page_size: int = 5) -> list[dict]:
    params = urllib.parse.urlencode({
        "api_key": USDA_API_KEY,
        "query": query,
        "pageSize": page_size,
        "dataType": "Foundation,SR Legacy",
    })
    url = f"{USDA_SEARCH}?{params}"
    req = urllib.request.Request(url, headers={"User-Agent": "SmartDietImporter/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8")).get("foods") or []
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as e:
        print("API error", query, e)
        return []


def import_api_common(conn: sqlite3.Connection) -> int:
    n = 0
    for q in COMMON_API_QUERIES:
        foods = api_search(q)
        for food in foods:
            # API search returns flattened nutrients sometimes
            calories = protein = fat = carbs = None
            for nut in food.get("foodNutrients") or []:
                nid = nut.get("nutrientId") or (nut.get("nutrient") or {}).get("id")
                amount = nut.get("value") if nut.get("value") is not None else nut.get("amount")
                if amount is None:
                    continue
                try:
                    val = float(amount)
                except (TypeError, ValueError):
                    continue
                if nid in N_ENERGY or nut.get("nutrientName") == "Energy":
                    # API often returns Energy in kcal as nutrientId 1008
                    unit = (nut.get("unitName") or "").lower()
                    if "kj" in unit:
                        val = val / 4.184
                    calories = val
                elif nid in N_PROTEIN or "Protein" in str(nut.get("nutrientName", "")):
                    protein = val
                elif nid in N_FAT or "lipid" in str(nut.get("nutrientName", "")).lower() or nut.get("nutrientName") == "Total lipid (fat)":
                    fat = val
                elif nid in N_CARB or "Carbohydrate" in str(nut.get("nutrientName", "")):
                    carbs = val
            name = (food.get("description") or "").strip()
            if not name or calories is None:
                continue
            cat = food.get("foodCategory") or ""
            if isinstance(cat, dict):
                cat = cat.get("description") or ""
            ok = upsert_food_full(
                conn,
                source_id=food.get("fdcId") or hash(name) % 700000,
                name=f"[USDA] {name}",
                category_label=str(cat),
                calories=calories,
                protein=protein,
                fat=fat,
                carbs=carbs,
                source="usda_api",
            )
            if ok:
                n += 1
        time.sleep(0.25)
        print(f"  API '{q}' -> +items, total api upserts ~{n}")
    conn.commit()
    return n


# ---------- recipes ----------
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
    "面条": ["挂面", "面条", "切面"],
    "豆角": ["豇豆", "豆角", "四季豆"],
    "莲藕": ["藕", "莲藕"],
    "山药": ["山药"],
    "木耳": ["木耳"],
    "金针菇": ["金针菇"],
    "带鱼": ["带鱼"],
    "草鱼": ["草鱼"],
    "鲫鱼": ["鲫鱼"],
    "羊肉": ["羊肉"],
    "鸭肉": ["鸭肉", "鸭"],
    "梨": ["梨"],
    "苹果": ["苹果"],
    "草莓": ["草莓"],
    "芝麻": ["芝麻"],
    "蜂蜜": ["蜂蜜"],
}


def find_food(conn, key):
    for kw in ALIASES.get(key, [key]):
        rows = conn.execute(
            "SELECT id, name, calories, protein, fat, carbs FROM foods "
            "WHERE name LIKE ? AND calories IS NOT NULL AND name NOT LIKE '[USDA]%' "
            "ORDER BY length(name) ASC LIMIT 1",
            (f"%{kw}%",),
        ).fetchall()
        if rows:
            return rows[0]
    return None


def build_recipe_list():
    """约 100 道中式家常菜模板。"""
    breakfast = [
        ("燕麦酸奶碗", 10, "orange", "燕麦热水浸泡。\n拌入酸奶。\n撒核桃。", [("燕麦", 50), ("酸奶", 150), ("核桃", 15)]),
        ("全蛋番茄早餐", 10, "orange", "鸡蛋炒熟。\n番茄同炒。\n配馒头。", [("鸡蛋", 100), ("番茄", 120), ("馒头", 80), ("油", 5)]),
        ("牛奶燕麦", 5, "orange", "燕麦冲温牛奶搅拌。", [("燕麦", 45), ("牛奶", 220)]),
        ("豆腐青菜粥", 25, "orange", "大米煮粥。\n加豆腐菠菜。", [("大米", 40), ("豆腐", 80), ("菠菜", 50)]),
        ("南瓜小米粥", 25, "orange", "小米煮开。\n加南瓜煮软。", [("小米", 45), ("南瓜", 120)]),
        ("馒头豆浆", 5, "orange", "馒头加热。\n豆浆温热。", [("馒头", 100), ("豆浆", 250)]),
        ("香蕉燕麦奶", 5, "orange", "燕麦香蕉泥冲牛奶。", [("燕麦", 40), ("香蕉", 100), ("牛奶", 180)]),
        ("玉米鸡蛋饼", 12, "orange", "蛋液加玉米摊饼。", [("鸡蛋", 100), ("玉米", 80), ("油", 5)]),
        ("紫薯燕麦碗", 15, "orange", "紫薯压泥拌燕麦酸奶。", [("紫薯", 120), ("燕麦", 40), ("酸奶", 80)]),
        ("蒸蛋羹配牛奶", 12, "orange", "鸡蛋蒸熟配牛奶馒头。", [("鸡蛋", 80), ("牛奶", 200), ("馒头", 60)]),
        ("苹果燕麦酸奶", 8, "orange", "燕麦拌酸奶切苹果。", [("燕麦", 40), ("酸奶", 150), ("苹果", 100)]),
        ("山药小米粥", 30, "orange", "小米山药同煮成粥。", [("小米", 40), ("山药", 100)]),
        ("蜂蜜香蕉酸奶", 5, "orange", "酸奶香蕉加少许蜂蜜。", [("酸奶", 180), ("香蕉", 90), ("蜂蜜", 10)]),
        ("芝麻馒头豆浆", 5, "orange", "馒头配豆浆，撒芝麻。", [("馒头", 90), ("豆浆", 220), ("芝麻", 8)]),
        ("菠菜鸡蛋粥", 20, "orange", "米粥打入蛋花加菠菜。", [("大米", 40), ("鸡蛋", 50), ("菠菜", 60)]),
        ("胡萝卜玉米粥", 25, "orange", "大米玉米胡萝卜同煮。", [("大米", 35), ("玉米", 60), ("胡萝卜", 60)]),
        ("草莓酸奶麦片", 5, "orange", "麦片拌酸奶与草莓。", [("麦片", 40), ("酸奶", 150), ("草莓", 80)]),
        ("梨子小米粥", 25, "orange", "小米煮粥加梨块。", [("小米", 40), ("梨", 100)]),
        ("南瓜馒头配蛋", 12, "orange", "水煮蛋配馒头南瓜。", [("鸡蛋", 50), ("馒头", 80), ("南瓜", 100)]),
        ("牛奶玉米糊", 10, "orange", "玉米糊冲牛奶。", [("玉米", 70), ("牛奶", 200)]),
        ("莲藕排骨粥轻食", 35, "orange", "大米莲藕少量排骨煮粥。", [("大米", 35), ("莲藕", 80), ("排骨", 40)]),
        ("黄瓜鸡蛋卷早餐", 10, "orange", "蛋皮卷入黄瓜丝。", [("鸡蛋", 100), ("黄瓜", 80), ("油", 5)]),
        ("紫薯牛奶", 10, "orange", "紫薯泥冲温牛奶。", [("紫薯", 150), ("牛奶", 180)]),
        ("核桃燕麦奶", 5, "orange", "燕麦牛奶加核桃。", [("燕麦", 45), ("牛奶", 200), ("核桃", 15)]),
        ("豆腐蔬菜汤早餐", 15, "orange", "豆腐菠菜清汤配馒头。", [("豆腐", 100), ("菠菜", 80), ("馒头", 70)]),
        ("玉米饼配豆浆", 12, "orange", "玉米鸡蛋饼配豆浆。", [("玉米", 70), ("鸡蛋", 50), ("豆浆", 200), ("油", 5)]),
        ("苹果牛奶燕麦", 5, "orange", "燕麦牛奶加苹果丁。", [("燕麦", 40), ("牛奶", 180), ("苹果", 90)]),
        ("红薯小米粥", 25, "orange", "紫薯小米同煮。", [("紫薯", 100), ("小米", 40)]),
        ("水煮蛋全麦感早餐", 8, "orange", "水煮蛋配馒头黄瓜。", [("鸡蛋", 100), ("馒头", 70), ("黄瓜", 80)]),
        ("酸奶水果杯", 5, "orange", "酸奶加香蕉苹果。", [("酸奶", 180), ("香蕉", 60), ("苹果", 60)]),
        ("金针菇蛋花汤配馒头", 12, "orange", "金针菇蛋花汤配馒头。", [("金针菇", 80), ("鸡蛋", 50), ("馒头", 70)]),
        ("海带豆腐粥", 25, "orange", "大米豆腐海带煮粥。", [("大米", 35), ("豆腐", 80), ("海带", 30)]),
        ("芹菜鸡蛋饼", 12, "orange", "蛋液加芹菜末摊饼。", [("鸡蛋", 100), ("芹菜", 60), ("油", 5)]),
    ]

    lunch = [
        ("鸡胸肉炒西兰花", 20, "green", "鸡胸腌制。\n西兰花焯水翻炒配米饭。", [("鸡胸", 150), ("西兰花", 200), ("大米", 90), ("油", 8)]),
        ("番茄炒蛋盖饭", 15, "green", "番茄炒蛋盖米饭。", [("鸡蛋", 100), ("番茄", 200), ("大米", 100), ("油", 10)]),
        ("青椒肉丝饭", 18, "green", "青椒肉丝配米饭。", [("猪肉", 120), ("青椒", 150), ("大米", 90), ("油", 10)]),
        ("虾仁糙米饭", 18, "green", "虾仁快炒配糙米黄瓜。", [("虾", 150), ("糙米", 90), ("黄瓜", 100), ("油", 8)]),
        ("牛肉洋葱饭", 20, "green", "牛肉洋葱快炒配米饭。", [("牛肉", 120), ("洋葱", 120), ("大米", 90), ("油", 10)]),
        ("麻婆豆腐饭", 18, "green", "豆腐肉末焖煮配米饭。", [("豆腐", 200), ("猪肉", 40), ("大米", 90), ("油", 8)]),
        ("土豆炖牛肉饭", 40, "green", "牛肉土豆胡萝卜炖配米饭。", [("牛肉", 150), ("土豆", 200), ("胡萝卜", 80), ("大米", 80)]),
        ("鱼香茄子饭", 25, "green", "茄子焖炒配米饭。", [("茄子", 250), ("大米", 90), ("油", 12)]),
        ("宫保鸡丁饭", 22, "green", "鸡丁青椒花生炒配米饭。", [("鸡", 140), ("青椒", 100), ("花生", 20), ("大米", 90), ("油", 10)]),
        ("芹菜炒肉丝饭", 15, "green", "芹菜肉丝配米饭。", [("猪肉", 100), ("芹菜", 150), ("大米", 90), ("油", 8)]),
        ("香菇青菜炒饭", 15, "green", "香菇青菜鸡蛋炒饭。", [("大米", 150), ("香菇", 50), ("小白菜", 80), ("鸡蛋", 50), ("油", 10)]),
        ("豆角炒肉饭", 18, "green", "豆角猪肉炒配米饭。", [("豆角", 180), ("猪肉", 100), ("大米", 90), ("油", 10)]),
        ("木耳炒鸡蛋饭", 12, "green", "木耳鸡蛋炒配米饭。", [("木耳", 40), ("鸡蛋", 100), ("大米", 90), ("油", 8)]),
        ("莲藕炒肉片饭", 20, "green", "莲藕肉片炒配米饭。", [("莲藕", 150), ("猪肉", 100), ("大米", 90), ("油", 10)]),
        ("番茄牛肉面", 25, "green", "番茄牛肉配面条。", [("牛肉", 100), ("番茄", 150), ("面条", 100), ("油", 8)]),
        ("茄子肉末饭", 20, "green", "茄子肉末焖配米饭。", [("茄子", 200), ("猪肉", 80), ("大米", 90), ("油", 10)]),
        ("青椒土豆丝饭", 15, "green", "青椒土豆丝配米饭。", [("青椒", 100), ("土豆", 180), ("大米", 90), ("油", 10)]),
        ("虾仁西兰花饭", 15, "green", "虾仁西兰花配米饭。", [("虾", 120), ("西兰花", 180), ("大米", 90), ("油", 8)]),
        ("香菇鸡丁饭", 18, "green", "香菇鸡丁配米饭。", [("鸡", 130), ("香菇", 80), ("大米", 90), ("油", 8)]),
        ("胡萝卜炒肉饭", 15, "green", "胡萝卜肉丝配米饭。", [("胡萝卜", 120), ("猪肉", 100), ("大米", 90), ("油", 8)]),
        ("金针菇肥牛风味饭", 15, "green", "金针菇牛肉炒配米饭。", [("牛肉", 110), ("金针菇", 100), ("大米", 90), ("油", 8)]),
        ("洋葱炒蛋饭", 12, "green", "洋葱炒蛋配米饭。", [("洋葱", 120), ("鸡蛋", 100), ("大米", 90), ("油", 8)]),
        ("冬瓜炒肉饭", 18, "green", "冬瓜猪肉炒配米饭。", [("冬瓜", 200), ("猪肉", 90), ("大米", 90), ("油", 8)]),
        ("海带豆腐盖饭", 15, "green", "海带豆腐烩配米饭。", [("海带", 50), ("豆腐", 160), ("大米", 90), ("油", 6)]),
        ("菠菜鸡蛋盖饭", 12, "green", "菠菜炒蛋配米饭。", [("菠菜", 150), ("鸡蛋", 80), ("大米", 90), ("油", 8)]),
        ("黄瓜炒肉片饭", 15, "green", "黄瓜肉片配米饭。", [("黄瓜", 150), ("猪肉", 100), ("大米", 90), ("油", 8)]),
        ("山药炒木耳饭", 15, "green", "山药木耳炒配米饭。", [("山药", 150), ("木耳", 30), ("大米", 90), ("油", 8)]),
        ("玉米排骨饭", 40, "green", "玉米排骨炖配米饭。", [("玉米", 120), ("排骨", 120), ("大米", 80)]),
        ("草鱼豆腐饭", 25, "green", "草鱼豆腐炖配米饭。", [("草鱼", 160), ("豆腐", 120), ("大米", 90), ("油", 6)]),
        ("带鱼米饭套餐", 20, "green", "带鱼煎配米饭青菜。", [("带鱼", 150), ("大米", 90), ("小白菜", 100), ("油", 10)]),
        ("羊肉胡萝卜饭", 30, "green", "羊肉胡萝卜炖配米饭。", [("羊肉", 130), ("胡萝卜", 100), ("大米", 90), ("油", 8)]),
        ("鸭肉白菜饭", 25, "green", "鸭肉大白菜炒配米饭。", [("鸭肉", 130), ("大白菜", 180), ("大米", 90), ("油", 8)]),
        ("鲫鱼豆腐汤饭", 30, "green", "鲫鱼豆腐汤配米饭。", [("鲫鱼", 180), ("豆腐", 100), ("大米", 80)]),
        ("西兰花牛肉饭", 18, "green", "西兰花牛肉炒配米饭。", [("西兰花", 180), ("牛肉", 120), ("大米", 90), ("油", 8)]),
        ("番茄鸡蛋面", 15, "green", "番茄鸡蛋卤面。", [("番茄", 160), ("鸡蛋", 80), ("面条", 110), ("油", 8)]),
    ]

    dinner = [
        ("清蒸鲈鱼", 18, "blue", "鲈鱼蒸熟配豆腐番茄。", [("鲈鱼", 200), ("豆腐", 100), ("番茄", 120)]),
        ("蒜蓉西兰花配鸡胸", 15, "blue", "鸡胸切片配西兰花。", [("鸡胸", 120), ("西兰花", 200), ("油", 6)]),
        ("蒸蛋配青菜", 15, "blue", "蒸蛋配焯青菜。", [("鸡蛋", 100), ("小白菜", 150), ("油", 5)]),
        ("豆腐海带汤饭", 20, "blue", "海带豆腐汤配米饭。", [("豆腐", 150), ("海带", 50), ("大米", 60)]),
        ("清炒时蔬糙米饭", 12, "blue", "时蔬快炒配糙米。", [("菠菜", 100), ("胡萝卜", 80), ("黄瓜", 80), ("糙米", 70), ("油", 8)]),
        ("虾仁豆腐", 15, "blue", "虾仁豆腐同焖。", [("虾", 120), ("豆腐", 180), ("油", 6)]),
        ("番茄牛肉汤馒头", 30, "blue", "番茄牛肉汤配馒头。", [("牛肉", 100), ("番茄", 200), ("馒头", 60)]),
        ("杂粮饭煎鸡胸", 25, "blue", "煎鸡胸配米饭玉米生菜。", [("鸡胸", 130), ("大米", 50), ("玉米", 40), ("生菜", 80), ("油", 6)]),
        ("酸辣白菜豆腐", 12, "blue", "白菜豆腐同炒。", [("大白菜", 200), ("豆腐", 150), ("油", 8)]),
        ("冬瓜排骨汤饭", 45, "blue", "冬瓜排骨汤配米饭。", [("排骨", 120), ("冬瓜", 200), ("大米", 70)]),
        ("黄瓜鸡蛋汤配馒头", 12, "blue", "黄瓜蛋花汤配馒头。", [("黄瓜", 150), ("鸡蛋", 50), ("馒头", 80)]),
        ("清蒸草鱼配青菜", 20, "blue", "草鱼清蒸配小白菜。", [("草鱼", 200), ("小白菜", 150)]),
        ("香菇青菜汤", 15, "blue", "香菇青菜豆腐汤。", [("香菇", 60), ("小白菜", 120), ("豆腐", 100)]),
        ("凉拌黄瓜鸡丝", 15, "blue", "鸡丝凉拌黄瓜。", [("鸡胸", 120), ("黄瓜", 150)]),
        ("菠菜豆腐汤", 12, "blue", "菠菜豆腐清汤。", [("菠菜", 120), ("豆腐", 150)]),
        ("芹菜牛肉轻食", 18, "blue", "芹菜牛肉少油快炒。", [("芹菜", 150), ("牛肉", 100), ("油", 6)]),
        ("金针菇蒸鸡", 25, "blue", "金针菇与鸡同蒸。", [("金针菇", 100), ("鸡", 150)]),
        ("山药排骨汤", 40, "blue", "山药排骨清炖。", [("山药", 150), ("排骨", 100)]),
        ("木耳黄瓜凉拌", 10, "blue", "木耳黄瓜凉拌。", [("木耳", 40), ("黄瓜", 150)]),
        ("莲藕排骨汤", 45, "blue", "莲藕排骨汤。", [("莲藕", 150), ("排骨", 100)]),
        ("番茄虾仁", 15, "blue", "番茄虾仁快炒。", [("番茄", 150), ("虾", 120), ("油", 6)]),
        ("茄子蒸蛋", 20, "blue", "茄子蒸蛋。", [("茄子", 180), ("鸡蛋", 80)]),
        ("清炒豆角", 12, "blue", "豆角少油清炒。", [("豆角", 200), ("油", 6)]),
        ("海带鸡蛋汤", 12, "blue", "海带蛋花汤。", [("海带", 40), ("鸡蛋", 50)]),
        ("鲫鱼萝卜汤", 35, "blue", "鲫鱼胡萝卜汤。", [("鲫鱼", 180), ("胡萝卜", 100)]),
        ("带鱼蒸蒜蓉青菜", 20, "blue", "带鱼蒸配青菜。", [("带鱼", 150), ("小白菜", 120)]),
        ("羊肉白菜汤", 30, "blue", "羊肉白菜清汤。", [("羊肉", 100), ("大白菜", 180)]),
        ("鸭肉冬瓜汤", 35, "blue", "鸭肉冬瓜汤。", [("鸭肉", 120), ("冬瓜", 200)]),
        ("西兰花蒸蛋", 15, "blue", "西兰花配蒸蛋。", [("西兰花", 150), ("鸡蛋", 80)]),
        ("生菜包鸡丁", 15, "blue", "鸡丁配生菜叶。", [("鸡", 120), ("生菜", 100), ("油", 5)]),
        ("土豆牛肉轻汤", 35, "blue", "土豆牛肉少油炖汤。", [("土豆", 150), ("牛肉", 100)]),
        ("洋葱番茄蛋汤", 12, "blue", "洋葱番茄蛋汤。", [("洋葱", 80), ("番茄", 120), ("鸡蛋", 50)]),
        ("玉米排骨轻汤", 40, "blue", "玉米排骨清汤。", [("玉米", 120), ("排骨", 90)]),
    ]

    recipes = []
    for name, mins, accent, steps, ings in breakfast:
        recipes.append((name, "早餐", mins, accent, steps, ings))
    for name, mins, accent, steps, ings in lunch:
        recipes.append((name, "午餐", mins, accent, steps, ings))
    for name, mins, accent, steps, ings in dinner:
        recipes.append((name, "晚餐", mins, accent, steps, ings))
    return recipes


def seed_recipes(conn: sqlite3.Connection) -> tuple[int, int]:
    conn.execute("DELETE FROM recipe_foods")
    conn.execute("DELETE FROM favorites")
    conn.execute("DELETE FROM recipes")
    created = skipped = 0
    for name, category, minutes, accent, steps, ingredients in build_recipe_list():
        matched = []
        missing = []
        for kw, qty in ingredients:
            food = find_food(conn, kw)
            if food:
                matched.append((food[0], qty, food))
            else:
                missing.append(kw)
        if missing:
            skipped += 1
            print("SKIP", name, missing)
            continue
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
    conn.commit()
    return created, skipped


def ensure_demo_user(conn: sqlite3.Connection) -> None:
    """演示账号：张明 / 123456（SHA256 盐值哈希，与客户端一致）。"""
    salt = "smartdiet"
    pwd_hash = hashlib.sha256((salt + "123456").encode("utf-8")).hexdigest()
    stored = f"{salt}:{pwd_hash}"
    cols = {r[1] for r in conn.execute("PRAGMA table_info(users)")}
    if "password_hash" not in cols:
        conn.execute("ALTER TABLE users ADD COLUMN password_hash TEXT")
    row = conn.execute("SELECT id FROM users WHERE name='张明'").fetchone()
    if row:
        conn.execute("UPDATE users SET password_hash=? WHERE id=?", (stored, row[0]))
    else:
        conn.execute(
            "INSERT INTO users(name,gender,goal,height,weight,calorie_target,password_hash) "
            "VALUES ('张明','male','gain',175,70,2100,?)",
            (stored,),
        )
    conn.commit()


def main():
    conn = sqlite3.connect(DB)
    ensure_category_column(conn)
    ensure_demo_user(conn)

    print("Import foundation foods...")
    n1 = import_foundation(conn)
    print(f"  foundation upserted: {n1}")

    print("Import SR Legacy filtered...")
    n2 = import_sr_legacy_filtered(conn, limit=800)
    print(f"  sr_legacy upserted: {n2}")

    print("Import API common foods...")
    n3 = import_api_common(conn)
    print(f"  api upserted: {n3}")

    print("Seed recipes...")
    created, skipped = seed_recipes(conn)
    print(f"recipes created={created}, skipped={skipped}")
    print("foods total", conn.execute("SELECT COUNT(*) FROM foods").fetchone()[0])
    print("by meal", list(conn.execute("SELECT category, COUNT(*) FROM recipes GROUP BY category")))
    conn.close()
    print("Done:", DB)


if __name__ == "__main__":
    main()
