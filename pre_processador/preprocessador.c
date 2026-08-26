#include "preprocessador.h"

#include <stddef.h>

#define TAMANHO_LINHA 4096


static int processar_linha(
    const char *origem,
    char *destino,
    size_t capacidade
) {
    size_t i = 0;
    size_t j = 0;

    int dentro_string = 0;
    int espaco_pendente = 0;

    if (capacidade == 0) {
        return -1;
    }

    while (origem[i] != '\0') {

        char c = origem[i];

        if (c == '\n' || c == '\r') {
            break;
        }

        if (dentro_string) {

            if (j + 1 >= capacidade) {
                return -1;
            }

            destino[j++] = c;

            if (c == '"') {
                dentro_string = 0;
            }

            i++;
            continue;
        }

        if (c == '#') {
            break;
        }

        if (c == '"') {

            if (espaco_pendente && j > 0) {

                if (j + 1 >= capacidade) {
                    return -1;
                }

                destino[j++] = ' ';
            }

            espaco_pendente = 0;

            if (j + 1 >= capacidade) {
                return -1;
            }

            destino[j++] = c;

            dentro_string = 1;

            i++;
            continue;
        }

        if (c == ' ' || c == '\t') {
            if (j > 0) {
                espaco_pendente = 1;
            }

            i++;
            continue;
        }

        if (espaco_pendente && j > 0) {

            if (j + 1 >= capacidade) {
                return -1;
            }

            destino[j++] = ' ';

            espaco_pendente = 0;
        }

        if (j + 1 >= capacidade) {
            return -1;
        }

        destino[j++] = c;

        i++;
    }

    destino[j] = '\0';

    return (int) j;
}

int preprocessar(FILE *entrada, FILE *saida) {

    char linha_original[TAMANHO_LINHA];
    char linha_processada[TAMANHO_LINHA];

    while (
        fgets(
            linha_original,
            sizeof(linha_original),
            entrada
        ) != NULL
    ) {

        int tamanho = processar_linha(
            linha_original,
            linha_processada,
            sizeof(linha_processada)
        );

        if (tamanho < 0) {
            return 0;
        }

        if (tamanho == 0) {
            continue;
        }

        if (fputs(linha_processada, saida) == EOF) {
            return 0;
        }

        if (fputc('\n', saida) == EOF) {
            return 0;
        }
    }

    if (ferror(entrada) || ferror(saida)) {
        return 0;
    }

    return 1;
}