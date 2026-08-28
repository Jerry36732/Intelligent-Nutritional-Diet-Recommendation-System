-- 多维健康档案：users 表扩展（可重复执行；已存在列会失败，应用内 ensureColumn 更稳妥）
-- 参考：《基于人工智能知识库的营养膳食推荐系统研究》用户档案模型

ALTER TABLE users ADD COLUMN dietary_choices TEXT DEFAULT '[]';
ALTER TABLE users ADD COLUMN food_intolerances TEXT DEFAULT '[]';
ALTER TABLE users ADD COLUMN nutritional_deficiencies TEXT DEFAULT '[]';
ALTER TABLE users ADD COLUMN allergies TEXT DEFAULT '[]';
ALTER TABLE users ADD COLUMN medical_conditions TEXT DEFAULT '[]';
