param([string]$Compiler = 'g++', [string]$ReceiverRoot = 'D:/ESP32-Projects/CrossInk-Sticky')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$senderHeaders = Join-Path $projectRoot 'src/module/service'
$receiverHeaders = Join-Path $ReceiverRoot 'src/features/sticky_notes'
$senderProtocol = Get-Content -LiteralPath (Join-Path $senderHeaders 'StickyNoteProtocol.h') -Raw
$receiverProtocol = Get-Content -LiteralPath (Join-Path $receiverHeaders 'StickyNoteProtocol.h') -Raw
if ($senderProtocol.Replace("`r`n", "`n") -ne $receiverProtocol.Replace("`r`n", "`n")) {
    throw 'Sender/receiver protocol headers differ'
}
$buildDirectory = Join-Path $projectRoot '.pio/notes-date-tests'
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
foreach ($headers in @($senderHeaders, $receiverHeaders)) {
    $testExecutable = Join-Path $buildDirectory 'sticky-chunk-regression.exe'
    & $Compiler -std=c++17 -Wall -Wextra -Werror -I $headers (Join-Path $projectRoot 'tests/sticky_chunk_regression.cpp') -o $testExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Chunk test compilation failed' }
    & $testExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Chunk regression failed' }
}
