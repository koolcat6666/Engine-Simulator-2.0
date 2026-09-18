param(
    [Parameter(Mandatory = $true)]
    [string]$File
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

if (-not (Test-Path $File)) {
    Write-Error "File non trovato: $File"
    exit 1
}

Write-Host "Importazione sessione da $File ..." -ForegroundColor Cyan
& $opencode import $File

if ($LASTEXITCODE -eq 0) {
    Write-Host "Importazione completata." -ForegroundColor Green
    Write-Host ""
    Write-Host "Per riprendere la sessione importata apri opencode e usa /sessions, oppure:" -ForegroundColor Yellow
    Write-Host "  opencode --continue"
} else {
    Write-Error "Import fallito (exit code $LASTEXITCODE)."
    exit 1
}