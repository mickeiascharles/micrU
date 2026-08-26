#include <stdio.h>
#include <stdlib.h>
#include "preprocessador.h"

int main(int argc, char *argv[]) {
    FILE *entrada;
    FILE *saida;

    if (argc != 3) {
        fprintf(
            stderr,
            "Uso: %s <arquivo_entrada.asm> <arquivo_saida.pre>\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }

    entrada = fopen(argv[1], "r");

    if (entrada == NULL) {
        fprintf(
            stderr,
            "Erro: nao foi possivel abrir o arquivo de entrada '%s'.\n",
            argv[1]
        );

        return EXIT_FAILURE;
    }

    saida = fopen(argv[2], "w");

    if (saida == NULL) {
        fprintf(
            stderr,
            "Erro: nao foi possivel criar o arquivo de saida '%s'.\n",
            argv[2]
        );

        fclose(entrada);

        return EXIT_FAILURE;
    }

    if (!preprocessar(entrada, saida)) {
        fprintf(
            stderr,
            "Erro: falha durante o pre-processamento.\n"
        );

        fclose(entrada);
        fclose(saida);

        return EXIT_FAILURE;
    }

    if (fclose(entrada) != 0) {
        fprintf(
            stderr,
            "Aviso: erro ao fechar o arquivo de entrada.\n"
        );
    }

    if (fclose(saida) != 0) {
        fprintf(
            stderr,
            "Erro: nao foi possivel finalizar o arquivo de saida.\n"
        );

        return EXIT_FAILURE;
    }

    printf("Pre-processamento concluido com sucesso.\n");

    return EXIT_SUCCESS;
}