# Compila o micrU e executa os casos de teste no Windows.
# Saidas geradas em build\testes\ e comparadas com testes\esperado\.
Set-Location (Join-Path $PSScriptRoot "..")

gcc -std=c99 -Wall -Wextra -Iinclude src\*.c -o micru.exe
if ($LASTEXITCODE -ne 0) { Write-Host "Falha na compilacao."; exit 2 }

New-Item -ItemType Directory -Force build\testes | Out-Null
$extensoes = @("pre", "lex", "ts", "lex.err", "sin.err", "dot")
$aprovados = 0
$reprovados = 0

foreach ($tipo in @("validos", "invalidos")) {
    $codigoEsperado = if ($tipo -eq "validos") { 0 } else { 1 }

    foreach ($f in Get-ChildItem "testes\$tipo\*.asm") {
        $nome = "$($tipo)_$($f.BaseName)"
        $prefixo = "build/testes/$nome"
        .\micru.exe "testes/$tipo/$($f.Name)" $prefixo | Out-Null
        $codigo = $LASTEXITCODE
        $falhas = @()
        if ($codigo -ne $codigoEsperado) { $falhas += "codigo $codigo" }

        foreach ($ext in $extensoes) {
            $gerado = [IO.File]::ReadAllText("$PWD\$prefixo.$ext") -replace "`r", ""
            $esperado = [IO.File]::ReadAllText("$PWD\testes\esperado\$nome.$ext") -replace "`r", ""
            if ($gerado -ne $esperado) { $falhas += ".$ext" }
        }

        if ($falhas.Count -eq 0) { $aprovados++; Write-Host "[OK]    $nome" }
        else { $reprovados++; Write-Host "[FALHA] $nome: $($falhas -join ' ')" }

        if ((Get-Item "$prefixo.dot").Length -gt 0 -and (Get-Command dot -ErrorAction SilentlyContinue)) {
            dot -Tpng "$prefixo.dot" -o "$prefixo.png"
        }
    }
}

Write-Host ""
Write-Host "Aprovados: $aprovados  Reprovados: $reprovados"
if ($reprovados -gt 0) { exit 1 }
