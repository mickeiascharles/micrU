#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "orquestrador.h"

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo_entrada.asm> <prefixo_saida>\n",
                argc > 0 ? argv[0] : "micru");
        return MICRU_FALHA;
    }
    return Orquestrar(argv[1], argv[2]);
}
