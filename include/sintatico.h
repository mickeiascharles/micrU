#ifndef SINTATICO_H
#define SINTATICO_H

#include <stdio.h>

#include "lexico.h"

#define SINTATICO_VALIDO 0
#define SINTATICO_INVALIDO 1
#define SINTATICO_FALHA (-1)

#define TOKEN_EOL "EOL"
#define TOKEN_EOF "TK_EOF"

/*
 * tokens: arquivo .lex aberto para leitura
 * arvore: arquivo .dot aberto para escrita (preenchido somente se valido)
 * erros:  arquivo .sin.err aberto para escrita
 *
 * Retorna SINTATICO_VALIDO, SINTATICO_INVALIDO ou SINTATICO_FALHA.
 */
int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros);

#endif
