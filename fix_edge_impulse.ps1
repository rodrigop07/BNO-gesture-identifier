# Este script corrige os problemas de arquivos do Edge Impulse no Windows
Write-Host "Iniciando correcoes do Edge Impulse..." -ForegroundColor Cyan

$basePath = $PSScriptRoot

# 1. Apagar pastas inuteis de porting
$portingPath = Join-Path $basePath "components\edge-impulse-sdk\porting"
if (Test-Path $portingPath) {
    Write-Host "1. Removendo pastas de arquiteturas inuteis (Arduino, Zephyr, etc)..."
    Get-ChildItem -Path $portingPath | Where-Object { $_.Name -notmatch '^espressif$' -and $_.Name -notmatch '\.h$' -and $_.Name -notmatch '^\.clang-format$' } | Remove-Item -Recurse -Force
}

# 2. Atualizar data de modificacao para o horario local
Write-Host "2. Sincronizando o fuso horario dos arquivos (evitando loop do CMake)..."
$folders = @("components\edge-impulse-sdk", "components\model-parameters", "components\tflite-model")
foreach ($folder in $folders) {
    $fullPath = Join-Path $basePath $folder
    if (Test-Path $fullPath) {
        Get-ChildItem -Path $fullPath -Recurse -Force | ForEach-Object { $_.LastWriteTime = (Get-Date) }
    }
}

# 3. Excluir a pasta de build antiga para nao dar conflito
$buildPath = Join-Path $basePath "build"
if (Test-Path $buildPath) {
    Write-Host "3. Removendo a pasta build/ antiga..."
    Remove-Item -Path $buildPath -Recurse -Force
}

Write-Host "Tudo pronto! Você ja pode rodar 'idf.py build'." -ForegroundColor Green
