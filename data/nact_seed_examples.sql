-- NAct 知识库示例种子（DatabaseManager 也会自动灌入；本文件便于手工维护）
-- 富铁食物示例（按名称模糊匹配后由应用写入 food_id）

INSERT OR IGNORE INTO disease_nutrient_rules(disease_name, nutrient_id, rule_type, description) VALUES
('2型糖尿病','糖','限制','甜品，蛋糕，糖水，奶茶'),
('高血压','钠','限制','腌，咸鱼，腊肉'),
('高血脂','脂肪','限制','肥肉，油炸，奶油'),
('贫血','铁','促进','菠菜，猪肝，瘦红肉');

-- 推荐历史表示例结构已在 nact_knowledge.sql
