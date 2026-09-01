[CmdletBinding()]
param(
    [ValidateRange(1, 65535)]
    [int]$Port = 8765,

    [string]$Database,

    [switch]$Offline,

    [switch]$Lan,

    [switch]$EnableNotion
)

$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot

$dataDirectory = Join-Path $PSScriptRoot "data"
if (-not (Test-Path -LiteralPath $dataDirectory)) {
    New-Item -ItemType Directory -Path $dataDirectory | Out-Null
}

if (-not $Database) {
    $Database = Join-Path $dataDirectory "sync-state.sqlite3"
}

$tokenPath = Join-Path $dataDirectory "cardputer-token.txt"
if ($env:CARDPUTER_SYNC_TOKEN) {
    $token = $env:CARDPUTER_SYNC_TOKEN.Trim()
    $tokenSource = "CARDPUTER_SYNC_TOKEN environment variable"
} elseif (Test-Path -LiteralPath $tokenPath) {
    $token = (Get-Content -Raw -LiteralPath $tokenPath).Trim()
    $tokenSource = $tokenPath
} else {
    $randomBytes = New-Object byte[] 32
    $generator = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    try {
        $generator.GetBytes($randomBytes)
    } finally {
        $generator.Dispose()
    }
    $token = [Convert]::ToBase64String($randomBytes)
    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($tokenPath, $token, $utf8WithoutBom)
    $tokenSource = $tokenPath
    Write-Host "Created private Cardputer token: $tokenPath"
}

if ($token.Length -lt 16) {
    throw "CARDPUTER_SYNC_TOKEN must contain at least 16 characters."
}
$env:CARDPUTER_SYNC_TOKEN = $token

$notionConfigPath = Join-Path $dataDirectory "notion.env"
if ($EnableNotion) {
    if ($Offline) {
        throw "-EnableNotion cannot be combined with -Offline."
    }
    if (Test-Path -LiteralPath $notionConfigPath) {
        foreach ($line in Get-Content -LiteralPath $notionConfigPath) {
            $trimmed = $line.Trim()
            if (-not $trimmed -or $trimmed.StartsWith("#")) {
                continue
            }
            $separator = $trimmed.IndexOf("=")
            if ($separator -le 0) {
                continue
            }
            $name = $trimmed.Substring(0, $separator).Trim()
            $value = $trimmed.Substring($separator + 1).Trim()
            if ($name -eq "NOTION_API_TOKEN") {
                $env:NOTION_API_TOKEN = $value
            } elseif ($name -eq "NOTION_DATABASE_ID") {
                $env:NOTION_DATABASE_ID = $value
            } elseif ($name -eq "NOTION_TEMPLATE_PATH") {
                $env:NOTION_TEMPLATE_PATH = $value
            } elseif ($name -eq "NOTION_ACCOUNT_RELATION_PAGE_ID") {
                $env:NOTION_ACCOUNT_RELATION_PAGE_ID = $value
            }
        }
    }
    if (-not $env:NOTION_API_TOKEN) {
        throw "NOTION_API_TOKEN is missing from $notionConfigPath or the environment."
    }
    if (-not $env:NOTION_DATABASE_ID) {
        throw "NOTION_DATABASE_ID is missing from $notionConfigPath or the environment."
    }
    if (-not $env:NOTION_TEMPLATE_PATH -or -not (Test-Path -LiteralPath $env:NOTION_TEMPLATE_PATH)) {
        throw "Notion request template is missing. Run .\configure-notion.bat first."
    }
}

$platformIoPython = if ($env:USERPROFILE) {
    Join-Path $env:USERPROFILE ".platformio\penv\Scripts\python.exe"
} else {
    $null
}
if ($platformIoPython -and (Test-Path -LiteralPath $platformIoPython)) {
    $python = $platformIoPython
} else {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if (-not $pythonCommand) {
        throw "Python was not found. Install Python 3.11+ or restore PlatformIO Python."
    }
    $python = $pythonCommand.Source
}

if ($Offline) {
    $classifier = "offline"
} else {
    $codexCommand = Get-Command codex.cmd -ErrorAction SilentlyContinue
    if (-not $codexCommand) {
        $codexCommand = Get-Command codex.exe -ErrorAction SilentlyContinue
    }
    if (-not $codexCommand) {
        $codexCommand = Get-Command codex -ErrorAction SilentlyContinue
    }
    if (-not $codexCommand) {
        throw "Codex was not found. Open a PowerShell where 'codex --version' works."
    }
    $env:CODEX_CLI_PATH = $codexCommand.Source
    $classifier = "codex"
}

$serverArguments = @(
    "-m", "expense_bridge.server",
    "--classifier", $classifier,
    "--port", $Port.ToString(),
    "--database", $Database
)

if ($Lan) {
    $serverArguments += @("--host", "0.0.0.0", "--allow-lan")
    $addresses = @(
        Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
            Where-Object {
                $_.IPAddress -ne "127.0.0.1" -and
                -not $_.IPAddress.StartsWith("169.254.") -and
                $_.AddressState -eq "Preferred"
            } |
            Select-Object -ExpandProperty IPAddress -Unique
    )
} else {
    $addresses = @("127.0.0.1")
}

if ($EnableNotion) {
    $serverArguments += "--enable-notion"
}

Write-Host "Starting Cardputer Expense Bridge"
if ($Lan) {
    Write-Host "LAN mode: enabled (trusted private networks only)"
    foreach ($address in $addresses) {
        Write-Host "Address: http://${address}:$Port"
    }
} else {
    Write-Host "Address: http://127.0.0.1:$Port"
    Write-Host "Cardputer upload: disabled until restarted with -Lan"
}
Write-Host "Classifier: $classifier"
Write-Host "Token source: $tokenSource"
if ($EnableNotion) {
    Write-Host "Notion submission: ENABLED"
    Write-Host "Notion config: $notionConfigPath"
} else {
    Write-Host "Notion submission: disabled"
}
Write-Host "Press Ctrl+C to stop."
Write-Host ""

& $python @serverArguments
exit $LASTEXITCODE
