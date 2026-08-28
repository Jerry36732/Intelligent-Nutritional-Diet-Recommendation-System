cmake_minimum_required(VERSION 3.19)
project(System LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network)

qt_standard_project_setup()

set(SMART_DIET_SOURCES
    src/main.cpp

    src/entities/User.cpp

    src/dao/DatabaseManager.cpp
    src/dao/UserDAO.cpp
    src/dao/FoodDAO.cpp
    src/dao/RecipeDAO.cpp

    src/dao/RecommendationHistoryDAO.cpp

    src/services/UserService.cpp
    src/services/AuthUtils.cpp
    src/services/RecipeText.cpp
    src/services/AiAssistantService.cpp

    src/engine/ScoreCalculator.cpp
    src/engine/RecommendEngine.cpp
    src/engine/RDSSEngine.cpp
    src/engine/NPGenerator.cpp
    src/engine/KnowledgeBase.cpp
    src/engine/DiversityFilter.cpp

    src/ui/RecipeCard.cpp
    src/ui/LogoWidget.cpp
    src/ui/RecipeDetailDialog.cpp
    src/ui/LoginDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/TagChipGroup.cpp
    src/ui/FoodSearchWidget.cpp
    src/ui/RecommendWidget.cpp
    src/ui/ProfileWidget.cpp
    src/ui/DashboardWidget.cpp
    src/ui/MainWindow.cpp
)

set(SMART_DIET_HEADERS
    src/entities/User.h
    src/entities/Food.h
    src/entities/Recipe.h
    src/entities/RecommendResult.h

    src/dao/DatabaseManager.h
    src/dao/UserDAO.h
    src/dao/FoodDAO.h
    src/dao/RecipeDAO.h

    src/services/UserService.h
    src/services/AiAssistantService.h

    src/engine/ScoreCalculator.h
    src/engine/RecommendEngine.h
    src/engine/RDSSEngine.h
    src/engine/NPGenerator.h
    src/engine/ScoredRecipe.h
    src/engine/KnowledgeBase.h
    src/engine/DiversityFilter.h

    src/dao/RecommendationHistoryDAO.h

    src/ui/RecipeCard.h
    src/ui/LogoWidget.h
    src/ui/RecipeDetailDialog.h
    src/ui/LoginDialog.h
    src/ui/SettingsDialog.h
    src/ui/TagChipGroup.h
    src/ui/FoodSearchWidget.h
    src/ui/RecommendWidget.h
    src/ui/ProfileWidget.h
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

# 复制营养数据库到构建输出目录
add_custom_command(TARGET System POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:System>/data
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/data/diet.db
        $<TARGET_FILE_DIR:System>/data/diet.db
    COMMENT "Copying diet.db to output directory"
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
