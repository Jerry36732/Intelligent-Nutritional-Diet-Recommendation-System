-- NAct 本体论风格知识库 + 推荐历史（由 DatabaseManager 初始化）

CREATE TABLE IF NOT EXISTS food_nutrient_relations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    food_id INTEGER NOT NULL,
    nutrient_id TEXT NOT NULL,
    concentration_level TEXT NOT NULL DEFAULT '中'
        CHECK(concentration_level IN ('高','中','低')),
    UNIQUE(food_id, nutrient_id)
);

CREATE TABLE IF NOT EXISTS disease_nutrient_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    disease_name TEXT NOT NULL,
    nutrient_id TEXT NOT NULL,
    rule_type TEXT NOT NULL CHECK(rule_type IN ('促进','限制')),
    description TEXT,
    UNIQUE(disease_name, nutrient_id, rule_type)
);

CREATE TABLE IF NOT EXISTS allergen_food_mapping (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    allergen_name TEXT NOT NULL,
    food_id INTEGER NOT NULL,
    UNIQUE(allergen_name, food_id)
);

CREATE TABLE IF NOT EXISTS nutrient_deficiency_foods (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    deficiency_name TEXT NOT NULL,
    food_id INTEGER NOT NULL,
    priority INTEGER NOT NULL DEFAULT 3 CHECK(priority BETWEEN 1 AND 5),
    UNIQUE(deficiency_name, food_id)
);

CREATE TABLE IF NOT EXISTS user_recommendation_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    recipe_id INTEGER NOT NULL,
    recipe_name TEXT,
    meal_label TEXT,
    recommended_on TEXT NOT NULL DEFAULT (date('now','localtime')),
    UNIQUE(user_id, recipe_id, recommended_on, meal_label)
);

CREATE INDEX IF NOT EXISTS idx_fnr_nutrient ON food_nutrient_relations(nutrient_id);
CREATE INDEX IF NOT EXISTS idx_afm_allergen ON allergen_food_mapping(allergen_name);
CREATE INDEX IF NOT EXISTS idx_ndf_def ON nutrient_deficiency_foods(deficiency_name);
CREATE INDEX IF NOT EXISTS idx_urh_user_date ON user_recommendation_history(user_id, recommended_on);
