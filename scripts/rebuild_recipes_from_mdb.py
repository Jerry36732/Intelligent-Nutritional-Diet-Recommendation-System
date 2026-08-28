# -*- coding: utf-8 -*-
"""从结构化 MDB 重建经过份量、映射和营养硬校验的食谱库。"""
from __future__ import annotations

import json
import hashlib
import math
import re
import shutil
import sqlite3
from collections import defaultdict
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DB = ROOT / "data" / "diet.db"
RAW = ROOT / "食谱数据" / "mdb_recipes_raw.json"
BACKUP = ROOT / "食谱数据" / "diet_before_mdb_rebuild.db"

ROLE_LIMITS = {
    "breakfast": (180.0, 380.0),
    "staple": (120.0, 180.0),
    "meat": (180.0, 280.0),
    "vegetable": (180.0, 260.0),
    "soup": (300.0, 450.0),
    "drink": (180.0, 260.0),
    "fruit": (120.0, 220.0),
    "mixed": (200.0, 320.0),
}

EXCLUDE_CANDIDATE = ("软糖", "奶糖", "糖果", "婴儿", "米粉", "调味汁", "罐头")
MEAT_WORDS = ("猪", "牛", "羊", "鸡", "鸭", "鹅", "鱼", "虾", "蟹", "肉", "排骨", "肝", "蛋", "火腿")
STAPLE_WORDS = ("米饭", "馒头", "花卷", "包子", "饺", "面条", "面饭", "饼", "粥", "糕", "窝头")
SOUP_WORDS = ("汤", "羹")
DRINK_WORDS = ("汁", "牛奶", "牛乳", "酸奶", "豆浆", "饮料")
AROMATICS = ("葱", "姜", "蒜", "香菜", "味精", "鸡精", "盐", "胡椒", "花椒", "大料", "八角", "桂皮", "料酒", "淀粉")
MICRO_INGREDIENT_MAX_G = 12.0

ALIASES = {
    "鸡蛋": "蛋（鸡蛋，均值)", "全蛋": "蛋（鸡蛋，均值)", "蛋清": "鸡蛋白",
    "蛋白": "鸡蛋白", "蛋黄": "蛋黄（鸡蛋黄）", "酸奶": "乳品（酸奶，均值)",
    "牛奶": "乳品（牛乳，均值)", "牛乳": "乳品（牛乳，均值)", "豆腐": "豆腐(均值)",
    "南豆腐": "豆腐(南)[南豆腐]", "北豆腐": "豆腐(北)", "米饭": "米饭(蒸)(均值)",
    "白米饭": "米饭(蒸)(均值)", "番茄": "番茄[西红柿]", "西红柿": "番茄[西红柿]",
    "全麦面包": "面包(均值)",
    "偏口鱼": "鲆[片口鱼，比目鱼]", "片口鱼": "鲆[片口鱼，比目鱼]",
    "植物油": "植物油（通用）", "食用油": "植物油（通用）",
    "盐": "精盐", "食盐": "精盐", "葱": "大葱", "姜": "姜[黄姜](鲜)",
    "白糖": "糖（白砂糖）", "白砂糖": "糖（白砂糖）", "蔗糖": "糖（白砂糖）",
    "香油": "芝麻油[香油]", "芝麻油": "芝麻油[香油]",
    "土豆": "马铃薯[土豆，洋芋]", "马铃薯": "马铃薯[土豆，洋芋]",
    "红枣": "枣(干)", "枣": "枣(干)", "干辣椒": "辣椒(红，尖，干)",
    "胡椒末": "胡椒粉", "胡椒面": "胡椒粉", "白胡椒": "胡椒粉",
    "淀粉": "玉米淀粉", "干淀粉": "玉米淀粉", "面粉": "小麦粉(标准粉)",
    "海米": "虾米[海米，虾仁]", "鲩鱼": "草鱼[白鲩，草包鱼]",
    "水发木耳": "木耳(水发)[黑木耳，云耳]", "木耳": "木耳(水发)[黑木耳，云耳]",
    "水发玉兰片": "玉兰片", "凤梨": "菠萝[凤梨，地菠萝]",
    "鲜干贝": "扇贝(鲜)", "干贝": "扇贝(干)[干贝]",
    "猪肥肉": "猪肉(肥)", "瘦猪肉": "猪肉(瘦)", "猪瘦肉": "猪肉(瘦)",
    "猪爪": "猪蹄", "猪肘子": "猪蹄", "猪排骨": "猪小排",
    "鸡脯肉": "鸡胸脯肉", "鸭子": "鸭(均值)", "母鸡": "鸡(均值)",
    "清水": "饮用水", "温开水": "饮用水",
    "鲜奶油": "乳品（奶油）", "甜杏仁": "杏仁", "核桃": "核桃(干)[胡桃]",
    "大蒜": "大蒜[蒜头](鲜)", "芝麻": "芝麻籽(白)",
    "猪肉(肥瘦)": "猪肉(肥瘦)(均值)", "猪油": "猪油(炼)", "花生油": "花生油",
    "酒": "绍兴黄酒(15度)", "白酒": "汉口白酒(49.6度)", "醋": "醋(均值)",
    "蚌肉": "河蚌", "水发海参": "海参", "海参": "海参", "北粳米": "粳米(标一)",
    "大米粥": "粳米粥", "菜心": "白菜薹[菜薹，菜心]", "赤砂糖": "糖（红糖）",
    "水发香菇": "香菇[香蕈，冬菇](鲜)", "橘子汁": "橙汁饮料", "原汁酱油": "酱油(均值)",
    "熟笋": "玉兰片", "瘦肉丝": "猪肉(瘦)",
    "鸡腿": "鸡腿（官方）", "鸡翅": "鸡(均值)", "鸡块": "鸡(均值)",
    "鸡肉": "鸡(均值)", "仔鸡": "鸡(均值)", "公鸡": "鸡(均值)",
    "鱼肉": "鲤鱼", "鲜鱼": "鲤鱼", "净鱼": "鲤鱼",
}

CATEGORY_FALLBACKS = (
    (("鸡",), "鸡(均值)"),
    (("鸭",), "鸭(均值)"),
    (("鹅",), "鹅(均值)"),
    (("猪肉", "肉馅", "肉丝", "肉片"), "猪肉(肥瘦)(均值)"),
    (("牛肉",), "牛肉(均值)"),
    (("羊肉",), "羊肉(均值)"),
    (("鱼",), "鲤鱼"),
    (("虾",), "虾米[海米，虾仁]"),
    (("蟹",), "河蟹"),
    (("青菜", "蔬菜"), "小白菜"),
    (("食用油", "植物油", "色拉油"), "植物油（通用）"),
)

DISPLAY_ALIASES = {
    "蛋（鸡蛋，均值)": "鸡蛋", "乳品（酸奶，均值)": "酸奶", "乳品（牛乳，均值)": "牛奶",
    "豆腐(均值)": "豆腐", "豆腐(南)[南豆腐]": "豆腐", "豆腐(北)": "豆腐",
    "米饭(蒸)(均值)": "白米饭", "番茄[西红柿]": "番茄",
}


def plain_name(value: str) -> str:
    value = re.sub(r"[\[【（(].*?[\]】）)]", "", value or "")
    value = re.sub(r"(?:鲜|熟|净|去皮|去骨|水发|干制|均值|市售)$", "", value)
    return re.sub(r"[^\u4e00-\u9fffA-Za-z0-9]", "", value).strip()


def normalize_recipe_name(value: str) -> str:
    """移除菜单/保健描述前缀与年龄限定，只保留可展示的菜品本名。"""
    value = re.sub(r"的做法$", "", value or "").strip()
    value = re.sub(r"^(?:(?:减肥|减脂|增肌|养生|保健|营养)?菜谱\s*\d*|健康减重|健康减脂)[_－—–-]+\s*", "", value)
    # MDB 中连接号前均是病症、功效或菜单标记，连接号后才是菜名。
    value = re.sub(r"^[^－—–-]+[－—–-]+\s*", "", value).strip()
    # 仅删除明确表示食用月龄/年龄的括号，不碰菜名中的普通说明。
    value = re.sub(r"\s*[（(][^）)]*(?:个?月|月龄|周岁|岁)[^）)]*[）)]\s*$", "", value).strip()
    return value


def canonical_source(value: str) -> str:
    value = (value or "").replace("…", "").replace("/", "").strip()
    value = re.sub(r"^(?:主料|配料|材料|调料)\s*[:：]?\s*", "", value)
    value = re.sub(r"(?:切末|切丝|切片|少许|适量)$", "", value)
    value = re.sub(r"(?:克|千克|公斤|kg|斤|两|一只|半只|一只半|一个)$", "", value, flags=re.I).strip()
    replacements = {
        "鲜肥母鸡": "母鸡", "肥嫩母鸡": "母鸡", "嫩母鸡": "母鸡", "开膛嫩仔公鸡": "母鸡",
        "净填鸭": "鸭子", "老鸭": "鸭子", "鲜活虾": "虾", "大虾": "虾",
        "鲜干贝粒": "鲜干贝", "桃仁": "核桃", "蒜泥": "大蒜", "蒜瓣": "大蒜",
        "猪五花肉": "猪肉(肥瘦)", "猪肥瘦肉": "猪肉(肥瘦)", "熟白芝麻": "芝麻",
        "熟花生油": "花生油", "化猪油": "猪油", "熟猪油": "猪油", "油": "植物油",
        "绍酒": "料酒", "白酒": "酒", "烧酒": "酒", "米醋": "醋", "醋精": "醋",
    }
    return replacements.get(value, value)


def display_name(food_name: str, source_name: str = "") -> str:
    if food_name in DISPLAY_ALIASES:
        return DISPLAY_ALIASES[food_name]
    source = plain_name(source_name)
    if source:
        source = source.replace("鲜", "").replace("熟", "")
        if len(source) >= 1:
            return source
    return plain_name(food_name) or food_name


def normalize_steps(text: str) -> str:
    text = (text or "").replace("\r", "\n").strip()
    text = re.sub(r"\s*\((\d+)\)\s*", r"\n\1. ", text)
    text = re.sub(r"\s*（(\d+)）\s*", r"\n\1. ", text)
    lines = [re.sub(r"^\d+[.、]\s*", "", x.strip()) for x in text.splitlines() if x.strip()]
    return "\n".join(f"{i}. {line}" for i, line in enumerate(lines, 1))


def build_type_paths(types: list[dict]) -> dict[int, str]:
    by_id = {int(x["节点ID"]): x for x in types}
    result = {}
    for node_id in by_id:
        names, cursor, seen = [], node_id, set()
        while cursor in by_id and cursor not in seen:
            seen.add(cursor)
            node = by_id[cursor]
            names.append(str(node.get("类型名称") or ""))
            cursor = int(node.get("父节点") or 0)
        result[node_id] = "/".join(reversed([x for x in names if x]))
    return result


def extract_ingredients(raw: str) -> list[dict]:
    """逐项解析 MDB 原料；支持“盐、花椒、味精各5克”和“实耗约500克”。"""
    items: list[dict] = []
    pending: list[str] = []

    def convert(value: str, unit: str) -> float:
        qty = float(value)
        unit = unit.lower()
        if unit in ("千克", "公斤", "kg"):
            return qty * 1000.0
        if unit == "斤":
            return qty * 500.0
        if unit == "两":
            return qty * 50.0
        return qty

    def names(value: str) -> list[str]:
        value = re.sub(r"[（(].*$", "", value).strip(" ：:。.")
        value = re.sub(r"(?:约|各)$", "", value).strip()
        value = re.sub(r"\d+(?:\.\d+)?\s*(?:个|只|枚|片|块|根|条|朵|瓣|张|碗)?", "", value).strip()
        if value in ("葱姜", "葱、姜"):
            return ["葱", "姜"]
        return [value] if value else []

    for token in re.split(r"[,，;；、。\n]+", raw or ""):
        token = token.strip()
        if not token:
            continue
        egg = re.search(r"((?:鸡蛋|全蛋))\s*(\d+(?:\.\d+)?)\s*(?:个|只|枚)", token)
        if egg and not re.search(r"\d+(?:\.\d+)?\s*(?:克|g|千克|公斤|kg|斤|两)", token, re.I):
            items.append({"source": "鸡蛋", "quantity": float(egg.group(2)) * 50.0})
            continue
        actual = re.search(r"实耗(?:约)?\s*(\d+(?:\.\d+)?)\s*(克|g|千克|公斤|kg|斤|两)", token, re.I)
        matches = list(re.finditer(r"(\d+(?:\.\d+)?)\s*(克|g|千克|公斤|kg|斤|两)", token, re.I))
        match = actual or (matches[-1] if matches else None)
        if match is None:
            pending.extend(names(token))
            continue
        qty = convert(match.group(1), match.group(2))
        prefix = token[:match.start()].strip()
        current = names(prefix)
        shared = "各" in prefix
        targets = (pending + current) if shared else current
        pending.clear()
        for source in targets:
            if 0.2 <= qty <= 10000:
                items.append({"source": source, "quantity": qty})
    return items


class FoodMatcher:
    def __init__(self, conn: sqlite3.Connection):
        self.rows = [dict(r) for r in conn.execute(
            "SELECT id,name,COALESCE(calories,0) calories,COALESCE(protein,0) protein,"
            "COALESCE(fat,0) fat,COALESCE(carbs,0) carbs FROM foods WHERE calories IS NOT NULL"
        )]
        self.by_name = {r["name"]: r for r in self.rows}

    def match(self, source: str) -> dict | None:
        source = canonical_source(source)
        compact = plain_name(source)
        for key, target in sorted(ALIASES.items(), key=lambda x: len(x[0]), reverse=True):
            if key in compact and target in self.by_name:
                return self.by_name[target]
        best, best_score = None, 0
        for row in self.rows:
            name = row["name"]
            simple = plain_name(name)
            if not simple:
                continue
            if any(x in name for x in EXCLUDE_CANDIDATE) and not any(x in compact for x in EXCLUDE_CANDIDATE):
                continue
            score = 0
            if compact == simple:
                score = 100
            elif len(compact) >= 2 and simple.startswith(compact):
                score = 82 + min(8, len(compact))
            elif len(simple) >= 2 and compact.startswith(simple):
                score = 76 + min(8, len(simple))
            elif len(compact) >= 2 and compact in simple:
                score = 66 + min(10, len(compact))
            elif len(simple) >= 3 and simple in compact:
                score = 62 + min(10, len(simple))
            if score > best_score:
                best, best_score = row, score
        if best_score >= 70:
            return best
        # 精确检索/别名均失败后，按用户允许的同类食材回退；保留原食材显示名以便追溯。
        for keywords, target in CATEGORY_FALLBACKS:
            if any(word in compact for word in keywords) and target in self.by_name:
                return self.by_name[target]
        return None


def classify(name: str, type_path: str, ingredients: list[dict]) -> str:
    blob = name + type_path
    if any(x in name for x in DRINK_WORDS) or "饮料" in type_path:
        return "drink"
    if any(x in name for x in SOUP_WORDS) or "汤类" in type_path:
        return "soup"
    if any(x in name for x in STAPLE_WORDS) or "家常主食" in type_path:
        return "staple"
    ing_blob = "".join(x["source"] for x in ingredients)
    if any(x in ing_blob for x in MEAT_WORDS):
        return "meat"
    if "素菜" in type_path or not any(x in blob + ing_blob for x in MEAT_WORDS):
        return "vegetable"
    return "mixed"


def category_for(role: str, index: int) -> str:
    if role in ("breakfast", "drink", "fruit", "staple"):
        return "早餐"
    return "午餐" if index % 2 == 0 else "晚餐"


def add_water_if_needed(conn: sqlite3.Connection, mapped: list[dict], role: str) -> None:
    if role not in ("soup", "drink"):
        return
    water = conn.execute("SELECT * FROM foods WHERE name='饮用水' LIMIT 1").fetchone()
    if water is None:
        cur = conn.execute(
            "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source) "
            "VALUES(-900001,'饮用水',0,0,0,0,'100g','标准化补充')"
        )
        water = conn.execute("SELECT * FROM foods WHERE id=?", (cur.lastrowid,)).fetchone()
    current = sum(x["quantity"] for x in mapped)
    target = 350.0 if role == "soup" else 220.0
    if target - current >= 0.2:
        mapped.append({"food": dict(water), "source": "饮用水", "display": "饮用水", "quantity": target-current})


def ignored_micro_food(conn: sqlite3.Connection, source: str) -> dict:
    canonical = canonical_source(source) or source.strip()
    name = f"微量调料：{canonical}"
    row = conn.execute("SELECT * FROM foods WHERE name=?", (name,)).fetchone()
    if row:
        return dict(row)
    digest = int(hashlib.sha1(("micro:" + canonical).encode("utf-8")).hexdigest()[:7], 16)
    source_id = -20000000 - digest
    while conn.execute("SELECT 1 FROM foods WHERE source_id=?", (source_id,)).fetchone():
        source_id -= 1
    cur = conn.execute(
        "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source) "
        "VALUES(?,?,0,0,0,0,'100g','微量香料/调味料：按规则忽略营养贡献')",
        (source_id, name),
    )
    return dict(conn.execute("SELECT * FROM foods WHERE id=?", (cur.lastrowid,)).fetchone())


def nutrition(mapped: list[dict]) -> dict:
    total_weight = sum(x["quantity"] for x in mapped)
    totals = {"calories": 0.0, "protein": 0.0, "fat": 0.0, "carbs": 0.0}
    for item in mapped:
        if item.get("ignore_nutrition"):
            continue
        factor = item["quantity"] / 100.0
        for key in totals:
            totals[key] += float(item["food"].get(key) or 0) * factor
    per100 = {key: value * 100.0 / total_weight for key, value in totals.items()}
    return {"weight": total_weight, "totals": totals, "per100": per100}


def merge_same_food(mapped: list[dict]) -> list[dict]:
    merged: dict[int, dict] = {}
    for item in mapped:
        food_id = int(item["food"]["id"])
        if food_id not in merged:
            merged[food_id] = dict(item)
        else:
            merged[food_id]["quantity"] += item["quantity"]
            if item["source"] not in merged[food_id]["source"]:
                merged[food_id]["source"] += "、" + item["source"]
    return list(merged.values())


def valid_nutrition(role: str, values: dict) -> bool:
    t, p = values["totals"], values["per100"]
    if not (50 <= values["weight"] <= 650):
        return False
    if not (5 <= t["calories"] <= 1000 and t["protein"] <= 90 and t["fat"] <= 80):
        return False
    if not (0 <= p["calories"] <= 650 and p["protein"] <= 45 and p["fat"] <= 65):
        return False
    if role == "drink" and t["calories"] > 350:
        return False
    return True


def scale_to_serving(mapped: list[dict], role: str) -> None:
    weight = sum(x["quantity"] for x in mapped)
    lo, hi = ROLE_LIMITS.get(role, ROLE_LIMITS["mixed"])
    target = (lo + hi) / 2.0
    if weight > hi or weight < lo * 0.55:
        factor = target / weight
        # 只缩放主食材；盐、鸡精、八角等微量调料保持原始克数，避免被放大。
        for item in mapped:
            if not item.get("ignore_nutrition"):
                item["quantity"] *= factor


def mark_micro_ingredients(mapped: list[dict]) -> None:
    for item in mapped:
        source = canonical_source(item.get("source", ""))
        item["ignore_nutrition"] = bool(
            item["quantity"] <= MICRO_INGREDIENT_MAX_G
            and any(word in source for word in AROMATICS)
        )


def ensure_columns(conn: sqlite3.Connection) -> None:
    recipe_cols = {x[1] for x in conn.execute("PRAGMA table_info(recipes)")}
    additions = {
        "total_weight": "REAL", "per100_calories": "REAL", "per100_protein": "REAL",
        "per100_fat": "REAL", "per100_carbs": "REAL", "source_ref": "TEXT",
    }
    for name, sql_type in additions.items():
        if name not in recipe_cols:
            conn.execute(f"ALTER TABLE recipes ADD COLUMN {name} {sql_type}")
    rf_cols = {x[1] for x in conn.execute("PRAGMA table_info(recipe_foods)")}
    for name in ("display_name", "source_text"):
        if name not in rf_cols:
            conn.execute(f"ALTER TABLE recipe_foods ADD COLUMN {name} TEXT")


def ensure_reference_foods(conn: sqlite3.Connection) -> None:
    """补充原料名称本身为泛称、但官方同类均值明确的基础食材。"""
    if not conn.execute("SELECT 1 FROM foods WHERE name='植物油（通用）'").fetchone():
        conn.execute(
            "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source) "
            "VALUES(-900002,'植物油（通用）',882.89,0,99.8,0.2,'100g',"
            "'中国食物成分表植物油同类均值；nlc.chinanutri.cn/fq/foodinfo/1509.html')"
        )
    if not conn.execute("SELECT 1 FROM foods WHERE name='鸡腿（官方）'").fetchone():
        conn.execute(
            "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source) "
            "VALUES(-900003,'鸡腿（官方）',180.0,16.0,13.0,0.0,'100g',"
            "'中国食物成分表；nlc.chinanutri.cn/fq/foodinfo/882.html')"
        )


def insert_recipe(conn: sqlite3.Connection, recipe: dict) -> int:
    n = recipe["nutrition"]
    cur = conn.execute(
        "INSERT INTO recipes(name,category,steps,cook_minutes,accent,total_calories,total_protein,"
        "total_carbs,total_fat,dish_role,source,source_url,nutrition_verified_at,total_weight,"
        "per100_calories,per100_protein,per100_fat,per100_carbs,source_ref) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        (recipe["name"], recipe["category"], recipe["steps"], recipe["minutes"], recipe["accent"],
         round(n["totals"]["calories"],1), round(n["totals"]["protein"],1),
         round(n["totals"]["carbs"],1), round(n["totals"]["fat"],1), recipe["role"],
         recipe["source"], recipe.get("source_url", ""), datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
         round(n["weight"],1), round(n["per100"]["calories"],1), round(n["per100"]["protein"],1),
         round(n["per100"]["fat"],1), round(n["per100"]["carbs"],1), recipe["source_ref"]),
    )
    rid = cur.lastrowid
    for item in recipe["ingredients"]:
        conn.execute(
            "INSERT INTO recipe_foods(recipe_id,food_id,quantity,display_name,source_text) VALUES(?,?,?,?,?)",
            (rid, item["food"]["id"], round(item["quantity"],1), item["display"],
             item["source"] + ("【营养忽略】" if item.get("ignore_nutrition") else "")),
        )
    return rid


def seed_curated(conn: sqlite3.Connection, matcher: FoodMatcher) -> list[dict]:
    seeds = [
        ("白米饭", "staple", [("白米饭", 150, "白米饭")], "1. 大米淘洗后按米水约1:1.2浸泡15分钟。\n2. 使用电饭煲标准煮饭程序煮熟。\n3. 程序结束后焖8分钟，翻松并按150g盛出。"),
        ("全蛋番茄早餐", "breakfast", [("鸡蛋", 100, "鸡蛋（2个）"), ("番茄", 120, "番茄"), ("全麦面包", 80, "全麦面包")], "1. 鸡蛋2个煮熟或少油煎熟。\n2. 番茄洗净切片，全麦面包加热。\n3. 将鸡蛋、番茄和面包装盘，趁热食用。"),
        ("青菜豆腐汤", "soup", [("豆腐", 100, "豆腐"), ("小白菜", 80, "青菜"), ("饮用水", 250, "饮用水")], "1. 青菜洗净切段，豆腐切成小块。\n2. 锅中加入饮用水烧开，放入豆腐煮3分钟。\n3. 加入青菜再煮2分钟，少量盐调味后盛出。"),
        ("牛奶", "drink", [("牛奶", 220, "牛奶")], "1. 取220g牛奶。\n2. 可直接饮用；需要温热时隔水或小火加热，避免沸腾。"),
        ("原味酸奶", "drink", [("酸奶", 150, "酸奶")], "1. 取150g原味酸奶。\n2. 保持冷藏，开封后直接食用。"),
        ("苹果", "fruit", [("苹果", 150, "苹果")], "1. 苹果洗净。\n2. 去核后切块，按150g食用。"),
        ("梨", "fruit", [("梨", 150, "梨")], "1. 梨洗净。\n2. 去核后切块，按150g食用。"),
        ("橙子", "fruit", [("橙", 150, "橙子")], "1. 橙子洗净并剥皮。\n2. 去籽后按150g食用。"),
    ]
    result = []
    for name, role, ingredients, steps in seeds:
        mapped = []
        for source, qty, display in ingredients:
            food = matcher.match(source)
            if food is None and source == "饮用水":
                add_water_if_needed(conn, mapped, "drink")
                food = matcher.match("饮用水") or dict(conn.execute("SELECT * FROM foods WHERE name='饮用水'").fetchone())
            if food is None:
                break
            mapped.append({"food": food, "source": source, "display": display, "quantity": float(qty)})
        if len(mapped) != len(ingredients):
            continue
        n = nutrition(mapped)
        result.append({"name": name, "role": role, "category": "早餐" if role != "soup" else "晚餐",
                       "steps": steps, "minutes": 10 if role != "staple" else 30, "accent": "green",
                       "source": "人工标准化基础食谱", "source_ref": "curated", "ingredients": mapped,
                       "nutrition": n})
    return result


def main() -> None:
    payload = json.loads(RAW.read_text(encoding="utf-8-sig"))
    paths = build_type_paths(payload["types"])
    if DB.exists():
        shutil.copy2(DB, BACKUP)
    conn = sqlite3.connect(DB)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys=ON")
    ensure_columns(conn)
    ensure_reference_foods(conn)
    matcher = FoodMatcher(conn)
    candidates, rejects = [], defaultdict(int)
    names = set()
    for idx, raw in enumerate(payload["recipes"]):
        name = normalize_recipe_name(str(raw.get("菜谱名称") or ""))
        steps = normalize_steps(str(raw.get("做法") or ""))
        source_items = extract_ingredients(str(raw.get("原料") or ""))
        if (not name or not re.search(r"[\u4e00-\u9fff]", name) or "�" in name or name in names
                or len(steps.splitlines()) < 1 or not source_items):
            rejects["missing"] += 1
            continue
        role = classify(name, paths.get(int(raw.get("菜谱类型号") or 0), ""), source_items)
        mapped = []
        failed = False
        for item in source_items:
            food = matcher.match(item["source"])
            if food is None:
                if item["quantity"] <= 10 or any(x in item["source"] for x in AROMATICS):
                    food = ignored_micro_food(conn, item["source"])
                else:
                    failed = True
                    break
            mapped.append({"food": food, "source": item["source"],
                           "display": display_name(food["name"], item["source"]),
                           "quantity": item["quantity"]})
        if failed or not mapped:
            rejects["unmatched"] += 1
            continue
        mapped = merge_same_food(mapped)
        mark_micro_ingredients(mapped)
        scale_to_serving(mapped, role)
        add_water_if_needed(conn, mapped, role)
        for item in mapped:
            item["quantity"] = round(item["quantity"], 1)
        n = nutrition(mapped)
        if not valid_nutrition(role, n):
            rejects["nutrition"] += 1
            continue
        names.add(name)
        candidates.append({"name": name, "role": role, "category": category_for(role, idx),
                           "steps": steps, "minutes": max(8, min(90, 15 + len(steps.splitlines()) * 5)),
                           "accent": "green", "source": "菜谱数据库.mdb（结构化导入）",
                           "source_ref": f"MDB:{raw.get('菜谱ID')}", "ingredients": mapped, "nutrition": n})

    # 扩大推荐池，同时按角色设上限，优先纳入 MDB 中可可靠结构化和复算的菜谱。
    quotas = {"meat": 220, "vegetable": 160, "soup": 80, "staple": 55, "drink": 35, "mixed": 110}
    selected, counts = [], defaultdict(int)
    for recipe in sorted(candidates, key=lambda x: (x["role"], x["name"])):
        quota = quotas.get(recipe["role"], 30)
        if counts[recipe["role"]] >= quota:
            continue
        selected.append(recipe)
        counts[recipe["role"]] += 1
    curated = seed_curated(conn, matcher)

    conn.execute("DELETE FROM favorites")
    conn.execute("DELETE FROM recipe_foods")
    conn.execute("DELETE FROM recipes")
    conn.execute("DELETE FROM sqlite_sequence WHERE name IN ('recipes','recipe_foods')")
    for recipe in curated + selected:
        insert_recipe(conn, recipe)
    conn.commit()

    # 重新读取并做数据库级复算，任何偏差或异常立即失败。
    bad = []
    for row in conn.execute("SELECT * FROM recipes ORDER BY id"):
        micro_sql = "rf.source_text LIKE '%【营养忽略】%'"
        calc = conn.execute(
            f"SELECT SUM(rf.quantity),SUM(CASE WHEN {micro_sql} THEN 0 ELSE rf.quantity*f.calories/100.0 END),"
            f"SUM(CASE WHEN {micro_sql} THEN 0 ELSE rf.quantity*f.protein/100.0 END),"
            f"SUM(CASE WHEN {micro_sql} THEN 0 ELSE rf.quantity*f.fat/100.0 END),"
            f"SUM(CASE WHEN {micro_sql} THEN 0 ELSE rf.quantity*f.carbs/100.0 END) "
            "FROM recipe_foods rf JOIN foods f ON f.id=rf.food_id WHERE rf.recipe_id=?",
            (row["id"],),
        ).fetchone()
        if not calc[0] or abs(calc[1] - row["total_calories"]) > 0.15 or row["total_calories"] > 1000 or row["total_protein"] > 90:
            bad.append((row["id"], row["name"], tuple(calc), row["total_calories"], row["total_protein"]))
    if bad:
        conn.rollback()
        raise RuntimeError(f"nutrition validation failed: {bad[:5]}")
    total = conn.execute("SELECT COUNT(*) FROM recipes").fetchone()[0]
    role_counts = dict(conn.execute("SELECT dish_role,COUNT(*) FROM recipes GROUP BY dish_role").fetchall())
    conn.close()
    print(json.dumps({"source_rows": len(payload["recipes"]), "accepted_candidates": len(candidates),
                      "database_recipes": total, "roles": role_counts, "rejects": dict(rejects),
                      "backup": str(BACKUP)}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
