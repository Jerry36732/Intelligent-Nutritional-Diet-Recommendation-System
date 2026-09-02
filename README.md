<div align="center">
  <img src="resources/branding/v5/shanheng-logo-horizontal.svg" alt="膳衡 Smart Diet" width="420">
  <h1>膳衡 · 智能营养膳食推荐系统</h1>
  <p>Smart Diet — Intelligent Nutritional Diet Recommendation System</p>
  <p><a href="#中文说明">中文</a> · <a href="#english">English</a></p>
</div>

---

## 中文说明

### 项目简介

“膳衡”是一款基于 Qt 6 与 SQLite 开发的桌面端智能营养膳食推荐系统。系统将用户健康档案、饮食偏好、过敏与不耐受信息、冰箱库存、实际饮食记录及运动健康数据统一起来，生成可解释、可调整的个性化三餐方案。

项目内置约 2,844 条食材营养数据和 593 道食谱，并支持 AI 食物识别、双照片份量校准、食谱 DNA 改造、风味对比、动态热量目标、网页食谱导入和智能购物清单等功能。

### 核心功能

- **个性化每日方案**：根据目标、身体数据、饮食偏好、过敏原和营养约束生成早餐、午餐与晚餐。
- **动态每日目标**：分析最近 7～30 天的实际摄入、体重、步数、活动消耗和睡眠数据，避免机械套用固定热量目标。
- **饮食记录与分析**：保存每日饮食，汇总热量和宏量营养素，查看趋势；误记内容可删除。
- **多角度菜品识别**：支持俯视图与侧视图，并可结合餐盘或银行卡等尺寸参照物，输出份量区间、推荐值和置信度。
- **食材与食物识别**：识别食材名称、特点、用途及主要营养信息，并可将结果加入冰箱库存或饮食记录。
- **冰箱库存管理**：按保质期批次管理库存，提示即将过期食材，并依据库存生成清冰箱食谱。
- **食谱大全与个人食谱**：支持分类、搜索、收藏、编辑、手动录入和网页导入。
- **受限网页导入**：优先读取网页结构化食谱；遇到登录或滑块验证时，可在应用内完成验证后提取，或粘贴正文解析。
- **食谱 DNA 改造**：按减脂、增蛋白、少油少盐或素食替换等目标改造食谱，同时校验原料、步骤和营养结果。
- **风味对比**：从实际原料与制作步骤估算甜、酸、咸、辣、鲜、香、酥脆和软糯八个维度，并比较改造前后的相似度和变化。
- **智能购物清单**：汇总三餐所需原料、扣除冰箱库存并排除常备调味料，可导出 PDF、Word、Excel 或文本文件。
- **健康数据导入**：支持 Apple Health 导出 XML、Android Health Connect JSON 和规范化 CSV。
- **AI 营养助手**：结合当前方案和用户上下文回答问题，并在用户明确修改偏好或忌口时更新方案。

### 技术架构

```text
Qt Widgets 界面层
        │
        ├── 业务服务：AI 识别、健康同步、动态目标、购物清单、网页导入
        ├── 推荐引擎：RDSS、NAct、NPGenerator、多样性过滤、清冰箱推荐
        └── DAO 数据访问层
                    │
                 SQLite
```

主要技术：

- C++17
- Qt 6.5+：Core、Widgets、Sql、Network、Svg、WebEngineWidgets
- SQLite / Qt SQL
- CMake 3.19+
- SiliconFlow 云端模型或 Ollama 本地模型

### 快速开始

#### 环境要求

- Windows 10/11
- Visual Studio 2022（MSVC x64）
- Qt 6.5 或更高版本，并安装上述 Qt 模块
- CMake 3.19 或更高版本

#### 获取并构建

```powershell
git clone https://github.com/Jerry36732/Intelligent-Nutritional-Diet-Recommendation-System.git
cd Intelligent-Nutritional-Diet-Recommendation-System

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```

请把 `C:/Qt/6.x.x/msvc2022_64` 替换为本机 Qt 安装目录。也可以直接使用 Qt Creator 打开根目录的 `CMakeLists.txt`，选择 Qt 6 MSVC 套件后构建。

#### 运行

```powershell
./build/Release/System.exe
```

公开仓库中的数据库不包含任何用户资料。首次运行时请注册新账号。构建过程只会在目标目录中不存在数据库时部署 `data/diet.db`，不会覆盖已有用户数据。

### AI 配置

AI 功能是可选的。复制配置模板：

```powershell
Copy-Item data/ai_config.example.json data/ai_config.json
```

然后在 `data/ai_config.json` 中填写 SiliconFlow API Key 和模型名称。也可以设置环境变量：

```powershell
$env:SILICONFLOW_API_KEY="your-api-key"
```

如未提供云端密钥，系统会尝试访问本机 Ollama 服务。`data/ai_config.json` 已加入 `.gitignore`，请勿提交真实密钥。

### 健康数据说明

当前桌面版通过文件导入健康数据：

- Apple Health：导出的 XML 文件
- Android Health Connect：导出的 JSON 文件
- 通用格式：规范化 CSV 文件

系统读取步数、活动消耗、体重和睡眠数据，用于动态目标分析。健康数据仅保存在本地 SQLite 数据库中。

### 测试

项目包含 9 个回归测试程序，覆盖推荐约束、方案生成、收藏、AI 语义、购物清单、健康智能、图像结构化结果和网页食谱导入。

构建后可在输出目录运行测试。以下两个测试需要传入基础数据库路径：

```powershell
./build/Release/test_user_favorites.exe ./data/diet.db
./build/Release/test_feature_pack.exe ./data/diet.db
```

网页验证窗体测试使用：

```powershell
$env:QTWEBENGINE_DISABLE_SANDBOX="1"
./build/Release/test_web_recipe_dialog.exe --self-test
```

### 项目结构

```text
cmake/       数据库部署辅助脚本
data/        SQLite 基础数据库、SQL 与 AI 配置模板
resources/   QSS、品牌资源、图标和内置图片
scripts/     数据整理与食谱维护工具
src/
  dao/       数据访问层
  engine/    推荐与规则引擎
  entities/  领域实体
  services/  AI、健康、购物清单与导入服务
  ui/        Qt Widgets 界面
tests/       回归测试程序
```

### 隐私与安全

- 公开数据库不包含账号、密码哈希、收藏、饮食日志或健康记录。
- API Key 只应保存在被忽略的 `data/ai_config.json` 或环境变量中。
- 用户数据默认保存在本机，不会由项目代码自动上传到 GitHub。
- AI 识别和助手功能在选择云端提供商时会把相应请求发送给该服务；如需完全本地处理，请使用 Ollama。

---

## English

### Overview

**Smart Diet (膳衡)** is a Qt 6 and SQLite desktop application for intelligent nutrition and meal planning. It combines a user's health profile, dietary preferences, allergies and intolerances, refrigerator inventory, actual food logs, and activity data to produce explainable and adjustable daily meal plans.

The project ships with approximately 2,844 food records and 593 recipes. It also provides AI-assisted food recognition, multi-angle portion calibration, recipe DNA transformation, flavor comparison, adaptive calorie targets, web recipe import, and smart shopping-list generation.

### Key Features

- **Personalized daily plans** based on goals, body metrics, preferences, allergens, and nutritional constraints.
- **Adaptive daily targets** derived from 7–30 days of intake, weight, steps, active calories, and sleep instead of a rigid calorie limit.
- **Food logging and analytics** with daily nutrition totals, trends, and deletion of incorrect records.
- **Multi-angle meal recognition** using top and side photos plus optional size references, returning a portion range, recommended estimate, and confidence score.
- **Ingredient and food recognition** for names, characteristics, common uses, and nutrition highlights.
- **Refrigerator inventory** with expiry batches, expiry warnings, and inventory-based recipe suggestions.
- **Recipe library and personal recipes** with browsing, search, favorites, editing, manual entry, and web import.
- **Restricted-page import fallback** through an in-app verification browser or pasted-page text parsing.
- **Recipe DNA transformation** for lower fat, higher protein, lower oil/salt, or vegetarian substitutions, with structured result validation.
- **Flavor comparison** across sweetness, sourness, saltiness, spiciness, umami, aroma, crispness, and softness based on actual ingredients and cooking steps.
- **Smart shopping lists** that aggregate meal ingredients, subtract refrigerator stock, exclude common pantry seasonings, and export to PDF, Word, Excel, or text.
- **Health-data import** from Apple Health XML, Android Health Connect JSON, or normalized CSV.
- **AI nutrition assistant** that answers contextual questions and regenerates plans only when the user explicitly changes preferences or restrictions.

### Architecture

```text
Qt Widgets UI
      │
      ├── Services: AI, health sync, adaptive targets, shopping lists, web import
      ├── Engines: RDSS, NAct, NPGenerator, diversity and fridge-clear logic
      └── DAO persistence layer
                    │
                 SQLite
```

Technology stack:

- C++17
- Qt 6.5+: Core, Widgets, Sql, Network, Svg, and WebEngineWidgets
- SQLite / Qt SQL
- CMake 3.19+
- SiliconFlow cloud models or local Ollama models

### Quick Start

#### Requirements

- Windows 10/11
- Visual Studio 2022 with the x64 MSVC toolchain
- Qt 6.5 or newer with the modules listed above
- CMake 3.19 or newer

#### Clone and Build

```powershell
git clone https://github.com/Jerry36732/Intelligent-Nutritional-Diet-Recommendation-System.git
cd Intelligent-Nutritional-Diet-Recommendation-System

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```

Replace `C:/Qt/6.x.x/msvc2022_64` with your local Qt installation. Alternatively, open the root `CMakeLists.txt` in Qt Creator and select a Qt 6 MSVC kit.

#### Run

```powershell
./build/Release/System.exe
```

The public database contains no user profile data. Create a new account on first launch. The build deploys `data/diet.db` only when the destination database does not already exist, so existing user data is preserved.

### AI Configuration

AI features are optional. Copy the configuration template:

```powershell
Copy-Item data/ai_config.example.json data/ai_config.json
```

Add your SiliconFlow API key and model names to `data/ai_config.json`, or set the environment variable:

```powershell
$env:SILICONFLOW_API_KEY="your-api-key"
```

When no cloud key is configured, the application attempts to use a local Ollama service. The real `data/ai_config.json` file is ignored by Git and must never be committed.

### Health Data

The desktop application currently imports health data from files:

- Apple Health exported XML
- Android Health Connect exported JSON
- Normalized CSV

Steps, active calories, weight, and sleep are used for adaptive-target analysis. Imported health records remain in the local SQLite database.

### Tests

Nine regression executables cover recommendation constraints, plan generation, favorites, AI preference semantics, shopping lists, health intelligence, structured vision results, and web recipe import.

Two tests require the source database path:

```powershell
./build/Release/test_user_favorites.exe ./data/diet.db
./build/Release/test_feature_pack.exe ./data/diet.db
```

Run the WebEngine dialog self-test with:

```powershell
$env:QTWEBENGINE_DISABLE_SANDBOX="1"
./build/Release/test_web_recipe_dialog.exe --self-test
```

### Repository Layout

```text
cmake/       Database deployment helper
data/        SQLite seed database, SQL files, and AI configuration template
resources/   QSS theme, branding, icons, and bundled images
scripts/     Data preparation and recipe maintenance utilities
src/
  dao/       Data-access layer
  engine/    Recommendation and rule engines
  entities/  Domain entities
  services/  AI, health, shopping-list, and import services
  ui/        Qt Widgets interface
tests/       Regression test executables
```

### Privacy and Security

- The public seed database contains no accounts, password hashes, favorites, food logs, or health records.
- Keep API keys only in the ignored `data/ai_config.json` file or environment variables.
- User data is stored locally and is never uploaded to GitHub by the application.
- Cloud AI features send relevant requests to the selected provider. Use Ollama when fully local processing is required.
