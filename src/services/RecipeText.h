#ifndef RECIPETEXT_H
#define RECIPETEXT_H

#include <QString>

namespace RecipeText {

/** 去掉「的做法」等后缀，统一为「酸菜鱼」「马蹄糕」 */
QString normalizeName(QString name);

} // namespace RecipeText

#endif // RECIPETEXT_H
