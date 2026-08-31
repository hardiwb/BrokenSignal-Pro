param([string]$Compiler = 'g++')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

# Notes feedback must fit even the narrower 54px slot (50px usable, 6px glyphs).
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'src/apps/notes') -Filter '*.cpp' | ForEach-Object {
    $source = Get-Content -LiteralPath $_.FullName -Raw
    foreach ($message in [regex]::Matches($source, 'showHdrMsg\("([^"\r\n]*)"\)')) {
        if ($message.Groups[1].Value.Length -gt 8) {
            throw "Notes transient message will be clipped: $($message.Groups[1].Value)"
        }
    }
}

function Get-FunctionSource([string]$RelativePath, [string]$Name) {
    $source = Get-Content -LiteralPath (Join-Path $projectRoot $RelativePath) -Raw
    $match = [regex]::Match($source, "(?m)^\s*(?:static )?(?:bool|String|void) $Name\(")
    if (-not $match.Success) { throw "Function not found: $Name" }
    $start = $source.IndexOf('{', $match.Index)
    $depth = 1
    $end = $start + 1
    while ($depth -gt 0 -and $end -lt $source.Length) {
        if ($source[$end] -eq '{') { $depth++ }
        if ($source[$end] -eq '}') { $depth-- }
        $end++
    }
    if ($depth -ne 0) { throw "Unbalanced function: $Name" }
    return $source.Substring($match.Index, $end - $match.Index)
}

$functions = @(
    Get-FunctionSource 'src/module/service/Clock.cpp' 'getDisplayTm'
    Get-FunctionSource 'src/module/service/Clock.cpp' 'getCurrentTime'
    Get-FunctionSource 'src/apps/notes/NotesModel.cpp' 'getViewedTime'
    Get-FunctionSource 'src/apps/notes/NotesModel.cpp' 'viewedDateKey'
    Get-FunctionSource 'src/apps/notes/NotesModel.cpp' 'updateViewedDateCache'
    Get-FunctionSource 'src/apps/notes/NotesModel.cpp' 'refreshViewedDate'
    Get-FunctionSource 'src/apps/notes/NotesStorage.cpp' 'parseDateKey'
    Get-FunctionSource 'src/apps/notes/NotesStorage.cpp' 'formatDateKey'
    Get-FunctionSource 'src/apps/notes/NotesModel.cpp' 'notesSendViewedDayToXteink'
)
$harness = Get-Content -LiteralPath (Join-Path $projectRoot 'tests/notes_date_regression.cpp') -Raw
$translationUnit = $harness.Replace('// INSERT_PRODUCTION_FUNCTIONS', ($functions -join "`n"))
$buildDirectory = Join-Path $projectRoot '.pio/notes-date-tests'
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
$testExecutable = Join-Path $buildDirectory 'notes-date-regression.exe'
$translationUnit | & $Compiler -std=c++17 -Wall -Wextra -I (Join-Path $projectRoot 'src') -x c++ -o $testExecutable -
if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }
& $testExecutable
if ($LASTEXITCODE -ne 0) { throw 'Notes date regression failed' }
