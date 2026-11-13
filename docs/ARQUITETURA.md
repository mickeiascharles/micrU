# Arquitetura do micrU

O micrU é organizado como um **pipeline orquestrado**: um orquestrador central
coordena módulos independentes, cada um responsável por uma única fase da
compilação. Nenhum módulo conhece os demais — eles se comunicam apenas pelos
arquivos intermediários que o orquestrador abre, entrega e fecha.

```
                     ┌──────────────────────────┐
  main.c  ─────────▶ │   orquestrador.c         │
  (CLI)              │  - monta caminhos        │
                     │  - abre/fecha artefatos  │
                     │  - executa as etapas     │
                     │  - decide o código final │
                     └────────────┬─────────────┘
                                  │
        ┌─────────────────────────┼──────────────────────────┐
        ▼                         ▼                          ▼
┌────────────────┐      ┌───────────────────┐      ┌────────────────────┐
│ preprocessador │ .pre │      lexico       │ .lex │     sintatico      │
│ comentários e  │ ───▶ │ AFD de tokens,    │ ───▶ │ descendente        │
│ espaços        │      │ tabela de símbolos│      │ recursivo + árvore │
└────────────────┘      └───────────────────┘      └────────────────────┘
      .pre               .lex  .ts  .lex.err          .sin.err  .dot
```

## Módulos

| Módulo | Arquivos | Responsabilidade |
|---|---|---|
| CLI | `src/main.c` | Valida argumentos e delega ao orquestrador. |
| Orquestrador | `src/orquestrador.c`, `include/orquestrador.h` | Gerencia o ciclo de vida dos arquivos, executa as etapas em ordem e interrompe o pipeline na primeira rejeição ou falha. |
| Pré-processador | `src/preprocessador.c`, `include/preprocessador.h` | Remove comentários, normaliza espaços e preserva a numeração de linhas. |
| Analisador léxico | `src/lexico.c`, `include/lexico.h` | Reconhece tokens por um AFD, monta a tabela de símbolos e registra erros léxicos. |
| Analisador sintático | `src/sintatico.c`, `include/sintatico.h` | Valida a gramática (BNF nos comentários de cada função), faz recuperação de erros por linha e gera a árvore sintática em Graphviz. |

## Contrato das etapas

Cada etapa é registrada na tabela `PIPELINE` do orquestrador como um par
`{nome, função}` e devolve um `ResultadoEtapa`:

- `ETAPA_CONCLUIDA` — segue para a próxima etapa;
- `ETAPA_REJEITADA` — o programa de entrada é inválido (código de saída `1`);
- `ETAPA_FALHOU` — erro operacional de E/S ou memória (código de saída `2`).

Para incluir uma nova fase (por exemplo, a análise semântica), basta
implementá-la em um módulo próprio, criar a função de etapa correspondente e
acrescentá-la à tabela `PIPELINE`.

## Artefatos gerados

| Extensão | Etapa | Conteúdo |
|---|---|---|
| `.pre` | Pré-processamento | Código sem comentários e com espaços normalizados. |
| `.lex` | Léxica | Um token por linha: `<CATEGORIA, lexema> linha coluna`. |
| `.ts` | Léxica | Tabela de símbolos em CSV. |
| `.lex.err` | Léxica | Erros léxicos encontrados. |
| `.sin.err` | Sintática | Erros sintáticos e total de erros. |
| `.dot` | Sintática | Árvore sintática (apenas para programas válidos). |
