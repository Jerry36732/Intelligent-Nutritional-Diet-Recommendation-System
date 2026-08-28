$ErrorActionPreference = 'Stop'
$docPath = (Get-ChildItem 'C:\Users\ROG\Documents\System\*Project Start*.doc' | Where-Object { $_.Name -notlike '*backup*' }).FullName
$backupPath = Join-Path 'C:\Users\ROG\Documents\System' 'Project_Start_Report_backup.doc'
Copy-Item $docPath $backupPath -Force
$word = New-Object -ComObject Word.Application
$word.Visible = $false
$doc = $word.Documents.Open($docPath)
function Replace-All($find, $replace) {
  $range = $doc.Content
  $range.Find.Execute($find, $false, $false, $false, $false, $false, $true, 1, $false, $replace, 2)
}
Replace-All 'xx系统项目立项报告' '智能营养膳食推荐系统项目开题报告'
Replace-All 'yyyy-mm-dd' '2026-08-27'
Replace-All 'Project ID_INIT_002' 'NDS-2026-001'
Replace-All '项目名称：' ('项目名称：' + '智能营养膳食推荐系统（Smart Diet Recommendation System）' + "`r`n`r`n")
Replace-All '项目简介：' ('项目简介：' + '本项目是一款基于 Qt（C++）框架与 SQLite 数据库的桌面端智能膳食规划工具。系统通过内置的营养学规则引擎，根据用户的身高、体重、健康目标（减重/增肌/维持），自动生成符合每日热量与宏量营养素（蛋白质、碳水、脂肪）需求的个性化三餐食谱及食材清单。

项目定位为轻量级 MVP（最小可行产品），聚焦"输入目标 → 一键生成方案 → 查看执行清单"的核心闭环，旨在验证智能推荐在本地化（中式家常菜）场景中的实用价值。

【需求分析与用户画像】
核心用户：张明，25岁，重庆某互联网公司初级程序员，独居、经常加班。痛点：外卖油腻营养不均，自炊时不知如何选择才能增肌（当前日均蛋白质约60g，目标≥100g）。使用场景：晚间打开软件设置目标，查看明日三餐推荐与食材清单，次日采购制作。目标：3个月内健康增肌3kg，偏好中式家常菜。

【竞品差异化】
MyFitnessPal：食材库大但中式菜谱少、难一键生成三餐 → 本项目：纯本地化中式菜谱 + 一键全自动生成。
薄荷健康：推荐与广告绑定、算法不透明、无法离线 → 本项目：规则透明 + 完全离线单机。
ChatGPT/豆包：菜谱灵活但营养总量不可控 → 本项目：强制营养指标校验（热量偏差≤5%，蛋白质达标）。

【功能优先级（MoSCoW）】
P0：注册/登录、身高体重与热量目标计算、预置100+食材与30+食谱、智能三餐推荐、三餐卡片与食谱详情。
P1：食材名称搜索。
P2（延后）：偏好标签、换一餐替换功能。

【原型设计要点】
主窗口 900×700，顶部导航（首页/食材库/智能推荐），中部展示目标与 BMI，三餐卡片（早30%/午40%/晚30%热量分配），支持重新生成方案；食谱详情弹窗展示食材克数与制作步骤。' + "`r`n`r`n")
Replace-All '项目目标：' ('项目目标：' + '1. 核心功能目标：实现用户信息管理、食材库维护、基于规则引擎的智能三餐推荐。
2. 性能目标：推荐计算响应时间 ≤ 2秒，界面操作无卡顿。
3. 数据目标：预置 ≥ 100种常见食材营养数据，≥ 30道中式家常菜谱（覆盖早/中/晚）。
4. 开发周期目标：严格控制在 7个自然日内完成可演示的稳定版本。' + "`r`n`r`n")
Replace-All '系统边界：' ('系统边界：' + '【包含（In Scope）】
• 用户本地注册与登录（SQLite存储）
• 健康目标设置（减重/增肌/维持）与BMI计算
• 预置食材库浏览与名称搜索
• 基于营养规则的个性化三餐推荐
• 食谱详情展示（食材清单+制作步骤）
• 支持切换用户重新生成方案

【不包含（Out of Scope / MVP延后）】
• 多日连续膳食计划（仅提供当日方案）
• 饮食记录与历史摄入追踪
• 营养分析图表与可视化看板
• 食谱收藏与用户自定义创建食谱
• 过敏原智能过滤（仅做数据标记）
• 网络同步、云端AI大模型API调用' + "`r`n`r`n")
Replace-All '项目计划： xxxx年 xx 月 xx 日 - xxxx年 xx 月 xx  日 （计xx 月）' '项目计划： 2026年 08月 27日 - 2026年 09月 02日 （计 7天）

【7日冲刺计划】
Day 1（08/27）：环境与数据基石 — SQLite含≥100食材、≥30食谱，程序可启动。
Day 2（08/28）：用户入口闭环 — 注册登录、目标设置、热量自动计算并保存。
Day 3（08/29）：食谱与食材可视化 — 食材列表页、食谱卡片静态展示。
Day 4（08/30）：核心算法 — RecommendEngine 完成，单元测试热量±5%。
Day 5（08/31）：全流程打通 — 登录→设目标→推荐→详情弹窗。
Day 6（09/01）：集成与美化 — 食材搜索、界面微调、状态栏。
Day 7（09/02）：验收封板 — 打包exe、演示视频、答辩PPT。'
Replace-All '(average salary+ management fee)* number of staff * day= 500*1*9 = 4500' '无（教育项目课程设计，不计人力成本）'
Replace-All '(平均工资+管理费)*人员数目*day= 500*1*9 = 4500' ''
Replace-All '总计： 无' '总计： 0 元'
Replace-All '说明： 无' ('说明： ' + '说明： 本项目全部资源基于开源 Qt 框架（LGPL协议）与公有领域营养数据，无第三方付费依赖。设备、场地、外协、交通、住宿及其它费用均为无。本项目为纯软件课程设计作业，全部开发工作在个人电脑（Windows 11）上完成，不产生实际资金开销。')
Replace-All '技术风险：1.2.解决:' '技术风险：
1. 推荐算法效果不佳（营养偏差过大）：贪心算法可能只保证总热量，忽略三大营养素供能比例。
2. Qt与SQLite多线程访问冲突：界面与数据查询线程同时操作数据库易引发 QSqlDatabase 冲突。

解决：
1. 参照《中国居民膳食指南（2022）》设定硬性约束（减脂碳水供能比≤55%，增肌蛋白质≥1.8g/kg）；食谱营养由 recipe_foods 关联表食材累加计算，禁止直接填入总营养值。
2. 放弃多线程异步加载，所有数据库操作在主线程执行，界面层 setEnabled(false) 防连点。

【AI协作专项 - 技术侧】
Round 1：AI生成建表语句 → 人工将 calories 改为 REAL，增加 unit 字段。
Round 2：AI给出 Harris-Benedict 公式 → 人工加入性别参数并微调活动系数。
Round 3：AI推荐机器学习协同过滤 → 否决，改为贪心+约束校验 O(n) 轻量算法。
Round 4：LNK2019 报错 → 人工确认 .pro 添加 sql 模块并统一 MinGW 编译环境。'
Replace-All '管理风险：1.2.解决：' '管理风险：
1. 7天工期紧张，范围易蔓延（如图表、饮食记录等附加功能）。
2. AI生成的UI代码结构混乱，大量硬编码 setGeometry，难以维护。

解决：
1. 贯彻 YAGNI 原则，功能分 P0/P1/P2，每日对照冲刺计划仅处理 P0；砍掉饮食记录与收藏夹。
2. 人机分工：本人设计 QVBoxLayout/QGridLayout 架构，AI 仅生成 createRecipeCard() 内部样式；关键布局与信号映射人工标注 [HUMAN-REFINE]。'
Replace-All '其它风险：1.2.解决：:' '其它风险：
1. 健康安全与免责：个体差异可能导致推荐方案不适用。
2. AI过度依赖导致代码查错困难（野指针、复杂封装）。

解决：
1. 启动界面与详情页添加免责声明："本工具仅供辅助参考，不构成医疗建议。"
2. 能力边界隔离：自定义 QAbstractTableModel 等内存管理类纯手写；拒绝引入 Python 数据分析与 QML 动画等超出 MVP 的建议，技术选型决策权由开发者掌握。

【AI协作总结】
将 AI 定位于"超级实习生"，用于 CRUD、控件创建与信息检索；业务决策（营养阈值）、架构设计与错误边界处理由本人主导。人类负责指挥与验收，机器负责执行。'
foreach ($table in $doc.Tables) {
  $c1 = $table.Cell(1,1).Range.Text
  if ($c1 -match '模块') {
    while ($table.Rows.Count -lt 2) { $table.Rows.Add() }
    $table.Cell(2, 1).Range.Text = '环境搭建与数据库设计'
    $table.Cell(2, 2).Range.Text = 'Qt配置、SQLite建表、初始化脚本'
    $table.Cell(2, 3).Range.Text = '0.8'
    $table.Cell(2, 4).Range.Text = '含预置100条食材+30条食谱'
    while ($table.Rows.Count -lt 3) { $table.Rows.Add() }
    $table.Cell(3, 1).Range.Text = '用户管理模块'
    $table.Cell(3, 2).Range.Text = '注册登录、目标设置与BMI计算'
    $table.Cell(3, 3).Range.Text = '1.2'
    $table.Cell(3, 4).Range.Text = '连接数据库存储用户记录'
    while ($table.Rows.Count -lt 4) { $table.Rows.Add() }
    $table.Cell(4, 1).Range.Text = '数据展示模块'
    $table.Cell(4, 2).Range.Text = '食材库列表、搜索框、食谱卡片UI'
    $table.Cell(4, 3).Range.Text = '1.0'
    $table.Cell(4, 4).Range.Text = 'QListWidget与QLineEdit'
    while ($table.Rows.Count -lt 5) { $table.Rows.Add() }
    $table.Cell(5, 1).Range.Text = '智能推荐引擎'
    $table.Cell(5, 2).Range.Text = '营养规则算法、热量分配器'
    $table.Cell(5, 3).Range.Text = '1.5'
    $table.Cell(5, 4).Range.Text = '核心算法，人工精调系数'
    while ($table.Rows.Count -lt 6) { $table.Rows.Add() }
    $table.Cell(6, 1).Range.Text = '推荐结果展示'
    $table.Cell(6, 2).Range.Text = '三餐卡片动态刷新、营养标签'
    $table.Cell(6, 3).Range.Text = '1.0'
    $table.Cell(6, 4).Range.Text = '布局管理、信号槽绑定'
    while ($table.Rows.Count -lt 7) { $table.Rows.Add() }
    $table.Cell(7, 1).Range.Text = '食谱详情弹窗'
    $table.Cell(7, 2).Range.Text = '模态对话框、食材与步骤渲染'
    $table.Cell(7, 3).Range.Text = '0.8'
    $table.Cell(7, 4).Range.Text = 'QTextEdit或QLabel排版'
    while ($table.Rows.Count -lt 8) { $table.Rows.Add() }
    $table.Cell(8, 1).Range.Text = '系统集成与界面打磨'
    $table.Cell(8, 2).Range.Text = '导航切换、窗口自适应'
    $table.Cell(8, 3).Range.Text = '0.5'
    $table.Cell(8, 4).Range.Text = '提升用户体验'
    while ($table.Rows.Count -lt 9) { $table.Rows.Add() }
    $table.Cell(9, 1).Range.Text = '测试与Bug修复'
    $table.Cell(9, 2).Range.Text = '端到端测试、边界Case'
    $table.Cell(9, 3).Range.Text = '0.7'
    $table.Cell(9, 4).Range.Text = '极端身高体重、空数据等'
    while ($table.Rows.Count -lt 10) { $table.Rows.Add() }
    $table.Cell(10, 1).Range.Text = '文档与答辩准备'
    $table.Cell(10, 2).Range.Text = '开题报告、演示脚本'
    $table.Cell(10, 3).Range.Text = '0.5'
    $table.Cell(10, 4).Range.Text = '整理AI协作记录'
    while ($table.Rows.Count -lt 11) { $table.Rows.Add() }
    $table.Cell(11, 1).Range.Text = '总工作量（人天）'
    $table.Cell(11, 2).Range.Text = '合计'
    $table.Cell(11, 3).Range.Text = '8.0'
    $table.Cell(11, 4).Range.Text = '7自然日交付，预留1天缓冲'
    break
  }
}
foreach ($table in $doc.Tables) {
  $h = $table.Cell(1,1).Range.Text
  if ($h -match 'Date' -or $h -match '日期') {
    if ($table.Rows.Count -ge 2) {
      $table.Cell(2,1).Range.Text = '2026-08-27'
      $table.Cell(2,2).Range.Text = 'V1.0'
      $table.Cell(2,3).Range.Text = 'INIT-001'
      $table.Cell(2,4).Range.Text = '全部章节'
      $table.Cell(2,5).Range.Text = '基于MVP创建初始版本，完成需求分析、原型设计、任务拆解与AI协作规划'
      $table.Cell(2,6).Range.Text = '自己的名字'
    }
    break
  }
}
$doc.Save()
$doc.Close()
$word.Quit()
Write-Host 'Done'