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
    "dessert": (80.0, 220.0),
    "mixed": (200.0, 320.0),
}

EXCLUDE_CANDIDATE = ("软糖", "奶糖", "糖果", "婴儿", "米粉", "调味汁", "罐头")
MEAT_WORDS = ("猪", "牛", "羊", "鸡", "鸭", "鹅", "鱼", "虾", "蟹", "肉", "排骨", "肝", "蛋", "火腿")
STAPLE_WORDS = ("米饭", "馒头", "花卷", "包子", "饺", "面条", "面饭", "粥", "窝头",
                "烧饼", "煎饼", "葱油饼", "面饼")
SOUP_WORDS = ("汤", "羹")
DRINK_WORDS = ("汁", "牛奶", "牛乳", "酸奶", "豆浆", "饮料")
AROMATICS = (
    "葱", "姜", "蒜", "香菜", "味精", "鸡精", "盐", "胡椒", "花椒", "大料", "八角",
    "桂皮", "肉桂", "丁香", "香叶", "陈皮", "草果", "肉豆蔻", "迷迭香", "月桂",
    "柠檬叶", "茉莉花", "香草精", "五香粉", "香精",
    "香料", "料酒", "淀粉",
)
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
    "生菜油": "植物油（通用）", "色拉油": "植物油（通用）",
    "盐": "精盐", "食盐": "精盐", "葱": "大葱", "姜": "姜[黄姜](鲜)",
    "白糖": "糖（白砂糖）", "白砂糖": "糖（白砂糖）", "蔗糖": "糖（白砂糖）",
    "香油": "芝麻油[香油]", "芝麻油": "芝麻油[香油]",
    "土豆": "马铃薯[土豆，洋芋]", "马铃薯": "马铃薯[土豆，洋芋]",
    "红枣": "枣(干)", "枣": "枣(干)", "干辣椒": "辣椒(红，尖，干)",
    "红辣椒": "辣椒(红，小)", "黄辣椒": "辣椒(红，小)",
    "青辣椒": "辣椒(青，尖)", "青椒": "辣椒(青，尖)", "圆椒": "辣椒(青，尖)",
    "香蕉": "香蕉[甘蕉]", "柚子": "柚[文旦]", "橙子": "橙",
    "皮蛋": "蛋（松花蛋，鸭蛋)[皮蛋]", "松花蛋": "蛋（松花蛋，鸭蛋)[皮蛋]",
    "甜豆浆": "豆浆", "豆皮": "豆腐皮", "豆苗": "豌豆苗",
    "麻油": "芝麻油[香油]", "水": "饮用水", "冰块": "饮用水",
    "莴苣": "莴笋[莴苣](鲜)", "小黄瓜": "黄瓜[胡瓜](鲜)",
    "面包粉": "面包(均值)", "大方面包": "面包(均值)",
    "五谷饭": "米饭(蒸)(均值)", "鸽蛋": "蛋（鸡蛋，均值)",
    "胡椒末": "胡椒粉", "胡椒面": "胡椒粉", "白胡椒": "胡椒粉",
    "淀粉": "玉米淀粉", "干淀粉": "玉米淀粉", "面粉": "小麦粉(标准粉)",
    "海米": "虾米[海米，虾仁]", "鲩鱼": "草鱼[白鲩，草包鱼]",
    "虾头": "虾（海虾）", "大虾头": "虾（海虾）",
    "肉蟹": "蟹（河蟹）", "精炼油": "植物油（通用）", "熟油": "植物油（通用）",
    "郫县豆瓣": "豆瓣酱", "猪二刀肉": "猪肉(肥瘦)(均值)",
    "猪骨肉": "猪肉(肥瘦)(均值)", "小牛后腿肉": "牛肉(后腿)",
    "鲜汤": "牛肉清汤（同类营养）",
    "熟碎花生米": "花生仁(炒)", "碎米芽菜": "榨菜",
    "水发木耳": "木耳(水发)[黑木耳，云耳]", "木耳": "木耳(水发)[黑木耳，云耳]",
    "水发玉兰片": "玉兰片", "凤梨": "菠萝[凤梨，地菠萝]",
    "鲜干贝": "扇贝(鲜)", "干贝": "扇贝(干)[干贝]",
    "猪肥肉": "猪肉(肥)", "瘦猪肉": "猪肉(瘦)", "猪瘦肉": "猪肉(瘦)",
    "猪爪": "猪蹄", "猪肘子": "猪蹄", "猪排骨": "猪小排",
    "鸡脯肉": "鸡胸脯肉", "鸭子": "鸭(均值)", "母鸡": "鸡(均值)",
    "清水": "饮用水", "温开水": "饮用水",
    "牛肉清汤": "牛肉清汤（同类营养）", "牛清汤": "牛肉清汤（同类营养）",
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
    "鱼肉": "鲤鱼[鲤拐子]", "鲜鱼": "鲤鱼[鲤拐子]", "净鱼": "鲤鱼[鲤拐子]",
    "蕃茄": "番茄[西红柿]", "大蕃茄": "番茄[西红柿]",
    "西洋芹": "芹菜茎", "西芹": "芹菜茎",
    "沙拉油": "植物油（通用）", "炸油": "植物油（通用）",
    "味噌": "酱油(均值)", "橘子": "橙", "橘": "橙",
    "鲜冬菇": "香菇[香蕈，冬菇](鲜)", "黑橄榄": "橄榄(白榄)",
    "北杏": "杏仁", "大排骨": "猪小排", "鸽子": "鸡(均值)",
    "鹌鹑": "鸡(均值)", "荷叶夹": "馒头(均值)", "清米汤": "粳米粥",
    "猪肺": "猪肉(肥瘦)(均值)", "云腿": "猪肉(肥瘦)(均值)",
    "党参": "枸杞子", "沙司": "酱油(均值)",
}

CATEGORY_FALLBACKS = (
    (("鸡",), "鸡(均值)"),
    (("鸭",), "鸭(均值)"),
    (("鹅",), "鹅(均值)"),
    (("猪肉", "肉馅", "肉丝", "肉片"), "猪肉(肥瘦)(均值)"),
    (("牛肉",), "牛肉(均值)"),
    (("羊肉",), "羊肉(均值)"),
    (("鱼",), "鲤鱼[鲤拐子]"),
    (("虾",), "虾（海虾）"),
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
    value = re.sub(r"^(?:原料|主料|配料|辅料|材料|调料)\s*[:：]?\s*", "", value)
    value = re.sub(r"(?:切末|切丝|切片|少许|适量)$", "", value)
    value = re.sub(r"(?:克|千克|公斤|kg|斤|两|一只|半只|一只半|一个)$", "", value, flags=re.I).strip()
    replacements = {
        "鲜肥母鸡": "母鸡", "肥嫩母鸡": "母鸡", "嫩母鸡": "母鸡", "开膛嫩仔公鸡": "母鸡",
        "净填鸭": "鸭子", "老鸭": "鸭子", "鲜活虾": "虾", "大虾": "虾",
        "鲜干贝粒": "鲜干贝", "桃仁": "核桃", "蒜泥": "大蒜", "蒜瓣": "大蒜",
        "猪五花肉": "猪肉(肥瘦)", "猪肥瘦肉": "猪肉(肥瘦)", "熟白芝麻": "芝麻",
        "熟花生油": "花生油", "化猪油": "猪油", "熟猪油": "猪油", "油": "植物油",
        "绍酒": "料酒", "白酒": "酒", "烧酒": "酒", "米醋": "醋", "醋精": "醋",
        "香茹": "香菇",
    }
    return replacements.get(value, value)


def display_name(food_name: str, source_name: str = "") -> str:
    if food_name in DISPLAY_ALIASES:
        return DISPLAY_ALIASES[food_name]
    source = plain_name(source_name)
    if source:
        source = source.replace("鲜", "").replace("熟", "").replace("咖哩", "咖喱")
        source = re.sub(r"^(?:调味料|配料)\s*[:：]\s*", "", source)
        source = re.sub(r"^[碎切]", "", source)
        source = re.sub(r"[（(]\s*\d+(?:\.\d+)?\s*(?:个|只|枚|片|块|根|条|朵|瓣|棵)[）)]\s*$", "", source)
        source = re.sub(
            r"(?:约)?(?:\d+(?:\.\d+)?(?:[/／]\d+)?|[一二两三四五六七八九十半]+)(?:约)?\s*"
            r"(?:小块|中匙|小勺|小匙|茶勺|茶匙|汤勺|汤匙|大匙|匙|棵|粒|杯|个|只|枚|片|块|根|条|朵|瓣|张)?(?:半)?[~～/／]?$",
            "", source,
        ).strip()
        source = re.sub(r"(?:公斤|千克|kg)$", "", source, flags=re.I).strip()
        source = re.sub(r"[lI](?:小勺|小匙|茶勺|茶匙|汤勺|汤匙|大匙|匙)$", "", source).strip()
        source = re.sub(r"(?:小勺|小匙|茶勺|茶匙|汤勺|汤匙|大匙|匙)$", "", source).strip()
        source = re.sub(r"(?:约|各)$", "", source).strip()
        source = {
            "猪蹄约克生姜": "生姜", "黄姜粉或咖喱粉": "咖喱粉",
            "蒜末": "大蒜", "蒜泥": "大蒜", "葱花": "香葱",
            "姜数片磨鼓半汤匙": "姜、豆豉", "姜数片磨鼓": "姜、豆豉",
        }.get(source, source)
        if len(source) >= 1:
            return source
    return plain_name(food_name) or food_name


def normalize_steps(text: str) -> str:
    text = (text or "").replace("\r", "\n").strip()
    text = re.sub(r"\s*\((\d+)\)\s*", r"\n\1. ", text)
    text = re.sub(r"\s*（(\d+)）\s*", r"\n\1. ", text)
    # MDB 中既有“1.”“1、”，也有“1。”“1．”编号。先移除源编号，
    # 再统一编号，避免生成“1. 1。……”并在详情页中出现孤立数字步骤。
    lines = [
        re.sub(r"^(?:\d+[.、。．)）]\s*)+", "", x.strip())
        for x in text.splitlines()
        if x.strip()
    ]
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
    """逐项解析 MDB 原料。

    除克重外也保留“2个虾头、4张煎饼、1只鸡”等计数原料。计数单位在
    原始 MDB 没有克重时按食材/单位的保守标准重量折算，并保留 raw_text，
    便于详情展示、审计和日后按更精确来源回填。
    """
    items: list[dict] = []
    pending: list[str] = []

    number_words = {
        "半": 0.5, "一": 1.0, "二": 2.0, "两": 2.0, "三": 3.0,
        "四": 4.0, "五": 5.0, "六": 6.0, "七": 7.0, "八": 8.0,
        "九": 9.0, "十": 10.0,
    }

    def number(value: str) -> float:
        value = value.strip()
        if value in number_words:
            return number_words[value]
        if "/" in value:
            numerator, denominator = value.split("/", 1)
            return float(numerator) / float(denominator)
        return float(value)

    def convert(value: str, unit: str) -> float:
        qty = float(value)
        unit = unit.lower()
        if unit in ("千克", "公斤", "kg"):
            return qty * 1000.0
        if unit in ("升", "l"):
            return qty * 1000.0
        if unit == "斤":
            return qty * 500.0
        if unit == "两":
            return qty * 50.0
        return qty

    def names(value: str) -> list[str]:
        value = re.sub(r"[（(].*$", "", value).strip(" ：:。.")
        value = re.sub(r"^(?:调味料|[A-CＡ-Ｃ]料)\s*[:：]?\s*", "", value, flags=re.I)
        value = re.sub(r"(?:各)?(?:适量|少许)$", "", value).strip()
        value = re.sub(r"(?:约|各)$", "", value).strip()
        value = re.sub(r"(?:约耗|实耗)$", "", value).strip()
        value = re.sub(
            r"\d+(?:\.\d+)?\s*(?:克|g|千克|公斤|kg|斤|两|毫升|ml|升|l)", "", value,
            flags=re.I).strip()
        value = re.sub(r"\d+(?:\.\d+)?\s*(?:小匙|茶匙|汤匙|大匙)", "", value).strip()
        value = re.sub(r"\d+(?:\.\d+)?\s*[－—–-]\s*$", "", value).strip()
        value = re.sub(
            r"(?:\d+/\d+|\d+(?:\.\d+)?|半|一|二|两|三|四|五|六|七|八|九|十)\s*"
            r"(?:个|只|枚|片|块|根|条|朵|瓣|张|碗|支|杯)", "", value).strip()
        value = re.sub(r"(?:约|各)$", "", value).strip()
        if value in ("葱姜", "葱、姜"):
            return ["葱", "姜"]
        return [value] if value else []

    def count_grams(source: str, count: float, unit: str) -> float:
        compact = plain_name(source)
        # 食材本身的常见可食部重量优先于泛化单位。这里只在源数据没有克重时使用。
        specific = (
            (("蛋黄",), 15.0), (("鹌鹑蛋",), 10.0), (("鸡蛋", "全蛋"), 50.0),
            (("虾头",), 15.0), (("大虾", "对虾", "海虾", "鲜虾"), 25.0),
            (("蒜瓣", "大蒜"), 5.0), (("香菇", "冬菇"), 15.0), (("红枣", "枣"), 8.0),
            (("番茄", "西红柿"), 150.0), (("土豆", "马铃薯"), 200.0),
            (("胡萝卜",), 150.0), (("苹果",), 200.0), (("柠檬",), 100.0),
            (("香蕉",), 120.0), (("面包",), 25.0), (("鸡翅",), 50.0),
            (("鸡腿",), 150.0), (("螃蟹", "河蟹"), 150.0),
            (("茉莉花", "玫瑰花", "菊花"), 0.2),
            (("鸡",), 1000.0), (("鸭",), 1500.0), (("鱼",), 500.0),
        )
        for keywords, grams in specific:
            if any(key in compact for key in keywords):
                return count * grams
        defaults = {
            "瓣": 5.0, "片": 10.0, "枚": 50.0, "个": 50.0, "只": 50.0,
            "块": 30.0, "根": 50.0, "条": 100.0, "朵": 15.0, "张": 20.0,
            "碗": 200.0, "支": 50.0, "杯": 200.0,
        }
        return count * defaults.get(unit, 50.0)

    # MDB 的少数记录使用私用区字符 U+E5E5 分隔原料，或只用空格连接
    # 连续的“食材+克重”。先统一分隔，避免整段被误当作一个超长食材名。
    normalized_raw = (raw or "").replace("\ue5e5", "、").replace("\ue5e4", "")
    normalized_raw = normalized_raw.replace("\u3000", "、")
    normalized_raw = re.sub(
        r"((?:\d+(?:\.\d+)?)\s*(?:克|g|千克|公斤|kg|斤|两|毫升|ml|升|l))"
        r"\s+(?=[\u4e00-\u9fff])",
        r"\1、", normalized_raw, flags=re.I,
    )
    normalized_raw = re.sub(
        r"(适量)\s*(?=(?:精炼油|植物油|花生油|猪油|色拉油))",
        r"\1、", normalized_raw,
    )
    normalized_raw = re.sub(
        r"((?:各)?(?:\d+/\d+|\d+(?:\.\d+)?|半|一|二|两|三|四|五|六|七|八|九|十)\s*"
        r"(?:个|只|枚|片|块|根|条|朵|瓣|张|碗|支|杯|小匙|大匙|茶匙|汤匙))"
        r"\s*(?=(?!约\d)[\u4e00-\u9fff])",
        r"\1、", normalized_raw,
    )

    for token in re.split(r"[,，;；、。\n]+", normalized_raw):
        token = token.strip()
        if not token:
            continue
        egg = re.search(r"((?:鸡蛋|全蛋))\s*(\d+(?:\.\d+)?)\s*(?:个|只|枚)", token)
        if egg and not re.search(r"\d+(?:\.\d+)?\s*(?:克|g|千克|公斤|kg|斤|两|毫升|ml|升|l)", token, re.I):
            items.append({"source": "鸡蛋", "quantity": float(egg.group(2)) * 50.0,
                          "raw_text": token, "estimated_from_count": True})
            continue
        actual = re.search(r"实耗(?:约)?\s*(\d+(?:\.\d+)?)\s*(克|g|千克|公斤|kg|斤|两|毫升|ml|升|l)", token, re.I)
        matches = list(re.finditer(r"(\d+(?:\.\d+)?)\s*(克|g|千克|公斤|kg|斤|两|毫升|ml|升|l)", token, re.I))
        match = actual or (matches[-1] if matches else None)
        if match is None:
            spoon_match = re.search(
                r"(\d+/\d+|\d+(?:\.\d+)?|半|一|二|两|三|四|五|六|七|八|九|十)\s*"
                r"(小匙|茶匙|大匙|汤匙|杯)\s*$", token)
            if spoon_match:
                count = number(spoon_match.group(1))
                grams = count * {"小匙": 5.0, "茶匙": 5.0, "大匙": 15.0,
                                 "汤匙": 15.0, "杯": 200.0}[spoon_match.group(2)]
                for source in names(token[:spoon_match.start()]):
                    items.append({"source": source, "quantity": grams, "raw_text": token,
                                  "estimated_from_count": True})
                continue
            count_match = re.search(
                r"(\d+/\d+|\d+(?:\.\d+)?|半|一|二|两|三|四|五|六|七|八|九|十)\s*"
                r"(个|只|枚|片|块|根|条|朵|瓣|张|碗|支|杯)(?:左右|约)?\s*$", token)
            if count_match:
                source_names = names(token[:count_match.start()])
                for source in source_names:
                    qty = count_grams(source, number(count_match.group(1)), count_match.group(2))
                    if 0.2 <= qty <= 10000:
                        items.append({"source": source, "quantity": qty, "raw_text": token,
                                      "estimated_from_count": True})
                continue
            if re.search(r"(?:适量|少许)\s*$", token):
                sources = names(token)
                if "各" in token and pending:
                    sources = pending + sources
                    pending.clear()
                for source in sources:
                    canonical = canonical_source(source)
                    if canonical:
                        qty = 2.0 if any(word in canonical for word in AROMATICS) else 5.0
                        items.append({"source": source, "quantity": qty, "raw_text": token,
                                      "estimated_from_count": False})
                continue
            pending.extend(names(token))
            continue
        qty = convert(match.group(1), match.group(2))
        prefix = token[:match.start()].strip()
        current = names(prefix)
        shared = "各" in prefix
        targets = (pending + current) if shared else current
        if shared:
            pending.clear()
        for source in targets:
            if 0.2 <= qty <= 10000:
                items.append({"source": source, "quantity": qty, "raw_text": token,
                              "estimated_from_count": False})
    # “盐、味精各适量”没有可换算克重，但仍应出现在原料清单中；按微量调料
    # 2 g 记载并在后续营养计算中忽略，避免详情页把步骤使用的调料漏掉。
    for source in pending:
        canonical = canonical_source(source)
        if canonical and canonical not in ("调味料", "原料", "主料", "配料"):
            quantity = 2.0 if any(word in canonical for word in AROMATICS) else 5.0
            items.append({"source": source, "quantity": quantity, "raw_text": source + "适量",
                          "estimated_from_count": False})
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
    if "肉饼" in name or "海鲜饼" in name:
        return "meat"
    if "土豆饼" in name or "香椿饼" in name:
        return "vegetable"
    if "松饼" in name or "蛋糕" in name:
        return "dessert"
    if any(x in name for x in STAPLE_WORDS) or "家常主食" in type_path:
        return "staple"
    ing_blob = "".join(x["source"] for x in ingredients)
    if any(x in ing_blob for x in MEAT_WORDS):
        return "meat"
    if "素菜" in type_path or not any(x in blob + ing_blob for x in MEAT_WORDS):
        return "vegetable"
    return "mixed"


def category_for(role: str, index: int) -> str:
    if role in ("breakfast", "drink", "fruit", "staple", "dessert"):
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
            raw_text = item.get("raw_text", "")
            if raw_text and raw_text not in merged[food_id].get("raw_text", ""):
                merged[food_id]["raw_text"] = "；".join(
                    x for x in (merged[food_id].get("raw_text", ""), raw_text) if x)
            merged[food_id]["estimated_from_count"] = bool(
                merged[food_id].get("estimated_from_count") or item.get("estimated_from_count"))
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
    if not conn.execute("SELECT 1 FROM foods WHERE name='牛肉清汤（同类营养）'").fetchone():
        conn.execute(
            "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source) "
            "VALUES(-900004,'牛肉清汤（同类营养）',7.0,1.14,0.22,0.04,'100g',"
            "'USDA FoodData Central 171538：beef broth ready-to-serve，同类近似值')"
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
                           "quantity": item["quantity"],
                           "raw_text": item.get("raw_text", item["source"]),
                           "estimated_from_count": item.get("estimated_from_count", False)})
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
    quotas = {"meat": 220, "vegetable": 160, "soup": 80, "staple": 55,
              "drink": 35, "dessert": 35, "mixed": 110}
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
    # 精选种子和 MDB 候选可能同名；只保留原料和步骤更完整的一条。
    unique_recipes: dict[str, dict] = {}
    for recipe in curated + selected:
        previous = unique_recipes.get(recipe["name"])
        score = (len(recipe["ingredients"]), len(recipe["steps"]))
        previous_score = ((len(previous["ingredients"]), len(previous["steps"]))
                          if previous else (-1, -1))
        if previous is None or score > previous_score:
            unique_recipes[recipe["name"]] = recipe
    for recipe in unique_recipes.values():
        insert_recipe(conn, recipe)
    conn.execute(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_recipes_public_unique_name ON recipes(name) "
        "WHERE IFNULL(source_ref,'') NOT LIKE 'USER:%'"
    )
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
