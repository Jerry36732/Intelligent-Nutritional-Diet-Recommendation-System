-- 食物营养成分表（数据来源于中国食物成分表 / nlc.chinanutri.cn）
CREATE TABLE IF NOT EXISTS foods (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id       INTEGER NOT NULL UNIQUE,
    name            TEXT NOT NULL,
    category_one    INTEGER,
    category_two    INTEGER,
    edible          TEXT,
    water           TEXT,
    energy_kj       REAL,
    calories        REAL,   -- kcal/100g
    protein         REAL,   -- g/100g
    fat             REAL,   -- g/100g
    carbs           REAL,   -- g/100g
    cholesterol     TEXT,
    ash             TEXT,
    dietary_fiber   TEXT,
    vitamin_a       TEXT,
    thiamin         TEXT,
    riboflavin      TEXT,
    niacin          TEXT,
    vitamin_c       TEXT,
    calcium         TEXT,
    iron            TEXT,
    unit            TEXT NOT NULL DEFAULT '100g',
    source          TEXT NOT NULL DEFAULT 'nlc.chinanutri.cn',
    updated_at      TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))
);

CREATE INDEX IF NOT EXISTS idx_foods_name ON foods(name);
CREATE INDEX IF NOT EXISTS idx_foods_calories ON foods(calories);
