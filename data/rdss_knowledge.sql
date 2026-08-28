-- RDSS 知识库：过敏原-食物 / 营养素-食物 / 营养素-缺乏症映射
-- 由 DatabaseManager::ensureSchema 初始化（可重复执行）

CREATE TABLE IF NOT EXISTS allergen_food_map (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    allergen TEXT NOT NULL,
    food_keyword TEXT NOT NULL,
    UNIQUE(allergen, food_keyword)
);

CREATE TABLE IF NOT EXISTS nutrient_food_map (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    nutrient TEXT NOT NULL,
    food_keyword TEXT NOT NULL,
    boost_score REAL NOT NULL DEFAULT 1.0,
    UNIQUE(nutrient, food_keyword)
);

CREATE TABLE IF NOT EXISTS deficiency_nutrient_map (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    deficiency TEXT NOT NULL UNIQUE,
    nutrient TEXT NOT NULL,
    description TEXT
);

CREATE TABLE IF NOT EXISTS medical_food_restrict (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    condition_name TEXT NOT NULL,
    ban_keyword TEXT NOT NULL,
    UNIQUE(condition_name, ban_keyword)
);
