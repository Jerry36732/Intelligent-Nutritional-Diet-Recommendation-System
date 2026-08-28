$ErrorActionPreference = "Stop"
$docx = "C:\Users\ROG\Documents\System\食谱数据\智能营养膳食推荐系统_食谱数据手册.docx"
$outDir = "C:\Users\ROG\Documents\System\食谱数据\rendered_word"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$pdf = Join-Path $outDir "食谱数据手册.pdf"
$word = New-Object -ComObject Word.Application
$word.Visible = $false
try {
    $doc = $word.Documents.Open($docx)
    $doc.ExportAsFixedFormat($pdf, 17)
    $doc.Close($false)
} finally {
    $word.Quit()
}
Write-Output $pdf
