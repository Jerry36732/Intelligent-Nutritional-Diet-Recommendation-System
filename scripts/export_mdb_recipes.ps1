param(
    [string]$MdbPath = "C:\Users\ROG\Documents\System\食谱数据\菜谱数据库(5000多种菜)\菜谱数据库.mdb",
    [string]$OutputPath = "C:\Users\ROG\Documents\System\食谱数据\mdb_recipes_raw.json"
)

$connection = New-Object System.Data.OleDb.OleDbConnection(
    "Provider=Microsoft.ACE.OLEDB.12.0;Data Source=$MdbPath;Persist Security Info=False;"
)
$connection.Open()
try {
    function Read-Table([string]$sql) {
        $command = $connection.CreateCommand()
        $command.CommandText = $sql
        $adapter = New-Object System.Data.OleDb.OleDbDataAdapter($command)
        $table = New-Object System.Data.DataTable
        [void]$adapter.Fill($table)
        $rows = New-Object System.Collections.Generic.List[object]
        foreach ($row in $table.Rows) {
            $item = [ordered]@{}
            foreach ($column in $table.Columns) {
                $value = $row[$column.ColumnName]
                $item[$column.ColumnName] = if ($value -is [DBNull]) { $null } else { $value }
            }
            $rows.Add([pscustomobject]$item)
        }
        return $rows
    }

    $payload = [ordered]@{
        exported_at = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
        source = $MdbPath
        recipes = @(Read-Table "SELECT * FROM [菜谱] ORDER BY [菜谱ID]")
        types = @(Read-Table "SELECT * FROM [类型树] ORDER BY [节点ID]")
    }
    $payload | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath -Encoding utf8
    Write-Output "exported=$($payload.recipes.Count); output=$OutputPath"
}
finally {
    $connection.Close()
}
