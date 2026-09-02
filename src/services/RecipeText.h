#ifndef RECIPETEXT_H
#define RECIPETEXT_H

#include <QString>

namespace RecipeText {

/** 去掉「的做法」等后缀，统一为「酸菜鱼」「马蹄糕」 */
QString normalizeName(QString name);

/**
 * 清理原始食谱中粘在食材名后的数量/单位，例如
 * “植物油公斤”“香葱3棵”“精盐1匙”。原始文本仍保留在 source_text。
 */
QString normalizeIngredientName(QString name);

} // namespace RecipeText

#endif // RECIPETEXT_H
