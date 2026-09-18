param(
    [string]$SessionId = "",
    [string]$OutDir = "sessions"
)

$ErrorActionPreference = "Stop"

$opencode = (Get-Command opencode -ErrorAction SilentlyContinue).Source
if (-not $opencode) {
    $opencode = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter "opencode.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $opencode) {
    Write-Error "Comando 'opencode' non trovato. Installalo con: winget install SST.opencode"
    exit 1
}

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

if (-not $SessionId) {
    $sessions = & $opencode session list --format json | ConvertFrom-Json
    if (-not $sessions -or $sessions.Count -eq 0) {
        Write-Error "Nessuna sessione disponibile da esportare."
        exit 1
    }

    $table = $sessions | ForEach-Object {
        [PSCustomObject]@{
            Id    = $_.id
            Title = $_.sessionTitle
            Time  = $_.timeUpdated
        }
    } | Format-Table -AutoSize | Out-String
    Write-Host "Sessioni disponibili:" -ForegroundColor Cyan
    Write-Host $table

    $SessionId = Read-Host "Inserisci l'ID della sessione da esportare (invio per l'ultima)"
    if (-not $SessionId) {
        $SessionId = ($sessions | Select-Object -First 1).id
    }
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$slug = ($SessionId -replace '[^a-zA-Z0-9]', '_') -replace '_{2,}', '_'
$outFile = Join-Path $OutDir ("{0}_{1}.json" -f $stamp, $slug)

Write-Host "Esportazione sessione '$SessionId' in $outFile ..." -ForegroundColor Cyan
& $opencode export $SessionId | Out-File -FilePath $outFile -Encoding utf8

if ($LASTEXITCODE -eq 0 -and (Test-Path $outFile)) {
    Write-Host "Esportazione completata: $outFile" -ForegroundColor Green
    Write-Host ""
    Write-Host "Prossimi passi:" -ForegroundColor Yellow
    Write-Host "  git add $OutDir"
    Write-Host "  git commit -m ""chat: export sessione $SessionId"""
    Write-Host "  git push"
} else {
    Write-Error "Export fallito (exit code $LASTEXITCODE)."
    exit 1
}