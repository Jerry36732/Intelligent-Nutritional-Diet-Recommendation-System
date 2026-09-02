-- 菜品角色修正：小吃不应作为主食
UPDATE recipes
SET dish_role = 'snack'
WHERE name IN ('土豆可乐饼', '可乐土豆饼', '可乐饼')
   OR name LIKE '%可乐饼%';

UPDATE recipes
SET dish_role = 'staple'
WHERE name = '白米饭';
