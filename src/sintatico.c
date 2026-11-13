#include "sintatico.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define TAMANHO_LINHA_LEX (TAMANHO_NOME_TOKEN + TAMANHO_LEXEMA_TOKEN + 128)
#define TAMANHO_DESCRICAO (TAMANHO_LEXEMA_TOKEN + 128)
#define CAPACIDADE_INICIAL 128

typedef struct {
    char *rotulo;
    char *lexema;
    int *filhos;
    size_t total_filhos;
    size_t capacidade_filhos;
} No;

typedef struct {
    Token *itens;
    size_t total;
    size_t capacidade;
} ListaTokens;

static ListaTokens entrada;
static size_t posicao;

static No *nos;
static size_t total_nos;
static size_t capacidade_nos;

static FILE *saida_erros;
static int total_erros;
static int falha;

static const char *const OPS_TRES_REGISTRADORES[] = {
    "INS_ADD", "INS_ADDU", "INS_SUB", "INS_SUBU", "INS_MUL",
    "INS_AND", "INS_OR", "INS_XOR", "INS_NOR", "INS_SLT", NULL
};
static const char *const OPS_DOIS_REGISTRADORES[] = {"INS_MOVE", "INS_DIV", NULL};
static const char *const OPS_UM_REGISTRADOR[] = {"INS_MFLO", "INS_MFHI", "INS_JR", NULL};
static const char *const OPS_DESLOCAMENTO[] = {"INS_SLL", "INS_SRL", NULL};
static const char *const OPS_MEMORIA[] = {
    "INS_LW", "INS_SW", "INS_LB", "INS_SB", "INS_LH", "INS_SH", NULL
};
static const char *const OPS_DESVIO[] = {
    "INS_BEQ", "INS_BNE", "INS_BLT", "INS_BLE", "INS_BGT", "INS_BGE", NULL
};
static const char *const OPS_SALTO[] = {"INS_J", "INS_JAL", NULL};
static const char *const OPS_SEM_OPERANDOS[] = {"INS_SYSCALL", "INS_NOP", NULL};
static const char *const DIRETIVAS_INTEIROS[] = {"DIR_WORD", "DIR_HALF", "DIR_BYTE", NULL};
static const char *const DIRETIVAS_STRING[] = {"DIR_ASCII", "DIR_ASCIIZ", NULL};

static void copiar_texto(char *destino, size_t capacidade, const char *origem) {
    if (capacidade == 0U) {
        return;
    }
    strncpy(destino, origem, capacidade - 1U);
    destino[capacidade - 1U] = '\0';
}

static char *duplicar_texto(const char *texto) {
    size_t tamanho = strlen(texto) + 1U;
    char *copia = (char *) malloc(tamanho);
    if (copia != NULL) {
        memcpy(copia, texto, tamanho);
    }
    return copia;
}

/* Ex.: SMB_COM -> SMB_COM(","), DIR_WORD -> DIR_WORD(".word"), REG -> REG */
static void descrever_categoria(const char *categoria, char *destino,
                                size_t capacidade) {
    const char *simbolo = NULL;
    char texto[TAMANHO_NOME_TOKEN];
    size_t i;

    if (strcmp(categoria, "SMB_COM") == 0) {
        simbolo = ",";
    } else if (strcmp(categoria, "SMB_COL") == 0) {
        simbolo = ":";
    } else if (strcmp(categoria, "SMB_OPA") == 0) {
        simbolo = "(";
    } else if (strcmp(categoria, "SMB_CPA") == 0) {
        simbolo = ")";
    }

    if (simbolo != NULL) {
        snprintf(destino, capacidade, "%s(\"%s\")", categoria, simbolo);
    } else if (strncmp(categoria, "DIR_", 4U) == 0 ||
               strncmp(categoria, "INS_", 4U) == 0) {
        size_t inicio = 0U;
        if (categoria[0] == 'D') {
            texto[inicio++] = '.';
        }
        for (i = 4U; categoria[i] != '\0' && inicio + 1U < sizeof(texto); ++i) {
            texto[inicio++] = (char) tolower((unsigned char) categoria[i]);
        }
        texto[inicio] = '\0';
        snprintf(destino, capacidade, "%s(\"%s\")", categoria, texto);
    } else {
        copiar_texto(destino, capacidade, categoria);
    }
}

static void descrever_token(const Token *token, char *destino,
                            size_t capacidade) {
    if (strcmp(token->nome, TOKEN_EOL) == 0) {
        snprintf(destino, capacidade, "EOL (término da linha %d)", token->linha);
    } else if (strcmp(token->nome, TOKEN_EOF) == 0) {
        copiar_texto(destino, capacidade, TOKEN_EOF);
    } else if (strcmp(token->nome, "STRING") == 0) {
        snprintf(destino, capacidade, "STRING(%s)", token->lexema);
    } else {
        snprintf(destino, capacidade, "%s(\"%s\")", token->nome, token->lexema);
    }
}

static int adicionar_token(ListaTokens *lista, const char *nome,
                           const char *lexema, int linha, int coluna) {
    Token *token;

    if (lista->total == lista->capacidade) {
        size_t nova_capacidade = lista->capacidade == 0U
                                     ? CAPACIDADE_INICIAL
                                     : lista->capacidade * 2U;
        Token *novos = (Token *) realloc(lista->itens,
                                         nova_capacidade * sizeof(*novos));
        if (novos == NULL) {
            return 0;
        }
        lista->itens = novos;
        lista->capacidade = nova_capacidade;
    }

    token = &lista->itens[lista->total++];
    copiar_texto(token->nome, sizeof(token->nome), nome);
    copiar_texto(token->lexema, sizeof(token->lexema), lexema);
    token->linha = linha;
    token->coluna = coluna;
    return 1;
}

/*
 * Le uma linha "<CATEGORIA, lexema> linha coluna". Como uma string pode conter
 * virgulas e '>', a posicao e lida a partir do ultimo '>' da linha.
 * Retorna 1 para token lido, 0 para linha em branco e -1 para linha invalida.
 */
static int interpretar_linha(char *linha, Token *token) {
    size_t tamanho = strlen(linha);
    char *fim;
    char *separador;
    size_t tamanho_nome;
    size_t tamanho_lexema;
    char sobra;

    while (tamanho > 0U &&
           (linha[tamanho - 1U] == '\n' || linha[tamanho - 1U] == '\r')) {
        linha[--tamanho] = '\0';
    }
    if (tamanho == 0U) {
        return 0;
    }
    if (linha[0] != '<') {
        return -1;
    }

    fim = strrchr(linha, '>');
    if (fim == NULL ||
        sscanf(fim + 1, " %d %d %c", &token->linha, &token->coluna, &sobra) != 2) {
        return -1;
    }

    separador = strstr(linha + 1, ", ");
    if (separador == NULL || separador > fim) {
        return -1;
    }

    tamanho_nome = (size_t) (separador - (linha + 1));
    tamanho_lexema = (size_t) (fim - (separador + 2));
    if (tamanho_nome == 0U || tamanho_nome >= sizeof(token->nome) ||
        tamanho_lexema >= sizeof(token->lexema)) {
        return -1;
    }

    memcpy(token->nome, linha + 1, tamanho_nome);
    token->nome[tamanho_nome] = '\0';
    memcpy(token->lexema, separador + 2, tamanho_lexema);
    token->lexema[tamanho_lexema] = '\0';
    return 1;
}

/*
 * Carrega o .lex inserindo EOL depois do ultimo token de cada linha. TK_EOF e
 * acrescentado somente se o arquivo nao o tiver.
 */
static int carregar_tokens(FILE *arquivo) {
    ListaTokens lidos = {NULL, 0U, 0U};
    char linha[TAMANHO_LINHA_LEX];
    Token token;
    Token eof;
    int tem_eof = 0;
    int ok = 1;
    size_t i;

    while (ok && fgets(linha, sizeof(linha), arquivo) != NULL) {
        int resultado;

        if (strchr(linha, '\n') == NULL && !feof(arquivo)) {
            fprintf(stderr, "Erro: linha muito longa no arquivo de tokens.\n");
            ok = 0;
            break;
        }

        resultado = interpretar_linha(linha, &token);
        if (resultado < 0) {
            fprintf(stderr, "Erro: linha invalida no arquivo de tokens: %s\n", linha);
            ok = 0;
        } else if (resultado > 0) {
            if (strcmp(token.nome, TOKEN_EOF) == 0) {
                eof = token;
                tem_eof = 1;
                break;
            }
            ok = adicionar_token(&lidos, token.nome, token.lexema,
                                 token.linha, token.coluna);
        }
    }

    if (ok && ferror(arquivo)) {
        fprintf(stderr, "Erro: falha ao ler o arquivo de tokens.\n");
        ok = 0;
    }

    for (i = 0U; ok && i < lidos.total; ++i) {
        const Token *t = &lidos.itens[i];

        ok = adicionar_token(&entrada, t->nome, t->lexema, t->linha, t->coluna);
        if (ok && (i + 1U == lidos.total || lidos.itens[i + 1U].linha != t->linha)) {
            ok = adicionar_token(&entrada, TOKEN_EOL, "", t->linha,
                                 t->coluna + (int) strlen(t->lexema));
        }
    }

    if (ok && !tem_eof) {
        copiar_texto(eof.nome, sizeof(eof.nome), TOKEN_EOF);
        copiar_texto(eof.lexema, sizeof(eof.lexema), "EOF");
        eof.linha = lidos.total > 0U ? lidos.itens[lidos.total - 1U].linha + 1 : 1;
        eof.coluna = 1;
    }
    if (ok) {
        ok = adicionar_token(&entrada, eof.nome, eof.lexema, eof.linha, eof.coluna);
    }

    free(lidos.itens);
    return ok;
}

static int novo_no(const char *rotulo, const char *lexema) {
    No *no;

    if (falha) {
        return -1;
    }

    if (total_nos == capacidade_nos) {
        size_t nova_capacidade = capacidade_nos == 0U
                                     ? CAPACIDADE_INICIAL
                                     : capacidade_nos * 2U;
        No *novos = (No *) realloc(nos, nova_capacidade * sizeof(*novos));
        if (novos == NULL) {
            falha = 1;
            return -1;
        }
        nos = novos;
        capacidade_nos = nova_capacidade;
    }

    no = &nos[total_nos];
    no->rotulo = duplicar_texto(rotulo);
    no->lexema = lexema != NULL ? duplicar_texto(lexema) : NULL;
    no->filhos = NULL;
    no->total_filhos = 0U;
    no->capacidade_filhos = 0U;
    if (no->rotulo == NULL || (lexema != NULL && no->lexema == NULL)) {
        free(no->rotulo);
        free(no->lexema);
        falha = 1;
        return -1;
    }
    return (int) total_nos++;
}

static void adicionar_filho(int pai, int filho) {
    No *no;

    if (pai < 0 || filho < 0 || falha) {
        return;
    }

    no = &nos[pai];
    if (no->total_filhos == no->capacidade_filhos) {
        size_t nova_capacidade = no->capacidade_filhos == 0U
                                     ? 4U
                                     : no->capacidade_filhos * 2U;
        int *novos = (int *) realloc(no->filhos, nova_capacidade * sizeof(*novos));
        if (novos == NULL) {
            falha = 1;
            return;
        }
        no->filhos = novos;
        no->capacidade_filhos = nova_capacidade;
    }
    no->filhos[no->total_filhos++] = filho;
}

static int nao_terminal(int pai, const char *nome) {
    int no = novo_no(nome, NULL);
    adicionar_filho(pai, no);
    return no;
}

static void escrever_escapado(FILE *saida, const char *texto) {
    for (; *texto != '\0'; ++texto) {
        if (*texto == '"' || *texto == '\\') {
            fputc('\\', saida);
        }
        fputc((unsigned char) *texto, saida);
    }
}

static int escrever_dot(FILE *saida) {
    size_t i;
    size_t j;

    fprintf(saida, "digraph ArvoreSintatica {\n");
    fprintf(saida, "    graph [ordering=out];\n");
    fprintf(saida, "    node [shape=box, fontname=\"Arial\"];\n");
    fprintf(saida, "    edge [arrowhead=none];\n\n");

    for (i = 0U; i < total_nos; ++i) {
        fprintf(saida, "    n%lu [label=\"", (unsigned long) i);
        escrever_escapado(saida, nos[i].rotulo);
        if (nos[i].lexema != NULL) {
            fputs("\\n", saida);
            escrever_escapado(saida, nos[i].lexema);
        }
        fputs("\"];\n", saida);
    }

    fputc('\n', saida);
    for (i = 0U; i < total_nos; ++i) {
        for (j = 0U; j < nos[i].total_filhos; ++j) {
            fprintf(saida, "    n%lu -> n%d;\n", (unsigned long) i, nos[i].filhos[j]);
        }
    }
    fprintf(saida, "}\n");

    return !ferror(saida);
}

static void liberar_estado(void) {
    size_t i;

    for (i = 0U; i < total_nos; ++i) {
        free(nos[i].rotulo);
        free(nos[i].lexema);
        free(nos[i].filhos);
    }
    free(nos);
    nos = NULL;
    total_nos = 0U;
    capacidade_nos = 0U;

    free(entrada.itens);
    entrada.itens = NULL;
    entrada.total = 0U;
    entrada.capacidade = 0U;
    posicao = 0U;
}

static const Token *atual(void) {
    return &entrada.itens[posicao];
}

static int verificar(const char *categoria) {
    return strcmp(atual()->nome, categoria) == 0;
}

static int pertence(const char *const *grupo) {
    for (; *grupo != NULL; ++grupo) {
        if (verificar(*grupo)) {
            return 1;
        }
    }
    return 0;
}

static void avancar(void) {
    if (!verificar(TOKEN_EOF)) {
        ++posicao;
    }
}

static int inicio_de_secao(void) {
    return verificar("DIR_DATA") || verificar("DIR_TEXT");
}

static int fim_de_secao(void) {
    return inicio_de_secao() || verificar(TOKEN_EOF);
}

static int inicio_de_instrucao(void) {
    return strncmp(atual()->nome, "INS_", 4U) == 0;
}

static int inicio_de_diretiva_dados(void) {
    return pertence(DIRETIVAS_INTEIROS) || pertence(DIRETIVAS_STRING) ||
           verificar("DIR_SPACE") || verificar("DIR_ALIGN");
}

static void erro_sintatico(const char *esperado) {
    char encontrado[TAMANHO_DESCRICAO];
    const Token *token = atual();

    descrever_token(token, encontrado, sizeof(encontrado));
    ++total_erros;
    if (fprintf(saida_erros, "Linha %d, coluna %d: Encontrado %s; esperado %s.\n",
                token->linha, token->coluna, encontrado, esperado) < 0) {
        falha = 1;
    }
}

/*
 * Descarta o restante da linha atual, consumindo o EOL. Para antes de TK_EOF e
 * de uma diretiva de secao, sem nunca avancar para a linha seguinte.
 */
static void recuperar(void) {
    while (!verificar(TOKEN_EOF) && !inicio_de_secao()) {
        int era_eol = verificar(TOKEN_EOL);
        avancar();
        if (era_eol) {
            return;
        }
    }
}

static int terminal(int pai, const char *categoria) {
    const Token *token = atual();

    if (!verificar(categoria)) {
        char esperado[TAMANHO_DESCRICAO];
        descrever_categoria(categoria, esperado, sizeof(esperado));
        erro_sintatico(esperado);
        return 0;
    }

    if (strcmp(categoria, TOKEN_EOL) == 0 || strcmp(categoria, TOKEN_EOF) == 0) {
        adicionar_filho(pai, novo_no(token->nome, NULL));
    } else {
        adicionar_filho(pai, novo_no(token->nome, token->lexema));
    }
    avancar();
    return 1;
}

static int terminal_do_grupo(int pai, const char *const *grupo,
                             const char *esperado) {
    if (!pertence(grupo)) {
        erro_sintatico(esperado);
        return 0;
    }
    return terminal(pai, atual()->nome);
}

static int secao(int pai);
static int secao_dados(int pai);
static int secao_codigo(int pai);
static int linha_dados(int pai);
static int linha_codigo(int pai);
static int rotulo(int pai);
static int diretiva_global(int pai);
static int diretiva_dados(int pai);
static int diretiva_inteiros(int pai);
static int lista_inteiros(int pai);
static int diretiva_string(int pai);
static int instrucao(int pai);
static int instrucao_tres_registradores(int pai);
static int op_tres_registradores(int pai);
static int instrucao_dois_registradores(int pai);
static int op_dois_registradores(int pai);
static int instrucao_um_registrador(int pai);
static int op_um_registrador(int pai);
static int instrucao_imediato(int pai);
static int instrucao_deslocamento(int pai);
static int op_deslocamento(int pai);
static int instrucao_li(int pai);
static int instrucao_la(int pai);
static int instrucao_memoria(int pai);
static int op_memoria(int pai);
static int endereco(int pai);
static int instrucao_desvio(int pai);
static int op_desvio(int pai);
static int instrucao_salto(int pai);
static int op_salto(int pai);
static int instrucao_sem_operandos(int pai);

/* <programa> ::= <secao> { <secao> } TK_EOF */
static int programa(void) {
    int no = novo_no("<programa>", NULL);

    if (!inicio_de_secao()) {
        do {
            erro_sintatico("DIR_DATA(\".data\") ou DIR_TEXT(\".text\")");
            recuperar();
        } while (!inicio_de_secao() && !verificar(TOKEN_EOF));
    }

    while (inicio_de_secao()) {
        secao(no);
    }

    return terminal(no, TOKEN_EOF);
}

/* <secao> ::= <secao_dados> | <secao_codigo> */
static int secao(int pai) {
    int no = nao_terminal(pai, "<secao>");

    if (verificar("DIR_DATA")) {
        return secao_dados(no);
    }
    return secao_codigo(no);
}

/* <secao_dados> ::= ".data" EOL { <linha_dados> } */
static int secao_dados(int pai) {
    int no = nao_terminal(pai, "<secao_dados>");
    int ok = terminal(no, "DIR_DATA") && terminal(no, TOKEN_EOL);

    if (!ok) {
        recuperar();
    }
    while (!fim_de_secao()) {
        if (!linha_dados(no)) {
            ok = 0;
        }
    }
    return ok;
}

/* <secao_codigo> ::= ".text" EOL { <linha_codigo> } */
static int secao_codigo(int pai) {
    int no = nao_terminal(pai, "<secao_codigo>");
    int ok = terminal(no, "DIR_TEXT") && terminal(no, TOKEN_EOL);

    if (!ok) {
        recuperar();
    }
    while (!fim_de_secao()) {
        if (!linha_codigo(no)) {
            ok = 0;
        }
    }
    return ok;
}

/*
 * <linha_dados> ::= <rotulo> [ <diretiva_dados> ] EOL
 *                 | <diretiva_dados> EOL
 *                 | <diretiva_global> EOL
 */
static int linha_dados(int pai) {
    int no = nao_terminal(pai, "<linha_dados>");
    int ok;

    if (verificar("ID")) {
        ok = rotulo(no);
        if (ok && inicio_de_diretiva_dados()) {
            ok = diretiva_dados(no);
        } else if (ok && !verificar(TOKEN_EOL)) {
            erro_sintatico(inicio_de_instrucao()
                               ? "diretiva de dados ou EOL (instruções só são "
                                 "permitidas na seção .text)"
                               : "diretiva de dados ou EOL");
            ok = 0;
        }
        ok = ok && terminal(no, TOKEN_EOL);
    } else if (inicio_de_diretiva_dados()) {
        ok = diretiva_dados(no) && terminal(no, TOKEN_EOL);
    } else if (verificar("DIR_GLOBL")) {
        ok = diretiva_global(no) && terminal(no, TOKEN_EOL);
    } else {
        erro_sintatico(inicio_de_instrucao()
                           ? "ID, diretiva de dados ou DIR_GLOBL(\".globl\") "
                             "(instruções só são permitidas na seção .text)"
                           : "ID, diretiva de dados ou DIR_GLOBL(\".globl\")");
        ok = 0;
    }

    if (!ok) {
        recuperar();
    }
    return ok;
}

/*
 * <linha_codigo> ::= <rotulo> [ <instrucao> ] EOL
 *                  | <instrucao> EOL
 *                  | <diretiva_global> EOL
 */
static int linha_codigo(int pai) {
    int no = nao_terminal(pai, "<linha_codigo>");
    int ok;

    if (verificar("ID")) {
        ok = rotulo(no);
        if (ok && inicio_de_instrucao()) {
            ok = instrucao(no);
        } else if (ok && !verificar(TOKEN_EOL)) {
            erro_sintatico(inicio_de_diretiva_dados()
                               ? "instrução ou EOL (diretivas de dados só são "
                                 "permitidas na seção .data)"
                               : "instrução ou EOL");
            ok = 0;
        }
        ok = ok && terminal(no, TOKEN_EOL);
    } else if (inicio_de_instrucao()) {
        ok = instrucao(no) && terminal(no, TOKEN_EOL);
    } else if (verificar("DIR_GLOBL")) {
        ok = diretiva_global(no) && terminal(no, TOKEN_EOL);
    } else {
        erro_sintatico(inicio_de_diretiva_dados()
                           ? "ID, instrução ou DIR_GLOBL(\".globl\") (diretivas "
                             "de dados só são permitidas na seção .data)"
                           : "ID, instrução ou DIR_GLOBL(\".globl\")");
        ok = 0;
    }

    if (!ok) {
        recuperar();
    }
    return ok;
}

/* <rotulo> ::= ID ":" */
static int rotulo(int pai) {
    int no = nao_terminal(pai, "<rotulo>");
    return terminal(no, "ID") && terminal(no, "SMB_COL");
}

/* <diretiva_global> ::= ".globl" ID */
static int diretiva_global(int pai) {
    int no = nao_terminal(pai, "<diretiva_global>");
    return terminal(no, "DIR_GLOBL") && terminal(no, "ID");
}

/*
 * <diretiva_dados> ::= <diretiva_inteiros> <lista_inteiros>
 *                    | <diretiva_string> STRING
 *                    | ".space" NUM_INT
 *                    | ".align" NUM_INT
 */
static int diretiva_dados(int pai) {
    int no = nao_terminal(pai, "<diretiva_dados>");

    if (pertence(DIRETIVAS_INTEIROS)) {
        return diretiva_inteiros(no) && lista_inteiros(no);
    }
    if (pertence(DIRETIVAS_STRING)) {
        return diretiva_string(no) && terminal(no, "STRING");
    }
    if (verificar("DIR_SPACE")) {
        return terminal(no, "DIR_SPACE") && terminal(no, "NUM_INT");
    }
    if (verificar("DIR_ALIGN")) {
        return terminal(no, "DIR_ALIGN") && terminal(no, "NUM_INT");
    }
    erro_sintatico("diretiva de dados");
    return 0;
}

/* <diretiva_inteiros> ::= ".word" | ".half" | ".byte" */
static int diretiva_inteiros(int pai) {
    int no = nao_terminal(pai, "<diretiva_inteiros>");
    return terminal_do_grupo(no, DIRETIVAS_INTEIROS, ".word, .half ou .byte");
}

/* <lista_inteiros> ::= NUM_INT { "," NUM_INT } */
static int lista_inteiros(int pai) {
    int no = nao_terminal(pai, "<lista_inteiros>");

    if (!terminal(no, "NUM_INT")) {
        return 0;
    }
    while (verificar("SMB_COM")) {
        if (!terminal(no, "SMB_COM") || !terminal(no, "NUM_INT")) {
            return 0;
        }
    }
    if (verificar("NUM_INT")) {
        erro_sintatico("SMB_COM(\",\") ou EOL");
        return 0;
    }
    return 1;
}

/* <diretiva_string> ::= ".ascii" | ".asciiz" */
static int diretiva_string(int pai) {
    int no = nao_terminal(pai, "<diretiva_string>");
    return terminal_do_grupo(no, DIRETIVAS_STRING, ".ascii ou .asciiz");
}

/*
 * <instrucao> ::= <instrucao_tres_registradores> | <instrucao_dois_registradores>
 *               | <instrucao_um_registrador> | <instrucao_imediato>
 *               | <instrucao_deslocamento> | <instrucao_li> | <instrucao_la>
 *               | <instrucao_memoria> | <instrucao_desvio> | <instrucao_salto>
 *               | <instrucao_sem_operandos>
 */
static int instrucao(int pai) {
    int no = nao_terminal(pai, "<instrucao>");

    if (pertence(OPS_TRES_REGISTRADORES)) {
        return instrucao_tres_registradores(no);
    }
    if (pertence(OPS_DOIS_REGISTRADORES)) {
        return instrucao_dois_registradores(no);
    }
    if (pertence(OPS_UM_REGISTRADOR)) {
        return instrucao_um_registrador(no);
    }
    if (verificar("INS_ADDI")) {
        return instrucao_imediato(no);
    }
    if (pertence(OPS_DESLOCAMENTO)) {
        return instrucao_deslocamento(no);
    }
    if (verificar("INS_LI")) {
        return instrucao_li(no);
    }
    if (verificar("INS_LA")) {
        return instrucao_la(no);
    }
    if (pertence(OPS_MEMORIA)) {
        return instrucao_memoria(no);
    }
    if (pertence(OPS_DESVIO)) {
        return instrucao_desvio(no);
    }
    if (pertence(OPS_SALTO)) {
        return instrucao_salto(no);
    }
    if (pertence(OPS_SEM_OPERANDOS)) {
        return instrucao_sem_operandos(no);
    }
    erro_sintatico("instrução");
    return 0;
}

/* <instrucao_tres_registradores> ::= <op_tres_registradores> REG "," REG "," REG */
static int instrucao_tres_registradores(int pai) {
    int no = nao_terminal(pai, "<instrucao_tres_registradores>");
    return op_tres_registradores(no) && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "REG");
}

/* <op_tres_registradores> ::= "add" | "addu" | "sub" | "subu" | "mul" | "and" | "or" | "xor" | "nor" | "slt" */
static int op_tres_registradores(int pai) {
    int no = nao_terminal(pai, "<op_tres_registradores>");
    return terminal_do_grupo(no, OPS_TRES_REGISTRADORES,
                             "operação com três registradores");
}

/* <instrucao_dois_registradores> ::= <op_dois_registradores> REG "," REG */
static int instrucao_dois_registradores(int pai) {
    int no = nao_terminal(pai, "<instrucao_dois_registradores>");
    return op_dois_registradores(no) && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "REG");
}

/* <op_dois_registradores> ::= "move" | "div" */
static int op_dois_registradores(int pai) {
    int no = nao_terminal(pai, "<op_dois_registradores>");
    return terminal_do_grupo(no, OPS_DOIS_REGISTRADORES, "move ou div");
}

/* <instrucao_um_registrador> ::= <op_um_registrador> REG */
static int instrucao_um_registrador(int pai) {
    int no = nao_terminal(pai, "<instrucao_um_registrador>");
    return op_um_registrador(no) && terminal(no, "REG");
}

/* <op_um_registrador> ::= "mflo" | "mfhi" | "jr" */
static int op_um_registrador(int pai) {
    int no = nao_terminal(pai, "<op_um_registrador>");
    return terminal_do_grupo(no, OPS_UM_REGISTRADOR, "mflo, mfhi ou jr");
}

/* <instrucao_imediato> ::= "addi" REG "," REG "," NUM_INT */
static int instrucao_imediato(int pai) {
    int no = nao_terminal(pai, "<instrucao_imediato>");
    return terminal(no, "INS_ADDI") && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "NUM_INT");
}

/* <instrucao_deslocamento> ::= <op_deslocamento> REG "," REG "," NUM_INT */
static int instrucao_deslocamento(int pai) {
    int no = nao_terminal(pai, "<instrucao_deslocamento>");
    return op_deslocamento(no) && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "NUM_INT");
}

/* <op_deslocamento> ::= "sll" | "srl" */
static int op_deslocamento(int pai) {
    int no = nao_terminal(pai, "<op_deslocamento>");
    return terminal_do_grupo(no, OPS_DESLOCAMENTO, "sll ou srl");
}

/* <instrucao_li> ::= "li" REG "," NUM_INT */
static int instrucao_li(int pai) {
    int no = nao_terminal(pai, "<instrucao_li>");
    return terminal(no, "INS_LI") && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "NUM_INT");
}

/* <instrucao_la> ::= "la" REG "," ID */
static int instrucao_la(int pai) {
    int no = nao_terminal(pai, "<instrucao_la>");
    return terminal(no, "INS_LA") && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "ID");
}

/* <instrucao_memoria> ::= <op_memoria> REG "," <endereco> */
static int instrucao_memoria(int pai) {
    int no = nao_terminal(pai, "<instrucao_memoria>");
    return op_memoria(no) && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && endereco(no);
}

/* <op_memoria> ::= "lw" | "sw" | "lb" | "sb" | "lh" | "sh" */
static int op_memoria(int pai) {
    int no = nao_terminal(pai, "<op_memoria>");
    return terminal_do_grupo(no, OPS_MEMORIA, "operação de acesso à memória");
}

/* <endereco> ::= ID | NUM_INT "(" REG ")" */
static int endereco(int pai) {
    int no = nao_terminal(pai, "<endereco>");

    if (verificar("ID")) {
        return terminal(no, "ID");
    }
    if (verificar("NUM_INT")) {
        return terminal(no, "NUM_INT") && terminal(no, "SMB_OPA") &&
               terminal(no, "REG") && terminal(no, "SMB_CPA");
    }
    erro_sintatico("ID ou NUM_INT (endereço)");
    return 0;
}

/* <instrucao_desvio> ::= <op_desvio> REG "," REG "," ID */
static int instrucao_desvio(int pai) {
    int no = nao_terminal(pai, "<instrucao_desvio>");
    return op_desvio(no) && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "REG") &&
           terminal(no, "SMB_COM") && terminal(no, "ID");
}

/* <op_desvio> ::= "beq" | "bne" | "blt" | "ble" | "bgt" | "bge" */
static int op_desvio(int pai) {
    int no = nao_terminal(pai, "<op_desvio>");
    return terminal_do_grupo(no, OPS_DESVIO, "operação de desvio condicional");
}

/* <instrucao_salto> ::= <op_salto> ID */
static int instrucao_salto(int pai) {
    int no = nao_terminal(pai, "<instrucao_salto>");
    return op_salto(no) && terminal(no, "ID");
}

/* <op_salto> ::= "j" | "jal" */
static int op_salto(int pai) {
    int no = nao_terminal(pai, "<op_salto>");
    return terminal_do_grupo(no, OPS_SALTO, "j ou jal");
}

/* <instrucao_sem_operandos> ::= "syscall" | "nop" */
static int instrucao_sem_operandos(int pai) {
    int no = nao_terminal(pai, "<instrucao_sem_operandos>");
    return terminal_do_grupo(no, OPS_SEM_OPERANDOS, "syscall ou nop");
}

int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros) {
    int resultado;

    if (tokens == NULL || arvore == NULL || erros == NULL) {
        return SINTATICO_FALHA;
    }

    liberar_estado();
    saida_erros = erros;
    total_erros = 0;
    falha = 0;

    if (!carregar_tokens(tokens)) {
        liberar_estado();
        return SINTATICO_FALHA;
    }

    programa();

    if (!falha) {
        if (total_erros == 0) {
            if (fprintf(erros, "Nenhum erro sintático encontrado.\n") < 0 ||
                !escrever_dot(arvore)) {
                falha = 1;
            }
        } else if (fprintf(erros, "\nTotal de erros sintáticos: %d\n", total_erros) < 0) {
            falha = 1;
        }
    }

    if (ferror(erros) || ferror(arvore)) {
        falha = 1;
    }

    if (falha) {
        resultado = SINTATICO_FALHA;
    } else if (total_erros == 0) {
        resultado = SINTATICO_VALIDO;
    } else {
        resultado = SINTATICO_INVALIDO;
    }

    liberar_estado();
    return resultado;
}
