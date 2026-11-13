#include "preprocessador.h"

int PreProcessamento(FILE *entrada, FILE *saida) {
    int caractere;
    int dentro_string = 0;
    int escape = 0;
    int em_comentario = 0;
    int linha_tem_conteudo = 0;
    int espaco_pendente = 0;

    if (entrada == NULL || saida == NULL) {
        return 0;
    }

    while ((caractere = fgetc(entrada)) != EOF) {
        if (caractere == '\r') {
            int proximo = fgetc(entrada);
            if (proximo != '\n' && proximo != EOF) {
                if (ungetc(proximo, entrada) == EOF) {
                    return 0;
                }
            }
            caractere = '\n';
        }

        if (caractere == '\n') {
            /* Toda quebra de linha e preservada (inclusive em linhas vazias
             * ou so com comentarios), para que a numeracao de linhas do
             * .pre, do .lex e dos diagnosticos coincida com o .asm. */
            if (fputc('\n', saida) == EOF) {
                return 0;
            }

            dentro_string = 0;
            escape = 0;
            em_comentario = 0;
            linha_tem_conteudo = 0;
            espaco_pendente = 0;
            continue;
        }

        if (em_comentario) {
            continue;
        }

        if (dentro_string) {
            if (fputc(caractere, saida) == EOF) {
                return 0;
            }
            linha_tem_conteudo = 1;

            if (escape) {
                escape = 0;
            } else if (caractere == '\\') {
                escape = 1;
            } else if (caractere == '"') {
                dentro_string = 0;
            }
            continue;
        }

        if (caractere == '#') {
            em_comentario = 1;
            espaco_pendente = 0;
            continue;
        }

        if (caractere == ' ' || caractere == '\t' || caractere == '\f' ||
            caractere == '\v') {
            if (linha_tem_conteudo) {
                espaco_pendente = 1;
            }
            continue;
        }

        if (espaco_pendente) {
            if (fputc(' ', saida) == EOF) {
                return 0;
            }
            espaco_pendente = 0;
        }

        if (fputc(caractere, saida) == EOF) {
            return 0;
        }

        linha_tem_conteudo = 1;
        if (caractere == '"') {
            dentro_string = 1;
            escape = 0;
        }
    }

    if (ferror(entrada)) {
        return 0;
    }

    if (linha_tem_conteudo && fputc('\n', saida) == EOF) {
        return 0;
    }

    return ferror(saida) ? 0 : 1;
}
