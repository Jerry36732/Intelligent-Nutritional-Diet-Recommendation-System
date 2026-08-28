# -*- coding: utf-8 -*-
"""Fill Project Start Report via Word COM - insert after section headers."""
import os
import subprocess

BASE = r"C:\Users\ROG\Documents\System"
CONTENT = os.path.join(BASE, "report_content")
BACKUP = os.path.join(BASE, "Project_Start_Report_backup.doc")


def read_txt(name):
    with open(os.path.join(CONTENT, name), encoding="utf-8") as f:
        return f.read().strip()


def ps_quote(s):
    return s.replace("'", "''").replace('"', '`"')


intro = read_txt("section1_intro.txt")
goals = read_txt("section1_goals.txt")
boundary = read_txt("section1_boundary.txt")
schedule = read_txt("section2_schedule.txt")
note3 = read_txt("section3_note.txt")
tech = read_txt("section4_tech.txt")
mgmt = read_txt("section4_mgmt.txt")
other = read_txt("section4_other.txt")

workload_rows = [
    ("环境搭建与数据库设计", "Qt配置、SQLite建表、初始化脚本", "0.8", "含预置100条食材+30条食谱"),
    ("用户管理模块", "注册登录、目标设置与BMI计算", "1.2", "连接数据库存储用户记录"),
    ("数据展示模块", "食材库列表、搜索框、食谱卡片UI", "1.0", "QListWidget与QLineEdit"),
    ("智能推荐引擎", "营养规则算法、热量分配器", "1.5", "核心算法，人工精调系数"),
    ("推荐结果展示", "三餐卡片动态刷新、营养标签", "1.0", "布局管理、信号槽绑定"),
    ("食谱详情弹窗", "模态对话框、食材与步骤渲染", "0.8", "QTextEdit或QLabel排版"),
    ("系统集成与界面打磨", "导航切换、窗口自适应", "0.5", "提升用户体验"),
    ("测试与Bug修复", "端到端测试、边界Case", "0.7", "极端身高体重、空数据等"),
    ("文档与答辩准备", "开题报告、演示脚本", "0.5", "整理AI协作记录"),
    ("总工作量（人天）", "合计", "8.0", "7自然日交付，预留1天缓冲"),
]

sections = {
    "项目名称：": "智能营养膳食推荐系统（Smart Diet Recommendation System）",
    "项目简介：": intro,
    "项目目标：": goals,
    "系统边界：": boundary,
}

ps = [
    "$ErrorActionPreference = 'Stop'",
    f"$src = (Get-ChildItem '{BASE}\\*Project Start*.doc' | Where-Object {{ $_.Name -notlike '*backup*' }}).FullName",
    f"$backup = '{BACKUP.replace(chr(92), '/')}'",
    f"$orig = '{BACKUP.replace(chr(92), '/')}'",
]

# Restore from backup if exists
ps.extend([
    "if (Test-Path $backup) { Copy-Item $backup $src -Force }",
    "$word = New-Object -ComObject Word.Application",
    "$word.Visible = $false",
    "$doc = $word.Documents.Open($src)",
    "function Replace-Short($find, $replace) {",
    "  $r = $doc.Content",
    "  while ($r.Find.Execute($find, $false, $false, $false, $false, $false, $true, 1, $false, $replace, 2)) {}",
    "}",
    "function Insert-AfterHeading($heading, $text) {",
    "  $r = $doc.Content",
    "  if (-not $r.Find.Execute($heading, $false, $false, $false, $false, $false, $true, 1, $false, '', 0)) { return }",
    "  $r.Collapse(0)  # wdCollapseEnd",
    "  $r.Text = \"`r`n\" + $text + \"`r`n\"",
    "}",
    "Replace-Short 'xx系统项目立项报告' '智能营养膳食推荐系统项目开题报告'",
    "Replace-Short 'yyyy-mm-dd' '2026-08-27'",
    "Replace-Short 'Project ID_INIT_002' 'NDS-2026-001'",
])

for heading, text in sections.items():
    ps.append(f"Insert-AfterHeading '{ps_quote(heading)}' '{ps_quote(text)}'")

old_schedule = "项目计划： xxxx年 xx 月 xx 日 - xxxx年 xx 月 xx  日 （计xx 月）"
ps.append(f"Replace-Short '{ps_quote(old_schedule)}' '{ps_quote(schedule)}'")

ps.append("Replace-Short '(average salary+ management fee)* number of staff * day= 500*1*9 = 4500' '无（教育项目课程设计，不计人力成本）'")
ps.append("Replace-Short '(平均工资+管理费)*人员数目*day= 500*1*9 = 4500' ''")
ps.append("Replace-Short '总计： 无' '总计： 0 元'")
ps.append(f"Replace-Short '说明： 无' '说明： {ps_quote(note3)}'")

# Section 4 - replace short placeholders with full blocks via insert after heading
ps.append(f"Insert-AfterHeading '技术风险：' '{ps_quote(tech)}'")
ps.append("Replace-Short '1.' ''")
ps.append("Replace-Short '2.' ''")
ps.append("Replace-Short '解决:' ''")
ps.append(f"Insert-AfterHeading '管理风险：' '{ps_quote(mgmt)}'")
ps.append(f"Insert-AfterHeading '其它风险：' '{ps_quote(other)}'")

# Workload table
ps.extend([
    "foreach ($table in $doc.Tables) {",
    "  $c1 = $table.Cell(1,1).Range.Text",
    "  if ($c1 -match '模块') {",
])
for i, row in enumerate(workload_rows):
    r = i + 2
    ps.append(f"    while ($table.Rows.Count -lt {r}) {{ $table.Rows.Add() }}")
    for c, val in enumerate(row, 1):
        ps.append(f"    $table.Cell({r}, {c}).Range.Text = '{ps_quote(val)}'")
ps.extend(["    break", "  }", "}"])

# Revision table
ps.extend([
    "foreach ($table in $doc.Tables) {",
    "  $h = $table.Cell(1,1).Range.Text",
    "  if ($h -match 'Date' -or $h -match '日期') {",
    "    if ($table.Rows.Count -ge 2) {",
    "      $table.Cell(2,1).Range.Text = '2026-08-27'",
    "      $table.Cell(2,2).Range.Text = 'V1.0'",
    "      $table.Cell(2,3).Range.Text = 'INIT-001'",
    "      $table.Cell(2,4).Range.Text = '全部章节'",
    "      $table.Cell(2,5).Range.Text = '基于MVP创建初始版本，完成需求分析、原型设计、任务拆解与AI协作规划'",
    "      $table.Cell(2,6).Range.Text = '自己的名字'",
    "    }",
    "    break",
    "  }",
    "}",
    "$doc.Save()",
    "$doc.Close()",
    "$word.Quit()",
    "Write-Host 'SUCCESS'",
])

ps_script = os.path.join(BASE, "fill_report_run.ps1")
with open(ps_script, "w", encoding="utf-8-sig") as f:
    f.write("\n".join(ps))

# Ensure backup exists from original
doc_path = None
for f in os.listdir(BASE):
    if "Project Start" in f and f.endswith(".doc") and "backup" not in f.lower():
        doc_path = os.path.join(BASE, f)
        break

if not os.path.exists(BACKUP) and doc_path:
    import shutil
    shutil.copy2(doc_path, BACKUP)

result = subprocess.run(
    ["powershell", "-ExecutionPolicy", "Bypass", "-File", ps_script],
    capture_output=True,
    text=True,
    encoding="utf-8",
    errors="replace",
)
with open(os.path.join(BASE, "fill_log.txt"), "w", encoding="utf-8") as log:
    log.write(f"code: {result.returncode}\nstdout: {result.stdout}\nstderr: {result.stderr}")
