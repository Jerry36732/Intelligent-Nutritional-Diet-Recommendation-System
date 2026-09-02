# -*- coding: utf-8 -*-
"""
从美食天下（home.meishichina.com）抓取菜谱并写入 data/diet.db。
仅用于课程项目本地营养库扩充；请控制频率，尊重网站服务条款。
"""
from __future__ import annotations

import argparse
import re
import sqlite3
import subprocess
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DB_PATH = ROOT / "data" / "diet.db"
UA = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
)

# 分类路径 -> (餐次候选, 菜品角色)
CATEGORIES = [
    ("zaocan", "早餐", "breakfast"),
    ("zhushi", "午餐", "staple"),
    ("recai", "午餐", "meat"),
    ("huncai", "午餐", "meat"),
    ("sucai", "晚餐", "vegetable"),
    ("liangcai", "晚餐", "vegetable"),
    ("tanggeng", "晚餐", "soup"),
    ("xiaochi", "早餐", "mixed"),
]


@dataclass
class ScrapedRecipe:
    source_id: str
    name: str
    meal: str
    role: str
    steps: str
    cook_minutes: int = 20
    ingredients: list[tuple[str, str]] = field(default_factory=list)
    url: str = ""


def fetch(url: str) -> str:
    # Windows 下 curl 比 urllib 更稳定（站点常对脚本 UA 限流）
    with tempfile.NamedTemporaryFile(delete=False, suffix=".html") as tmp:
        tmp_path = tmp.name
    try:
        cmd = [
            "curl.exe",
            "-sL",
            "--max-time",
            "20",
            "-A",
            UA,
            "-H",
            "Accept-Language: zh-CN,zh;q=0.9",
            "-H",
            "Referer: https://home.meishichina.com/recipe.html",
            url,
            "-o",
            tmp_path,
        ]
        subprocess.run(cmd, check=False, capture_output=True)
        return Path(tmp_path).read_text(encoding="utf-8", errors="ignore")
    finally:
        Path(tmp_path).unlink(missing_ok=True)


def strip_tags(text: str) -> str:
    text = re.sub(r"<[^>]+>", "", text)
    return re.sub(r"\s+", " ", text).strip()


def list_recipe_ids(category: str, page: int) -> list[str]:
    if page <= 1:
        url = f"https://home.meishichina.com/recipe/{category}/"
    else:
        url = f"https://home.meishichina.com/recipe/{category}/page/{page}/"
    try:
        html = fetch(url)
    except Exception as exc:
        print(f"  list fail {url}: {exc}")
        return []
    ids = sorted(set(re.findall(r"recipe-(\d+)\.html", html)))
    return ids


def normalize_recipe_name(name: str) -> str:
    name = name.strip()
    if name.endswith("的做法"):
        name = name[:-len("的做法")].strip()
    if name.endswith("做法") and len(name) > 2:
        name = name[:-2].strip()
    return name


def parse_recipe(source_id: str, meal: str, role: str) -> ScrapedRecipe | None:
    url = f"https://home.meishichina.com/recipe-{source_id}.html"
    try:
        html = fetch(url)
    except Exception as exc:
        print(f"  detail fail {url}: {exc}")
        return None

    m = re.search(r'id="recipe_title"\s+value="([^"]+)"', html)
    if not m:
        m = re.search(r"<title>([^<_]+)", html)
    name = strip_tags(m.group(1)) if m else ""
    name = normalize_recipe_name(name)
    if not name or name == "菜谱":
        return None

    ings = re.findall(
        r'<span class="category_s1">.*?(?:<a[^>]*>|<b>)(.*?)(?:</a>|</b>).*?</span>\s*'
        r'<span class="category_s2">(.*?)</span>',
        html,
        re.S,
    )
    ingredients: list[tuple[str, str]] = []
    skip = {"咸鲜", "口味", "煮", "工艺", "炒", "耗时", "难度", "简单", "普通", "高级", "十分钟", "二十分钟", "半小时", "一小时"}
    for raw_name, raw_amt in ings:
        n = strip_tags(raw_name)
        a = strip_tags(raw_amt)
        if not n or n in skip or a in skip:
            continue
        if a in {"口味", "工艺", "耗时", "难度"}:
            continue
        ingredients.append((n, a))

    steps = re.findall(
        r'<div class="recipeStep_word"><div class="grey">\d+</div>(.*?)</div>',
        html,
        re.S,
    )
    step_texts = [strip_tags(s) for s in steps if strip_tags(s)]
    steps_joined = "\n".join(f"{i+1}. {s}" for i, s in enumerate(step_texts))

    minutes = 20
    tm = re.search(r"(\d+)\s*分钟", html)
    if tm:
        minutes = max(5, min(120, int(tm.group(1))))
    # 从配料里的“十分钟/二十分钟”也可推断
    for n, a in ings:
        if "分钟" in strip_tags(n) or "分钟" in strip_tags(a):
            mm = re.search(r"(\d+)", strip_tags(n) + strip_tags(a))
            if mm:
                minutes = max(5, min(120, int(mm.group(1))))

    # 按菜名微调角色
    role2 = classify_role(name, role)

    return ScrapedRecipe(
        source_id=source_id,
        name=name,
        meal=meal if role2 != "breakfast" else "早餐",
        role=role2,
        steps=steps_joined or "详见原文步骤。",
        cook_minutes=minutes,
        ingredients=ingredients,
        url=url,
    )


def classify_role(name: str, default: str) -> str:
    drink_kw = ["汁", "牛奶", "豆浆", "酸奶", "拿铁", "咖啡", "奶茶", "米浆", "椰汁", "梨汁", "苹果汁", "橙汁", "柠檬茶", "饮品"]
    breakfast_kw = ["粥", "油条", "包子", "馒头", "三明治", "煎饼", "燕麦", "吐司", "面包"]
    staple_kw = ["饭", "面", "米", "饼", "馒头", "花卷", "河粉", "米粉", "意面", "面条"]
    soup_kw = ["汤", "羹", "煲"]
    veg_kw = ["拌", "沙拉", "青菜", "西兰花", "菠菜", "豆腐", "瓜", "菇", "茄", "豆芽", "藕"]
    meat_kw = ["肉", "牛", "猪", "羊", "鸡", "鸭", "鱼", "虾", "排骨", "翅", "蛋"]

    if any(k in name for k in drink_kw):
        return "drink"
    if "肉饼" in name or "海鲜饼" in name:
        return "meat"
    if "土豆饼" in name or "香椿饼" in name:
        return "vegetable"
    if "松饼" in name or "蛋糕" in name:
        return "dessert"
    if any(k in name for k in breakfast_kw) and default in {"breakfast", "mixed"}:
        return "breakfast"
    if any(k in name for k in soup_kw):
        return "soup"
    if any(k in name for k in staple_kw) and not any(k in name for k in meat_kw):
        return "staple"
    if any(k in name for k in meat_kw):
        return "meat"
    if any(k in name for k in veg_kw):
        return "vegetable"
    return default


def parse_quantity(amount: str) -> float:
    """粗略把 '100g'/'2个'/'1勺' 转成克。"""
    amount = amount.strip()
    m = re.search(r"([\d.]+)\s*(g|克|毫升|ml)", amount, re.I)
    if m:
        return float(m.group(1))
    m = re.search(r"([\d.]+)\s*个", amount)
    if m:
        return float(m.group(1)) * 50.0
    m = re.search(r"([\d.]+)\s*(勺|匙)", amount)
    if m:
        return float(m.group(1)) * 10.0
    m = re.search(r"([\d.]+)", amount)
    if m:
        return float(m.group(1)) * 30.0
    return 50.0


def ensure_columns(conn: sqlite3.Connection) -> None:
    cols = {r[1] for r in conn.execute("PRAGMA table_info(recipes)")}
    alters = []
    if "dish_role" not in cols:
        alters.append("ALTER TABLE recipes ADD COLUMN dish_role TEXT DEFAULT 'mixed'")
    if "source" not in cols:
        alters.append("ALTER TABLE recipes ADD COLUMN source TEXT")
    if "source_url" not in cols:
        alters.append("ALTER TABLE recipes ADD COLUMN source_url TEXT")
    for sql in alters:
        conn.execute(sql)
    conn.commit()


def load_food_index(conn: sqlite3.Connection) -> list[tuple[int, str, float, float, float, float]]:
    rows = conn.execute(
        "SELECT id, name, calories, protein, carbs, fat FROM foods ORDER BY length(name) DESC"
    ).fetchall()
    return [(int(r[0]), r[1], float(r[2] or 0), float(r[3] or 0), float(r[4] or 0), float(r[5] or 0)) for r in rows]


def match_food(food_index, ingredient_name: str):
    # exact / contains
    for fid, fname, cal, p, c, f in food_index:
        if ingredient_name == fname or ingredient_name in fname or fname in ingredient_name:
            return fid, cal, p, c, f
    # first char overlap for short names
    if len(ingredient_name) >= 2:
        for fid, fname, cal, p, c, f in food_index:
            if ingredient_name[:2] in fname:
                return fid, cal, p, c, f
    return None


def estimate_nutrition(ingredients, food_index):
    total_cal = total_p = total_c = total_f = 0.0
    linked = []
    for name, amt in ingredients:
        qty = parse_quantity(amt)
        matched = match_food(food_index, name)
        if matched:
            fid, cal, p, c, f = matched
            factor = qty / 100.0
            total_cal += cal * factor
            total_p += p * factor
            total_c += c * factor
            total_f += f * factor
            linked.append((fid, qty))
        else:
            # 未匹配：按普通食材估算
            total_cal += qty * 1.2
            total_p += qty * 0.05
            total_c += qty * 0.1
            total_f += qty * 0.03
    if total_cal < 40:
        total_cal, total_p, total_c, total_f = 220.0, 12.0, 20.0, 8.0
    return total_cal, total_p, total_c, total_f, linked


def upsert_recipe(conn: sqlite3.Connection, recipe: ScrapedRecipe, food_index) -> bool:
    existing = conn.execute(
        "SELECT id FROM recipes WHERE source_url = ? OR (name = ? AND ifnull(source,'') = 'meishichina')",
        (recipe.url, recipe.name),
    ).fetchone()
    if not existing:
        # 公共库同名食谱以现有审核记录为准；网页导入不再制造第二份同名卡片。
        public_same_name = conn.execute(
            "SELECT id FROM recipes WHERE name=? "
            "AND IFNULL(source_ref,'') NOT LIKE 'USER:%' LIMIT 1",
            (recipe.name,),
        ).fetchone()
        if public_same_name:
            return False
    cal, p, c, f, linked = estimate_nutrition(recipe.ingredients, food_index)
    steps = recipe.steps
    if recipe.ingredients:
        ing_line = "、".join(n for n, _ in recipe.ingredients[:12])
        steps = f"用料：{ing_line}\n{steps}"

    if existing:
        rid = existing[0]
        conn.execute(
            """UPDATE recipes SET category=?, steps=?, cook_minutes=?, dish_role=?,
               total_calories=?, total_protein=?, total_carbs=?, total_fat=?, source=?, source_url=?
               WHERE id=?""",
            (
                recipe.meal,
                steps,
                recipe.cook_minutes,
                recipe.role,
                cal,
                p,
                c,
                f,
                "meishichina",
                recipe.url,
                rid,
            ),
        )
        conn.execute("DELETE FROM recipe_foods WHERE recipe_id=?", (rid,))
    else:
        cur = conn.execute(
            """INSERT INTO recipes(name, category, steps, cook_minutes, accent, dish_role,
               total_calories, total_protein, total_carbs, total_fat, source, source_url)
               VALUES (?,?,?,?,?,?,?,?,?,?,?,?)""",
            (
                recipe.name,
                recipe.meal,
                steps,
                recipe.cook_minutes,
                "teal",
                recipe.role,
                cal,
                p,
                c,
                f,
                "meishichina",
                recipe.url,
            ),
        )
        rid = cur.lastrowid

    for fid, qty in linked[:20]:
        try:
            conn.execute(
                "INSERT OR REPLACE INTO recipe_foods(recipe_id, food_id, quantity) VALUES (?,?,?)",
                (rid, fid, qty),
            )
        except sqlite3.Error:
            pass
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pages", type=int, default=4, help="每个分类抓取页数")
    parser.add_argument("--delay", type=float, default=0.35, help="请求间隔秒")
    parser.add_argument("--limit", type=int, default=180, help="最多新增/更新菜谱数")
    args = parser.parse_args()

    if not DB_PATH.exists():
        raise SystemExit(f"missing db: {DB_PATH}")

    conn = sqlite3.connect(DB_PATH)
    ensure_columns(conn)
    food_index = load_food_index(conn)
    print(f"foods indexed: {len(food_index)}")

    seen_ids: set[str] = set()
    scraped: list[ScrapedRecipe] = []

    for cat, meal, role in CATEGORIES:
        for page in range(1, args.pages + 1):
            ids = list_recipe_ids(cat, page)
            print(f"[{cat} p{page}] {len(ids)} links")
            time.sleep(args.delay)
            for sid in ids:
                if sid in seen_ids:
                    continue
                seen_ids.add(sid)
                recipe = parse_recipe(sid, meal, role)
                time.sleep(args.delay)
                if not recipe:
                    continue
                scraped.append(recipe)
                print(f"  + {recipe.name} ({recipe.role}/{recipe.meal})", flush=True)
                if len(scraped) >= args.limit:
                    break
            if len(scraped) >= args.limit:
                break
        if len(scraped) >= args.limit:
            break

    # 首页补一批
    if len(scraped) < args.limit:
        try:
            html = fetch("https://home.meishichina.com/recipe.html")
            for sid in sorted(set(re.findall(r"recipe-(\d+)\.html", html))):
                if sid in seen_ids:
                    continue
                seen_ids.add(sid)
                recipe = parse_recipe(sid, "午餐", "mixed")
                time.sleep(args.delay)
                if recipe:
                    scraped.append(recipe)
                    print(f"  + {recipe.name} (home)", flush=True)
                if len(scraped) >= args.limit:
                    break
        except Exception as exc:
            print("home fail", exc, flush=True)

    ok = 0
    for recipe in scraped:
        if upsert_recipe(conn, recipe, food_index):
            ok += 1
    conn.commit()

    # 给旧菜谱补角色标签，便于荤素搭配
    for rid, name, cat in conn.execute("SELECT id, name, category FROM recipes").fetchall():
        role = classify_role(name or "", "breakfast" if cat == "早餐" else "mixed")
        if cat == "早餐":
            role = "breakfast"
        conn.execute("UPDATE recipes SET dish_role=? WHERE id=? AND (dish_role IS NULL OR dish_role='' OR dish_role='mixed')", (role, rid))
    conn.commit()

    total = conn.execute("SELECT count(*) FROM recipes").fetchone()[0]
    by_role = conn.execute(
        "SELECT ifnull(dish_role,'mixed'), count(*) FROM recipes GROUP BY 1 ORDER BY 2 DESC"
    ).fetchall()
    print(f"upserted={ok} total_recipes={total} roles={by_role}", flush=True)
    conn.close()


if __name__ == "__main__":
    main()
