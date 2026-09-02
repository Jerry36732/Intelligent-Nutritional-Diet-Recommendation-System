cmake_minimum_required(VERSION 3.19)
project(System LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network Svg WebEngineWidgets)

qt_standard_project_setup()

set(SMART_DIET_SOURCES
    src/main.cpp

    src/entities/User.cpp

    src/dao/DatabaseManager.cpp
    src/dao/UserDAO.cpp
    src/dao/FoodDAO.cpp
    src/dao/RecipeDAO.cpp

    src/dao/RecommendationHistoryDAO.cpp
    src/dao/FridgeDAO.cpp
    src/dao/PersonalRecipeDAO.cpp
    src/dao/FoodLogDAO.cpp
    src/dao/HealthDataDAO.cpp

    src/services/UserService.cpp
    src/services/AuthUtils.cpp
    src/services/RecipeText.cpp
    src/services/RecipeImageProvider.cpp
    src/services/AiAssistantService.cpp
    src/services/ShoppingListService.cpp
    src/services/ShoppingListExportService.cpp
    src/services/IngredientMeasureService.cpp
    src/services/NutritionAiService.cpp
    src/services/HealthDataSyncService.cpp
    src/services/AdaptiveTargetService.cpp
    src/services/FlavorFingerprintService.cpp
    src/services/WebRecipeImportService.cpp

    src/engine/ScoreCalculator.cpp
    src/engine/RecommendEngine.cpp
    src/engine/RDSSEngine.cpp
    src/engine/NPGenerator.cpp
    src/engine/KnowledgeBase.cpp
    src/engine/DiversityFilter.cpp
    src/engine/FridgeClearEngine.cpp

    src/ui/RecipeCard.cpp
    src/ui/LogoWidget.cpp
    src/ui/UiAssets.cpp
    src/ui/RecipeDetailDialog.cpp
    src/ui/LoginDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/TagChipGroup.cpp
    src/ui/FoodSearchWidget.cpp
    src/ui/FoodDetailDialog.cpp
    src/ui/RecommendWidget.cpp
      src/ui/FridgeWidget.cpp
      src/ui/FridgeVisionDialog.cpp
    src/ui/ShoppingListDialog.cpp
    src/ui/RecipeEditorDialog.cpp
    src/ui/ProfileWidget.cpp
    src/ui/FavoritesWidget.cpp
    src/ui/RecipeLibraryWidget.cpp
    src/ui/FoodVisionDialog.cpp
    src/ui/IngredientVisionDialog.cpp
    src/ui/DietAnalyticsWidget.cpp
    src/ui/HealthSyncDialog.cpp
    src/ui/RecipeDnaDialog.cpp
    src/ui/DashboardWidget.cpp
    src/ui/MainWindow.cpp
)

set(SMART_DIET_HEADERS
    src/entities/User.h
    src/entities/Food.h
    src/entities/Recipe.h
    src/entities/RecommendResult.h
    src/entities/HealthData.h

    src/dao/DatabaseManager.h
    src/dao/UserDAO.h
    src/dao/FoodDAO.h
    src/dao/RecipeDAO.h

    src/services/UserService.h
    src/services/AiAssistantService.h
    src/services/RecipeImageProvider.h
    src/services/ShoppingListService.h
    src/services/ShoppingListExportService.h
    src/services/IngredientMeasureService.h
    src/services/NutritionAiService.h
    src/services/HealthDataSyncService.h
    src/services/AdaptiveTargetService.h
    src/services/FlavorFingerprintService.h
    src/services/WebRecipeImportService.h

    src/engine/ScoreCalculator.h
    src/engine/RecommendEngine.h
    src/engine/RDSSEngine.h
    src/engine/NPGenerator.h
    src/engine/ScoredRecipe.h
    src/engine/KnowledgeBase.h
    src/engine/DiversityFilter.h
    src/engine/FridgeClearEngine.h

    src/entities/FridgeItem.h

    src/dao/RecommendationHistoryDAO.h
    src/dao/FridgeDAO.h
    src/dao/PersonalRecipeDAO.h
    src/dao/FoodLogDAO.h
    src/dao/HealthDataDAO.h

    src/ui/RecipeCard.h
    src/ui/LogoWidget.h
    src/ui/UiAssets.h
    src/ui/RecipeDetailDialog.h
    src/ui/LoginDialog.h
    src/ui/SettingsDialog.h
    src/ui/TagChipGroup.h
    src/ui/HealthProfileOptions.h
    src/ui/FoodSearchWidget.h
    src/ui/FoodDetailDialog.h
    src/ui/RecommendWidget.h
      src/ui/FridgeWidget.h
      src/ui/FridgeVisionDialog.h
    src/ui/ShoppingListDialog.h
    src/ui/RecipeEditorDialog.h
    src/ui/ProfileWidget.h
    src/ui/FavoritesWidget.h
    src/ui/RecipeLibraryWidget.h
    src/ui/FoodVisionDialog.h
    src/ui/IngredientVisionDialog.h
    src/ui/DietAnalyticsWidget.h
    src/ui/HealthSyncDialog.h
    src/ui/RecipeDnaDialog.h
    src/ui/MainWindow.h
)

qt_add_executable(System
    WIN32 MACOSX_BUNDLE
    ${SMART_DIET_SOURCES}
    ${SMART_DIET_HEADERS}
    resources/resources.qrc
)

target_include_directories(System PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(System
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Sql
        Qt::Network
        Qt::Svg
        Qt::WebEngineWidgets
)

# RDSS 单元测试（虚拟用户过敏过滤）
qt_add_executable(test_rdss
    tests/test_rdss.cpp
    src/engine/RDSSEngine.cpp
    src/engine/ScoreCalculator.cpp
    src/entities/User.cpp
    src/dao/DatabaseManager.cpp
)
target_include_directories(test_rdss PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_rdss PRIVATE Qt::Core Qt::Sql)

qt_add_executable(test_np_generator
    tests/test_np_generator.cpp
    src/engine/NPGenerator.cpp
    src/engine/ScoreCalculator.cpp
    src/engine/DiversityFilter.cpp
    src/entities/User.cpp
    src/services/UserService.cpp
    src/services/AuthUtils.cpp
    src/dao/UserDAO.cpp
    src/dao/DatabaseManager.cpp
)
target_include_directories(test_np_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_np_generator PRIVATE Qt::Core Qt::Sql)

# 用户默认值与统一收藏持久化回归测试（在数据库副本上运行）。
qt_add_executable(test_user_favorites
    tests/test_user_favorites.cpp
    src/entities/User.cpp
    src/services/AuthUtils.cpp
    src/services/RecipeText.cpp
    src/dao/DatabaseManager.cpp
    src/dao/UserDAO.cpp
    src/dao/RecipeDAO.cpp
    src/dao/FoodDAO.cpp
)
target_include_directories(test_user_favorites PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_user_favorites PRIVATE Qt::Core Qt::Sql)

# AI 偏好语义回归：明确“不喜欢/不吃”必须本地处理并避开冲突食材。
qt_add_executable(test_ai_assistant
    tests/test_ai_assistant.cpp
    src/entities/User.cpp
    src/services/AiAssistantService.cpp
)
target_include_directories(test_ai_assistant PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_ai_assistant PRIVATE Qt::Core Qt::Network)

# 智能购物清单、临期库存和个人食谱持久化回归测试。
qt_add_executable(test_feature_pack
    tests/test_feature_pack.cpp
    src/dao/DatabaseManager.cpp
    src/dao/FridgeDAO.cpp
    src/dao/FoodLogDAO.cpp
    src/dao/HealthDataDAO.cpp
    src/dao/PersonalRecipeDAO.cpp
    src/dao/RecipeDAO.cpp
    src/services/RecipeText.cpp
    src/services/RecipeImageProvider.cpp
    src/services/ShoppingListService.cpp
    src/services/ShoppingListExportService.cpp
    src/services/IngredientMeasureService.cpp
    src/services/AdaptiveTargetService.cpp
    src/services/HealthDataSyncService.cpp
    src/services/FlavorFingerprintService.cpp
    src/services/UserService.cpp
    src/services/AuthUtils.cpp
    src/dao/UserDAO.cpp
    src/entities/User.cpp
)
target_include_directories(test_feature_pack PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_feature_pack PRIVATE Qt::Core Qt::Gui Qt::Sql)

# 多模态识别与食谱 DNA 结构化输出回归测试（不访问网络）。
qt_add_executable(test_nutrition_ai
    tests/test_nutrition_ai.cpp
    src/services/NutritionAiService.cpp
    src/services/FlavorFingerprintService.cpp
    src/dao/DatabaseManager.cpp
)
target_include_directories(test_nutrition_ai PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_nutrition_ai PRIVATE Qt::Core Qt::Gui Qt::Network Qt::Sql)

qt_add_executable(test_health_intelligence
    tests/test_health_intelligence.cpp
    src/entities/User.cpp
    src/dao/DatabaseManager.cpp
    src/dao/FoodLogDAO.cpp
    src/dao/HealthDataDAO.cpp
    src/dao/UserDAO.cpp
    src/services/AuthUtils.cpp
    src/services/UserService.cpp
    src/services/AdaptiveTargetService.cpp
    src/services/HealthDataSyncService.cpp
    src/services/FlavorFingerprintService.cpp
)
target_include_directories(test_health_intelligence PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_health_intelligence PRIVATE Qt::Core Qt::Sql)

# 网页食谱导入解析与登录/验证码降级回归测试（不访问网络）。
qt_add_executable(test_web_recipe_import
    tests/test_web_recipe_import.cpp
    src/services/WebRecipeImportService.cpp
)
target_include_directories(test_web_recipe_import PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_web_recipe_import PRIVATE Qt::Core Qt::Gui)

# 网页导入窗体的真实控件布局与粘贴回填冒烟测试。
qt_add_executable(test_web_recipe_dialog
    tests/test_web_recipe_dialog.cpp
    src/ui/RecipeEditorDialog.cpp
    src/ui/UiAssets.cpp
    src/services/WebRecipeImportService.cpp
    src/services/IngredientMeasureService.cpp
    src/services/ShoppingListService.cpp
    src/services/RecipeText.cpp
    src/dao/PersonalRecipeDAO.cpp
    src/dao/RecipeDAO.cpp
    src/dao/FridgeDAO.cpp
    src/dao/DatabaseManager.cpp
)
target_include_directories(test_web_recipe_dialog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_web_recipe_dialog PRIVATE
    Qt::Core Qt::Gui Qt::Widgets Qt::Sql Qt::Network Qt::Svg Qt::WebEngineWidgets)

# 复制营养数据库到构建输出目录
add_custom_command(TARGET System POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:System>/data
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DB=${CMAKE_SOURCE_DIR}/data/diet.db
        -DDEST_DB=$<TARGET_FILE_DIR:System>/data/diet.db
        -P ${CMAKE_SOURCE_DIR}/cmake/DeployDatabaseIfMissing.cmake
    COMMENT "Deploying diet.db without overwriting user data"
)

# 将经审核/检索的食谱图片作为离线资源部署到可执行文件旁。
if (EXISTS "${CMAKE_SOURCE_DIR}/食谱数据/图片工作区/assets/recipes/complete")
    add_custom_command(TARGET System POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:System>/data/recipe_images
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/食谱数据/图片工作区/assets/recipes/complete"
            $<TARGET_FILE_DIR:System>/data/recipe_images
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/食谱数据/图片工作区/完整食谱配图清单.json"
            $<TARGET_FILE_DIR:System>/data/recipe_images.json
        COMMENT "Copying offline recipe image library"
    )
endif()

# 同时复制可读 QSS，便于开发构建在资源缓存异常时仍加载最新主题。
add_custom_command(TARGET System POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/resources/styles.qss
        $<TARGET_FILE_DIR:System>/styles.qss
    COMMENT "Copying desktop theme stylesheet"
)

# 若存在本地 AI 配置则一并复制（含 API Key，勿提交仓库）
if (EXISTS "${CMAKE_SOURCE_DIR}/data/ai_config.json")
    add_custom_command(TARGET System POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/data/ai_config.json
            $<TARGET_FILE_DIR:System>/data/ai_config.json
        COMMENT "Copying ai_config.json to output directory"
    )
endif()

# 部署 Qt 运行时与 SQLite 驱动（修复 Driver not loaded）
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
if (WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET System POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            $<$<CONFIG:Debug>:--debug>
            $<$<NOT:$<CONFIG:Debug>>:--release>
            --sql
            --no-translations
            \"$<TARGET_FILE:System>\"
        COMMENT "Running windeployqt (including SQL plugins)"
    )
endif()

install(TARGETS System
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET System
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
