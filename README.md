# micrU

Compilador de **μ-assembly → C** desenvolvido para a disciplina de
**Linguagens Formais, Autômatos e Compiladores**.

Fases já implementadas: pré-processamento, análise léxica e análise sintática,
com geração de todos os artefatos intermediários e da árvore sintática em
formato Graphviz.

## Estrutura

```
micrU/
├── include/            # Interfaces públicas de cada módulo
├── src/
│   ├── main.c          # Interface de linha de comando
│   ├── orquestrador.c  # Coordena o pipeline de compilação
│   ├── preprocessador.c
│   ├── lexico.c
│   └── sintatico.c
├── testes/
│   ├── validos/        # Programas que devem ser aceitos
│   ├── invalidos/      # Programas que devem ser rejeitados
│   └── esperado/       # Saídas de referência
├── docs/               # Arquitetura e relatório técnico
└── Regras_PROFESSOR/   # Especificação e exemplos da disciplina
```

Detalhes em [docs/ARQUITETURA.md](docs/ARQUITETURA.md).

## Compilação

Requer um compilador C99 (`gcc` ou `clang`).

```sh
make
```

No Windows, sem `make`:

```powershell
gcc -std=c99 -Wall -Wextra -Iinclude src\*.c -o micru.exe
```

## Uso

```sh
./micru <arquivo_entrada.asm> <prefixo_saida>
```

Exemplo:

```sh
./micru Regras_PROFESSOR/Exemplo01.asm build/exemplo01
```

Gera `build/exemplo01.pre`, `.lex`, `.ts`, `.lex.err`, `.sin.err` e `.dot`.
Para visualizar a árvore sintática (requer Graphviz):

```sh
dot -Tpng build/exemplo01.dot -o build/exemplo01.png
```

### Códigos de saída

| Código | Significado |
|---|---|
| `0` | Programa sintaticamente válido |
| `1` | Programa rejeitado (erros léxicos ou sintáticos) |
| `2` | Falha operacional (argumentos, E/S ou memória) |

## Testes

```sh
make test        # casos de testes/validos e testes/invalidos
make exemplos    # exemplos fornecidos pelo professor
```

No Windows: `powershell -File testes\executar_testes.ps1`.

## Licença

Distribuído sob a licença MIT. Consulte [LICENSE](LICENSE).
