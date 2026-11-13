#ifndef LEXICO_H
#define LEXICO_H

#include <stdio.h>

#define TAMANHO_NOME_TOKEN 50
#define TAMANHO_LEXEMA_TOKEN 512
#define TAMANHO_CATEGORIA_SIMBOLO 40

typedef struct {
    char nome[TAMANHO_NOME_TOKEN];
    char lexema[TAMANHO_LEXEMA_TOKEN];
    int linha;
    int coluna;
} Token;

typedef struct {
    char lexema[TAMANHO_LEXEMA_TOKEN];
    char categoria[TAMANHO_CATEGORIA_SIMBOLO];
    int primeiraLinha;
    int primeiraColuna;
} Simbolo;

#define LEXICO_VALIDO 0
#define LEXICO_COM_ERROS 1
#define LEXICO_FALHA (-1)

/*
 * entrada: arquivo .pre aberto para leitura
 * tokens:  arquivo .lex aberto para escrita
 * tabela:  arquivo .ts aberto para escrita (tabela de simbolos em CSV)
 * erros:   arquivo .lex.err aberto para escrita
 *
 * Retorna LEXICO_VALIDO, LEXICO_COM_ERROS ou LEXICO_FALHA.
 */
int AnaliseLexica(FILE *entrada, FILE *tokens, FILE *tabela, FILE *erros);

#endif
