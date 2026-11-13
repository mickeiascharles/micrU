#ifndef ORQUESTRADOR_H
#define ORQUESTRADOR_H

/* Codigos de encerramento do compilador. */
#define MICRU_VALIDO 0
#define MICRU_REJEITADO 1
#define MICRU_FALHA 2

/*
 * Executa o pipeline completo do micrU sobre o arquivo de entrada:
 *
 *   .asm -> [pre-processador] -> .pre
 *        -> [analisador lexico] -> .lex, .ts, .lex.err
 *        -> [analisador sintatico] -> .sin.err, .dot
 *
 * Todos os artefatos sao gravados como <prefixo><extensao>.
 * Retorna MICRU_VALIDO, MICRU_REJEITADO ou MICRU_FALHA.
 */
int Orquestrar(const char *caminho_entrada, const char *prefixo);

#endif
