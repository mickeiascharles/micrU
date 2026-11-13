#!/bin/sh
# Executa todos os casos de teste e compara as saidas com testes/esperado/.
# Uso: sh testes/executar_testes.sh [--atualizar]

cd "$(dirname "$0")/.." || exit 2

BIN=./micru
SAIDA=build/testes
ESPERADO=testes/esperado
EXTENSOES="pre lex ts lex.err sin.err dot"

[ -x "$BIN" ] || { echo "Binario $BIN nao encontrado. Execute 'make'."; exit 2; }
mkdir -p "$SAIDA"
[ "$1" = "--atualizar" ] && mkdir -p "$ESPERADO"

aprovados=0
reprovados=0

for tipo in validos invalidos; do
    [ "$tipo" = validos ] && esperado_codigo=0 || esperado_codigo=1

    for asm in testes/"$tipo"/*.asm; do
        nome="${tipo}_$(basename "$asm" .asm)"
        "$BIN" "$asm" "$SAIDA/$nome" >/dev/null 2>&1
        codigo=$?
        falhas=""

        [ "$codigo" -eq "$esperado_codigo" ] ||
            falhas=" codigo $codigo (esperado $esperado_codigo)"

        for ext in $EXTENSOES; do
            if [ "$1" = "--atualizar" ]; then
                cp "$SAIDA/$nome.$ext" "$ESPERADO/$nome.$ext"
            elif ! cmp -s "$SAIDA/$nome.$ext" "$ESPERADO/$nome.$ext"; then
                falhas="$falhas .$ext"
            fi
        done

        if [ -z "$falhas" ]; then
            aprovados=$((aprovados + 1))
            echo "[OK]    $nome"
        else
            reprovados=$((reprovados + 1))
            echo "[FALHA] $nome:$falhas"
        fi

        if command -v dot >/dev/null 2>&1 && [ -s "$SAIDA/$nome.dot" ]; then
            dot -Tpng "$SAIDA/$nome.dot" -o "$SAIDA/$nome.png"
        fi
    done
done

echo
echo "Aprovados: $aprovados  Reprovados: $reprovados"
[ "$reprovados" -eq 0 ]
