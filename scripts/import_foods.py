# -*- coding: utf-8 -*-
"""
从中国疾病预防控制中心食物营养成分查询平台导入食材数据到 SQLite。
数据来源: https://nlc.chinanutri.cn/fq/
"""
from __future__ import annotations

import json
import re
import sqlite3
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

BASE_URL = "https://nlc.chinanutri.cn/fq/"
API_LIST = "FoodInfoQueryAction!queryFoodInfoList.do"
REQUEST_DELAY = 0.12
MAX_RETRIES = 3

ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "data"
DB_PATH = DATA_DIR / "diet.db"
SCHEMA_PATH = DATA_DIR / "schema.sql"

# list API 字段索引（与站点列表页列一致）
IDX_ID = 0
IDX_NAME = 2
IDX_EDIBLE = 5
IDX_WATER = 6
IDX_ENERGY = 7
IDX_PROTEIN = 8
IDX_FAT = 9
IDX_CHOLESTEROL = 10
IDX_ASH = 11
IDX_CARBS = 12
IDX_FIBER = 13
IDX_VITAMIN_A = 14
IDX_THIAMIN = 17
IDX_RIBOFLAVIN = 18
IDX_NIACIN = 19
IDX_VITAMIN_C = 20
IDX_CALCIUM = 21
IDX_IRON = 22


def parse_number(value: str | None) -> float | None:
    if not value or not isinstance(value, str):
        return None
    value = value.strip()
    if value in ("", "—", "-", "Tr", "un", "NULL"):
        return None
    match = re.search(r"[-+]?\d*\.?\d+", value.replace(",", ""))
    if not match:
        return None
    try:
        return float(match.group())
    except ValueError:
        return None


def parse_energy_kj(value: str | None) -> float | None:
    num = parse_number(value)
    return num if num is not None else None


def kj_to_kcal(kj: float | None) -> float | None:
    if kj is None:
        return None
    return round(kj / 4.184, 2)


def fetch_food_list(
    category_one: int = 0,
    category_two: int = 0,
    page_num: int = 1,
) -> dict:
    params = {
        "categoryOne": category_one,
        "categoryTwo": category_two,
        "foodName": "",
        "pageNum": page_num,
        "field": 0,
        "flag": 1,
    }
    url = BASE_URL + API_LIST
    data = urllib.parse.urlencode(params).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64)",
            "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
            "Referer": BASE_URL,
            "X-Requested-With": "XMLHttpRequest",
        },
        method="POST",
    )
    for attempt in range(MAX_RETRIES):
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except (urllib.error.URLError, urllib.error.HTTPError, json.JSONDecodeError, TimeoutError) as exc:
            if attempt == MAX_RETRIES - 1:
                raise
            time.sleep(1.5 * (attempt + 1))
            print(f"  重试 page {page_num}: {exc}")
    raise RuntimeError("unreachable")


def init_db(conn: sqlite3.Connection) -> None:
    if SCHEMA_PATH.exists():
        conn.executescript(SCHEMA_PATH.read_text(encoding="utf-8"))
    conn.commit()


def row_from_item(item: list, category_one: int, category_two: int) -> dict | None:
    if len(item) < 13:
        return None
    source_id = item[IDX_ID]
    name = (item[IDX_NAME] or "").strip()
    if not source_id or not name:
        return None

    energy_kj = parse_energy_kj(item[IDX_ENERGY] if len(item) > IDX_ENERGY else None)
    protein = parse_number(item[IDX_PROTEIN] if len(item) > IDX_PROTEIN else None)
    fat = parse_number(item[IDX_FAT] if len(item) > IDX_FAT else None)
    carbs = parse_number(item[IDX_CARBS] if len(item) > IDX_CARBS else None)

    def txt(idx: int) -> str | None:
        if len(item) <= idx:
            return None
        v = item[idx]
        if v is None:
            return None
        s = str(v).strip()
        return s if s else None

    return {
        "source_id": int(source_id),
        "name": name,
        "category_one": category_one,
        "category_two": category_two,
        "edible": txt(IDX_EDIBLE),
        "water": txt(IDX_WATER),
        "energy_kj": energy_kj,
        "calories": kj_to_kcal(energy_kj),
        "protein": protein,
        "fat": fat,
        "carbs": carbs,
        "cholesterol": txt(IDX_CHOLESTEROL),
        "ash": txt(IDX_ASH),
        "dietary_fiber": txt(IDX_FIBER),
        "vitamin_a": txt(IDX_VITAMIN_A),
        "thiamin": txt(IDX_THIAMIN),
        "riboflavin": txt(IDX_RIBOFLAVIN),
        "niacin": txt(IDX_NIACIN),
        "vitamin_c": txt(IDX_VITAMIN_C),
        "calcium": txt(IDX_CALCIUM),
        "iron": txt(IDX_IRON),
        "unit": "100g",
        "source": "nlc.chinanutri.cn",
    }


INSERT_SQL = """
INSERT INTO foods (
    source_id, name, category_one, category_two, edible, water,
    energy_kj, calories, protein, fat, carbs,
    cholesterol, ash, dietary_fiber, vitamin_a, thiamin, riboflavin,
    niacin, vitamin_c, calcium, iron, unit, source
) VALUES (
    :source_id, :name, :category_one, :category_two, :edible, :water,
    :energy_kj, :calories, :protein, :fat, :carbs,
    :cholesterol, :ash, :dietary_fiber, :vitamin_a, :thiamin, :riboflavin,
    :niacin, :vitamin_c, :calcium, :iron, :unit, :source
)
ON CONFLICT(source_id) DO UPDATE SET
    name=excluded.name,
    category_one=excluded.category_one,
    category_two=excluded.category_two,
    edible=excluded.edible,
    water=excluded.water,
    energy_kj=excluded.energy_kj,
    calories=excluded.calories,
    protein=excluded.protein,
    fat=excluded.fat,
    carbs=excluded.carbs,
    cholesterol=excluded.cholesterol,
    ash=excluded.ash,
    dietary_fiber=excluded.dietary_fiber,
    vitamin_a=excluded.vitamin_a,
    thiamin=excluded.thiamin,
    riboflavin=excluded.riboflavin,
    niacin=excluded.niacin,
    vitamin_c=excluded.vitamin_c,
    calcium=excluded.calcium,
    iron=excluded.iron,
    unit=excluded.unit,
    source=excluded.source,
    updated_at=datetime('now', 'localtime')
"""


def import_all(conn: sqlite3.Connection) -> int:
    """通过 categoryOne=0 & categoryTwo=0 拉取全库分页列表。"""
    first = fetch_food_list(0, 0, 1)
    total_pages = int(first.get("totalPages", 1))
    print(f"共 {total_pages} 页，开始导入...")

    inserted = 0
    seen: set[int] = set()

    for page in range(1, total_pages + 1):
        payload = first if page == 1 else fetch_food_list(0, 0, page)
        items = payload.get("list") or []
        page_count = 0
        for item in items:
            row = row_from_item(item, 0, 0)
            if not row or row["source_id"] in seen:
                continue
            seen.add(row["source_id"])
            conn.execute(INSERT_SQL, row)
            page_count += 1
            inserted += 1

        conn.commit()
        print(f"  页 {page}/{total_pages}，本页新增 {page_count}，累计 {inserted}")
        if page < total_pages:
            time.sleep(REQUEST_DELAY)

    return inserted


def main() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    try:
        init_db(conn)
        count = import_all(conn)
        total = conn.execute("SELECT COUNT(*) FROM foods").fetchone()[0]
        sample = conn.execute(
            "SELECT name, calories, protein, fat, carbs FROM foods ORDER BY id LIMIT 5"
        ).fetchall()
        print(f"\n导入完成：处理 {count} 条，数据库共 {total} 条食物记录")
        print("样例数据：")
        for row in sample:
            print(f"  {row['name']}: {row['calories']}kcal, P{row['protein']}g F{row['fat']}g C{row['carbs']}g")
        print(f"数据库路径: {DB_PATH}")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
