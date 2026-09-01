[CmdletBinding()]
param(
    [switch]$Force,

    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot

$dataDirectory = if ($OutputDirectory) { $OutputDirectory } else { Join-Path $PSScriptRoot "data" }
$environmentPath = Join-Path $dataDirectory "notion.env"
$templatePath = Join-Path $dataDirectory "notion-request-template.json"

if (-not $Force -and ((Test-Path -LiteralPath $environmentPath) -or (Test-Path -LiteralPath $templatePath))) {
    $answer = Read-Host "Notion configuration already exists. Overwrite it? [y/N]"
    if ($answer -notmatch "^[Yy]$") {
        Write-Host "No files changed."
        exit 0
    }
}

Write-Host "Configure Notion Expense Sync"
Write-Host "The token is stored locally under companion\data and is not displayed."
$secureToken = Read-Host "Notion integration token" -AsSecureString
$tokenPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secureToken)
try {
    $token = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($tokenPointer).Trim()
} finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($tokenPointer)
}
if ($token.Length -lt 10) {
    throw "The Notion integration token is missing or too short."
}

$databaseId = (Read-Host "Notion expense database ID").Trim()
if (-not $databaseId -or $databaseId -match "\s") {
    throw "The Notion database ID cannot be empty or contain whitespace."
}

$accountRelationId = (Read-Host "Related Account page ID (optional; Enter to skip)").Trim()
if ($accountRelationId -and $accountRelationId -match "\s") {
    throw "The related Account page ID cannot contain whitespace."
}

New-Item -ItemType Directory -Force -Path $dataDirectory | Out-Null
$utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
$environmentText = "NOTION_API_TOKEN=$token`nNOTION_DATABASE_ID=$databaseId`nNOTION_ACCOUNT_RELATION_PAGE_ID=$accountRelationId`nNOTION_TEMPLATE_PATH=$templatePath`n"
[System.IO.File]::WriteAllText($environmentPath, $environmentText, $utf8WithoutBom)

$properties = [ordered]@{
    Name = [ordered]@{
        title = @(
            [ordered]@{
                text = [ordered]@{
                    content = "{{Title}}"
                }
            }
        )
    }
    Expense = [ordered]@{
        number = "{{Nominal}}"
    }
    "Type of Expense" = [ordered]@{
        select = [ordered]@{
            name = "{{Jenis pengeluaran}}"
        }
    }
    "Date " = [ordered]@{
        date = [ordered]@{
            start = "{{Tanggal}}"
        }
    }
}
if ($accountRelationId) {
    $properties["Account"] = [ordered]@{
        relation = @(
            [ordered]@{
                id = $accountRelationId
            }
        )
    }
}

$template = [ordered]@{
    parent = [ordered]@{
        database_id = "{{NOTION_DATABASE_ID}}"
    }
    properties = $properties
}
$templateJson = $template | ConvertTo-Json -Depth 12
[System.IO.File]::WriteAllText($templatePath, $templateJson + "`n", $utf8WithoutBom)

$token = $null
Write-Host ""
Write-Host "Created private configuration:"
Write-Host "  $environmentPath"
Write-Host "  $templatePath"
Write-Host "Both files are ignored by Git."
Write-Host "Start sync with: .\start-expense-server.bat"
