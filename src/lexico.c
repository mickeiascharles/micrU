#include "lexico.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CAPACIDADE_INICIAL 128

typedef struct {
    const char *lexema;
    const char *token;
} PalavraReservada;

typedef struct {
    char tipo[TAMANHO_NOME_TOKEN];
    char lexema[TAMANHO_LEXEMA_TOKEN];
    int linha;
    int coluna;
} ErroLexico;

typedef struct {
    const unsigned char *fonte;
    size_t tamanho;
    size_t indice;
    int linha;
    int coluna;
} Leitor;

typedef enum {
    Q_INICIAL,
    Q_IDENTIFICADOR,
    Q_IDENTIFICADOR_MALFORMADO,
    Q_DIRETIVA,
    Q_REGISTRADOR,
    Q_NUMERO_DECIMAL,
    Q_PREFIXO_HEXADECIMAL,
    Q_NUMERO_HEXADECIMAL,
    Q_NUMERO_MALFORMADO,
    Q_STRING,
    Q_ESCAPE_STRING,
    Q_ESCAPE_STRING_INVALIDO,
    Q_COMENTARIO,
    Q_SIMBOLO,
    Q_ERRO
} EstadoAFD;

static const PalavraReservada DIRETIVAS[] = {
    {".data", "DIR_DATA"},
    {".text", "DIR_TEXT"},
    {".word", "DIR_WORD"},
    {".half", "DIR_HALF"},
    {".byte", "DIR_BYTE"},
    {".space", "DIR_SPACE"},
    {".ascii", "DIR_ASCII"},
    {".asciiz", "DIR_ASCIIZ"},
    {".globl", "DIR_GLOBL"},
    {".align", "DIR_ALIGN"}
};

static const PalavraReservada INSTRUCOES[] = {
    {"li", "INS_LI"}, {"la", "INS_LA"}, {"move", "INS_MOVE"},
    {"lw", "INS_LW"}, {"sw", "INS_SW"}, {"lb", "INS_LB"},
    {"sb", "INS_SB"}, {"lh", "INS_LH"}, {"sh", "INS_SH"},
    {"add", "INS_ADD"}, {"addi", "INS_ADDI"}, {"addu", "INS_ADDU"},
    {"sub", "INS_SUB"}, {"subu", "INS_SUBU"}, {"mul", "INS_MUL"},
    {"div", "INS_DIV"}, {"mflo", "INS_MFLO"}, {"mfhi", "INS_MFHI"},
    {"and", "INS_AND"}, {"or", "INS_OR"}, {"xor", "INS_XOR"},
    {"nor", "INS_NOR"}, {"sll", "INS_SLL"}, {"srl", "INS_SRL"},
    {"slt", "INS_SLT"}, {"beq", "INS_BEQ"}, {"bne", "INS_BNE"},
    {"blt", "INS_BLT"}, {"ble", "INS_BLE"}, {"bgt", "INS_BGT"},
    {"bge", "INS_BGE"}, {"j", "INS_J"}, {"jal", "INS_JAL"},
    {"jr", "INS_JR"}, {"syscall", "INS_SYSCALL"}, {"nop", "INS_NOP"}
};

static const char *REGISTRADORES_NOMEADOS[] = {
    "$zero", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$t8", "$t9", "$s0", "$s1", "$s2", "$s3", "$s4", "$s5",
    "$s6", "$s7", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

static Simbolo *tabela_simbolos = NULL;
static size_t total_simbolos = 0;
static size_t capacidade_simbolos = 0;
static ErroLexico *erros = NULL;
static size_t total_erros = 0;
static size_t capacidade_erros = 0;
static int falha_analise = 0;

static size_t quantidade_diretivas(void) {
    return sizeof(DIRETIVAS) / sizeof(DIRETIVAS[0]);
}

static size_t quantidade_instrucoes(void) {
    return sizeof(INSTRUCOES) / sizeof(INSTRUCOES[0]);
}

static size_t quantidade_registradores_nomeados(void) {
    return sizeof(REGISTRADORES_NOMEADOS) / sizeof(REGISTRADORES_NOMEADOS[0]);
}

static void copiar_texto(char *destino, size_t capacidade, const char *origem) {
    if (capacidade == 0U) {
        return;
    }
    strncpy(destino, origem, capacidade - 1U);
    destino[capacidade - 1U] = '\0';
}

static int e_letra_ascii(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int e_digito_ascii(int c) {
    return c >= '0' && c <= '9';
}

static int e_hexadecimal_ascii(int c) {
    return e_digito_ascii(c) || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int e_espaco(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\f' || c == '\v';
}

static int e_fronteira(int c) {
    return c == EOF || e_espaco(c) || c == ',' || c == ':' ||
           c == '(' || c == ')' || c == '#' || c == '"';
}

static int e_caractere_identificador(int c) {
    return e_letra_ascii(c) || e_digito_ascii(c) || c == '_';
}

static void para_minusculas(char *texto) {
    size_t i;
    for (i = 0U; texto[i] != '\0'; ++i) {
        texto[i] = (char) tolower((unsigned char) texto[i]);
    }
}

static const char *buscar_reservada(const PalavraReservada *palavras,
                                    size_t quantidade,
                                    const char *lexema) {
    char minusculo[TAMANHO_LEXEMA_TOKEN];
    size_t i;

    copiar_texto(minusculo, sizeof(minusculo), lexema);
    para_minusculas(minusculo);

    for (i = 0U; i < quantidade; ++i) {
        if (strcmp(minusculo, palavras[i].lexema) == 0) {
            return palavras[i].token;
        }
    }
    return NULL;
}

static int registrador_valido(const char *lexema) {
    char minusculo[TAMANHO_LEXEMA_TOKEN];
    size_t i;

    copiar_texto(minusculo, sizeof(minusculo), lexema);
    para_minusculas(minusculo);

    for (i = 0U; i < quantidade_registradores_nomeados(); ++i) {
        if (strcmp(minusculo, REGISTRADORES_NOMEADOS[i]) == 0) {
            return 1;
        }
    }

    if (minusculo[0] == '$' && e_digito_ascii((unsigned char) minusculo[1])) {
        char *fim = NULL;
        long numero = strtol(minusculo + 1, &fim, 10);
        if (*fim == '\0' && numero >= 0 && numero <= 31) {
            char forma_canonica[8];
            int escritos = snprintf(forma_canonica, sizeof(forma_canonica),
                                    "$%ld", numero);
            return escritos > 0 && (size_t) escritos < sizeof(forma_canonica) &&
                   strcmp(forma_canonica, minusculo) == 0;
        }
    }

    return 0;
}

static int garantir_simbolos(size_t necessaria) {
    Simbolo *nova_tabela;
    size_t nova_capacidade;

    if (necessaria <= capacidade_simbolos) {
        return 1;
    }

    nova_capacidade = capacidade_simbolos == 0U
                          ? CAPACIDADE_INICIAL
                          : capacidade_simbolos * 2U;
    while (nova_capacidade < necessaria) {
        nova_capacidade *= 2U;
    }

    nova_tabela = (Simbolo *) realloc(tabela_simbolos,
                                      nova_capacidade * sizeof(*nova_tabela));
    if (nova_tabela == NULL) {
        return 0;
    }

    tabela_simbolos = nova_tabela;
    capacidade_simbolos = nova_capacidade;
    return 1;
}

static int adicionar_simbolo(const char *lexema, const char *categoria,
                             int linha, int coluna) {
    Simbolo *simbolo;
    size_t i;

    for (i = 0U; i < total_simbolos; ++i) {
        if (strcmp(tabela_simbolos[i].lexema, lexema) == 0) {
            if (tabela_simbolos[i].primeiraLinha == 0 && linha > 0) {
                tabela_simbolos[i].primeiraLinha = linha;
                tabela_simbolos[i].primeiraColuna = coluna;
            }
            return 1;
        }
    }

    if (!garantir_simbolos(total_simbolos + 1U)) {
        return 0;
    }

    simbolo = &tabela_simbolos[total_simbolos++];
    copiar_texto(simbolo->lexema, sizeof(simbolo->lexema), lexema);
    copiar_texto(simbolo->categoria, sizeof(simbolo->categoria), categoria);
    simbolo->primeiraLinha = linha;
    simbolo->primeiraColuna = coluna;
    return 1;
}

static int inicializar_tabela_simbolos(void) {
    size_t i;
    int numero;

    free(tabela_simbolos);
    tabela_simbolos = NULL;
    total_simbolos = 0U;
    capacidade_simbolos = 0U;

    for (i = 0U; i < quantidade_diretivas(); ++i) {
        if (!adicionar_simbolo(DIRETIVAS[i].lexema, "diretiva", 0, 0)) {
            return 0;
        }
    }
    for (i = 0U; i < quantidade_instrucoes(); ++i) {
        if (!adicionar_simbolo(INSTRUCOES[i].lexema, "instrução", 0, 0)) {
            return 0;
        }
    }
    for (i = 0U; i < quantidade_registradores_nomeados(); ++i) {
        if (!adicionar_simbolo(REGISTRADORES_NOMEADOS[i], "registrador", 0, 0)) {
            return 0;
        }
    }
    for (numero = 0; numero <= 31; ++numero) {
        char registrador[8];
        int escritos = snprintf(registrador, sizeof(registrador), "$%d", numero);
        if (escritos < 0 || (size_t) escritos >= sizeof(registrador) ||
            !adicionar_simbolo(registrador, "registrador", 0, 0)) {
            return 0;
        }
    }
    return 1;
}

static int garantir_erros(size_t necessaria) {
    ErroLexico *novos_erros;
    size_t nova_capacidade;

    if (necessaria <= capacidade_erros) {
        return 1;
    }

    nova_capacidade = capacidade_erros == 0U
                          ? CAPACIDADE_INICIAL
                          : capacidade_erros * 2U;
    while (nova_capacidade < necessaria) {
        nova_capacidade *= 2U;
    }

    novos_erros = (ErroLexico *) realloc(erros,
                                         nova_capacidade * sizeof(*novos_erros));
    if (novos_erros == NULL) {
        return 0;
    }
    erros = novos_erros;
    capacidade_erros = nova_capacidade;
    return 1;
}

static int registrar_erro(const Token *token) {
    ErroLexico *erro;

    if (!garantir_erros(total_erros + 1U)) {
        return 0;
    }
    erro = &erros[total_erros++];
    copiar_texto(erro->tipo, sizeof(erro->tipo), token->nome);
    copiar_texto(erro->lexema, sizeof(erro->lexema), token->lexema);
    erro->linha = token->linha;
    erro->coluna = token->coluna;
    return 1;
}

static unsigned char *ler_entrada(FILE *entrada, size_t *tamanho) {
    unsigned char *dados;
    size_t capacidade = 4096U;
    size_t usados = 0U;

    dados = (unsigned char *) malloc(capacidade + 1U);
    if (dados == NULL) {
        return NULL;
    }

    for (;;) {
        size_t livres = capacidade - usados;
        size_t lidos = fread(dados + usados, 1U, livres, entrada);
        usados += lidos;

        if (lidos < livres) {
            if (ferror(entrada)) {
                free(dados);
                return NULL;
            }
            break;
        }

        if (capacidade > (size_t) -1 / 2U) {
            free(dados);
            return NULL;
        }
        capacidade *= 2U;
        {
            unsigned char *novos_dados =
                (unsigned char *) realloc(dados, capacidade + 1U);
            if (novos_dados == NULL) {
                free(dados);
                return NULL;
            }
            dados = novos_dados;
        }
    }

    dados[usados] = '\0';
    *tamanho = usados;
    return dados;
}

static int atual(const Leitor *leitor) {
    if (leitor->indice >= leitor->tamanho) {
        return EOF;
    }
    return leitor->fonte[leitor->indice];
}

static int proximo(const Leitor *leitor) {
    if (leitor->indice + 1U >= leitor->tamanho) {
        return EOF;
    }
    return leitor->fonte[leitor->indice + 1U];
}

static int avancar(Leitor *leitor) {
    int c;

    if (leitor->indice >= leitor->tamanho) {
        return EOF;
    }

    c = leitor->fonte[leitor->indice++];
    if (c == '\r') {
        if (leitor->indice < leitor->tamanho &&
            leitor->fonte[leitor->indice] == '\n') {
            ++leitor->indice;
        }
        ++leitor->linha;
        leitor->coluna = 1;
    } else if (c == '\n') {
        ++leitor->linha;
        leitor->coluna = 1;
    } else {
        ++leitor->coluna;
    }
    return c;
}

static void iniciar_token(Token *token, const Leitor *leitor) {
    token->nome[0] = '\0';
    token->lexema[0] = '\0';
    token->linha = leitor->linha;
    token->coluna = leitor->coluna;
}

static void adicionar_caractere(Token *token, size_t *tamanho_lexema,
                                int c, int *estourou) {
    if (*tamanho_lexema + 1U < sizeof(token->lexema)) {
        token->lexema[*tamanho_lexema] = (char) c;
        ++(*tamanho_lexema);
        token->lexema[*tamanho_lexema] = '\0';
    } else {
        *estourou = 1;
    }
}

static void marcar_lexema_truncado(Token *token) {
    size_t tamanho = strlen(token->lexema);
    if (tamanho >= 3U) {
        token->lexema[tamanho - 3U] = '.';
        token->lexema[tamanho - 2U] = '.';
        token->lexema[tamanho - 1U] = '.';
    }
}

static void definir_token(Token *token, const char *nome) {
    copiar_texto(token->nome, sizeof(token->nome), nome);
}

static void consumir_ignorados(Leitor *leitor) {
    for (;;) {
        int c = atual(leitor);
        if (e_espaco(c)) {
            avancar(leitor);
        } else if (c == '#') {
            while (atual(leitor) != EOF && atual(leitor) != '\n' &&
                   atual(leitor) != '\r') {
                avancar(leitor);
            }
        } else {
            break;
        }
    }
}

static Token reconhecer_token(Leitor *leitor) {
    Token token;
    EstadoAFD estado = Q_INICIAL;
    size_t tamanho_lexema = 0U;
    int estourou = 0;
    int c;

    consumir_ignorados(leitor);
    iniciar_token(&token, leitor);
    c = atual(leitor);

    if (c == EOF) {
        definir_token(&token, "TK_EOF");
        copiar_texto(token.lexema, sizeof(token.lexema), "EOF");
        return token;
    }

    if (c == ',' || c == ':' || c == '(' || c == ')') {
        estado = Q_SIMBOLO;
        adicionar_caractere(&token, &tamanho_lexema, avancar(leitor), &estourou);
        if (c == ',') {
            definir_token(&token, "SMB_COM");
        } else if (c == ':') {
            definir_token(&token, "SMB_COL");
        } else if (c == '(') {
            definir_token(&token, "SMB_OPA");
        } else {
            definir_token(&token, "SMB_CPA");
        }
        (void) estado;
        return token;
    }

    if (e_letra_ascii(c) || c == '_') {
        const char *token_instrucao;
        estado = Q_IDENTIFICADOR;
        while (!e_fronteira(atual(leitor))) {
            int lido = avancar(leitor);
            adicionar_caractere(&token, &tamanho_lexema, lido, &estourou);
            if (!e_caractere_identificador(lido)) {
                estado = Q_IDENTIFICADOR_MALFORMADO;
            }
        }

        if (estourou) {
            marcar_lexema_truncado(&token);
            definir_token(&token, "ERRO_LEXEMA_MUITO_LONGO");
        } else if (estado == Q_IDENTIFICADOR_MALFORMADO) {
            definir_token(&token, "ERRO_IDENTIFICADOR_MALFORMADO");
        } else {
            token_instrucao = buscar_reservada(INSTRUCOES,
                                                quantidade_instrucoes(),
                                                token.lexema);
            if (token_instrucao != NULL) {
                para_minusculas(token.lexema);
                definir_token(&token, token_instrucao);
                if (!adicionar_simbolo(token.lexema, "instrução",
                                       token.linha, token.coluna)) {
                    falha_analise = 1;
                }
            } else {
                definir_token(&token, "ID");
                if (!adicionar_simbolo(token.lexema, "identificador/rótulo",
                                       token.linha, token.coluna)) {
                    falha_analise = 1;
                }
            }
        }
        return token;
    }

    if (c == '.') {
        const char *token_diretiva;
        estado = Q_DIRETIVA;
        while (!e_fronteira(atual(leitor))) {
            adicionar_caractere(&token, &tamanho_lexema, avancar(leitor),
                                &estourou);
        }

        token_diretiva = estourou
                             ? NULL
                             : buscar_reservada(DIRETIVAS,
                                                quantidade_diretivas(),
                                                token.lexema);
        if (estourou) {
            marcar_lexema_truncado(&token);
            definir_token(&token, "ERRO_LEXEMA_MUITO_LONGO");
        } else if (token_diretiva == NULL) {
            definir_token(&token, "ERRO_DIRETIVA_INVALIDA");
        } else {
            para_minusculas(token.lexema);
            definir_token(&token, token_diretiva);
            if (!adicionar_simbolo(token.lexema, "diretiva",
                                   token.linha, token.coluna)) {
                falha_analise = 1;
            }
        }
        (void) estado;
        return token;
    }

    if (c == '$') {
        estado = Q_REGISTRADOR;
        while (!e_fronteira(atual(leitor))) {
            adicionar_caractere(&token, &tamanho_lexema, avancar(leitor),
                                &estourou);
        }

        if (estourou) {
            marcar_lexema_truncado(&token);
            definir_token(&token, "ERRO_LEXEMA_MUITO_LONGO");
        } else if (!registrador_valido(token.lexema)) {
            definir_token(&token, "ERRO_REGISTRADOR_INVALIDO");
        } else {
            para_minusculas(token.lexema);
            definir_token(&token, "REG");
            if (!adicionar_simbolo(token.lexema, "registrador",
                                   token.linha, token.coluna)) {
                falha_analise = 1;
            }
        }
        (void) estado;
        return token;
    }

    if (e_digito_ascii(c) || (c == '-' && e_digito_ascii(proximo(leitor)))) {
        int hexadecimal = 0;
        int tem_digito_hexadecimal = 0;
        int sufixo_de_identificador = 1;
        int tem_letra_ou_sublinhado = 0;

        estado = Q_NUMERO_DECIMAL;
        if (c == '-') {
            adicionar_caractere(&token, &tamanho_lexema, avancar(leitor),
                                &estourou);
        }

        if (atual(leitor) == '0' &&
            (proximo(leitor) == 'x' || proximo(leitor) == 'X')) {
            estado = Q_PREFIXO_HEXADECIMAL;
            hexadecimal = 1;
            adicionar_caractere(&token, &tamanho_lexema, avancar(leitor),
                                &estourou);
            adicionar_caractere(&token, &tamanho_lexema, avancar(leitor),
                                &estourou);

            while (!e_fronteira(atual(leitor))) {
                int lido = avancar(leitor);
                adicionar_caractere(&token, &tamanho_lexema, lido, &estourou);
                if (e_hexadecimal_ascii(lido)) {
                    tem_digito_hexadecimal = 1;
                    if (estado != Q_NUMERO_MALFORMADO) {
                        estado = Q_NUMERO_HEXADECIMAL;
                    }
                } else {
                    estado = Q_NUMERO_MALFORMADO;
                }
            }
        } else {
            while (!e_fronteira(atual(leitor))) {
                int lido = avancar(leitor);
                adicionar_caractere(&token, &tamanho_lexema, lido, &estourou);
                if (!e_digito_ascii(lido)) {
                    estado = Q_NUMERO_MALFORMADO;
                    if (!e_caractere_identificador(lido)) {
                        sufixo_de_identificador = 0;
                    }
                    if (e_letra_ascii(lido) || lido == '_') {
                        tem_letra_ou_sublinhado = 1;
                    }
                }
            }
        }

        if (estourou) {
            marcar_lexema_truncado(&token);
            definir_token(&token, "ERRO_LEXEMA_MUITO_LONGO");
        } else if (hexadecimal && !tem_digito_hexadecimal) {
            definir_token(&token, "ERRO_NUMERO_MALFORMADO");
        } else if (estado == Q_NUMERO_MALFORMADO) {
            if (!hexadecimal && sufixo_de_identificador &&
                tem_letra_ou_sublinhado) {
                definir_token(&token, "ERRO_IDENTIFICADOR_MALFORMADO");
            } else {
                definir_token(&token, "ERRO_NUMERO_MALFORMADO");
            }
        } else {
            definir_token(&token, "NUM_INT");
        }
        return token;
    }

    if (c == '"') {
        int fechou = 0;
        int escape_invalido = 0;

        estado = Q_STRING;
        adicionar_caractere(&token, &tamanho_lexema, avancar(leitor),
                            &estourou);
        while (atual(leitor) != EOF && atual(leitor) != '\n' &&
               atual(leitor) != '\r') {
            int lido = avancar(leitor);
            adicionar_caractere(&token, &tamanho_lexema, lido, &estourou);

            if (estado == Q_ESCAPE_STRING) {
                if (lido != 'n' && lido != 't' && lido != '"' &&
                    lido != '\\' && lido != '0') {
                    escape_invalido = 1;
                    estado = Q_ESCAPE_STRING_INVALIDO;
                } else {
                    estado = Q_STRING;
                }
            } else if (lido == '\\') {
                estado = Q_ESCAPE_STRING;
            } else if (lido == '"') {
                fechou = 1;
                break;
            } else if (estado == Q_ESCAPE_STRING_INVALIDO) {
                estado = Q_STRING;
            }
        }

        if (estourou) {
            marcar_lexema_truncado(&token);
            definir_token(&token, "ERRO_LEXEMA_MUITO_LONGO");
        } else if (!fechou) {
            definir_token(&token, "ERRO_STRING_NAO_FECHADA");
        } else if (escape_invalido) {
            definir_token(&token, "ERRO_SEQUENCIA_ESCAPE_INVALIDA");
        } else {
            definir_token(&token, "STRING");
        }
        return token;
    }

    estado = Q_ERRO;
    adicionar_caractere(&token, &tamanho_lexema, avancar(leitor), &estourou);
    definir_token(&token, "ERRO_CARACTERE_INVALIDO");
    (void) estado;
    return token;
}

static void reiniciar_erros(void) {
    free(erros);
    erros = NULL;
    total_erros = 0U;
    capacidade_erros = 0U;
}

static void liberar_tabela_simbolos(void) {
    free(tabela_simbolos);
    tabela_simbolos = NULL;
    total_simbolos = 0U;
    capacidade_simbolos = 0U;
}

static void emitir_tokens(Leitor *leitor, FILE *saida) {
    for (;;) {
        Token token = reconhecer_token(leitor);
        if (fprintf(saida, "<%s, %s> %d %d\n", token.nome, token.lexema,
                    token.linha, token.coluna) < 0) {
            falha_analise = 1;
            return;
        }
        if (strncmp(token.nome, "ERRO_", 5U) == 0 && !registrar_erro(&token)) {
            falha_analise = 1;
            return;
        }
        if (strcmp(token.nome, "TK_EOF") == 0) {
            return;
        }
    }
}

static void escrever_tabela_simbolos(FILE *saida) {
    size_t i;

    if (fprintf(saida, "LEXEMA,CATEGORIA,LINHA,COLUNA\n") < 0) {
        falha_analise = 1;
        return;
    }
    for (i = 0U; i < total_simbolos; ++i) {
        if (fprintf(saida, "%s,%s,%d,%d\n",
                    tabela_simbolos[i].lexema,
                    tabela_simbolos[i].categoria,
                    tabela_simbolos[i].primeiraLinha,
                    tabela_simbolos[i].primeiraColuna) < 0) {
            falha_analise = 1;
            return;
        }
    }
}

static void escrever_erros(FILE *saida) {
    size_t i;

    if (total_erros == 0U) {
        if (fprintf(saida, "Nenhum erro léxico encontrado.\n") < 0) {
            falha_analise = 1;
        }
        return;
    }
    for (i = 0U; i < total_erros; ++i) {
        if (fprintf(saida, "<%s, %s> %d %d\n", erros[i].tipo, erros[i].lexema,
                    erros[i].linha, erros[i].coluna) < 0) {
            falha_analise = 1;
            return;
        }
    }
}

int AnaliseLexica(FILE *entrada, FILE *tokens, FILE *tabela, FILE *saida_erros) {
    unsigned char *fonte;
    size_t tamanho_fonte = 0U;
    Leitor leitor;
    int resultado;

    if (entrada == NULL || tokens == NULL || tabela == NULL ||
        saida_erros == NULL) {
        return LEXICO_FALHA;
    }

    falha_analise = 0;
    reiniciar_erros();
    if (!inicializar_tabela_simbolos()) {
        liberar_tabela_simbolos();
        return LEXICO_FALHA;
    }

    fonte = ler_entrada(entrada, &tamanho_fonte);
    if (fonte == NULL) {
        liberar_tabela_simbolos();
        return LEXICO_FALHA;
    }

    leitor.fonte = fonte;
    leitor.tamanho = tamanho_fonte;
    leitor.indice = 0U;
    leitor.linha = 1;
    leitor.coluna = 1;

    emitir_tokens(&leitor, tokens);
    if (!falha_analise) {
        escrever_tabela_simbolos(tabela);
    }
    if (!falha_analise) {
        escrever_erros(saida_erros);
    }
    if (ferror(tokens) || ferror(tabela) || ferror(saida_erros)) {
        falha_analise = 1;
    }

    if (falha_analise) {
        resultado = LEXICO_FALHA;
    } else if (total_erros > 0U) {
        resultado = LEXICO_COM_ERROS;
    } else {
        resultado = LEXICO_VALIDO;
    }

    free(fonte);
    liberar_tabela_simbolos();
    reiniciar_erros();
    return resultado;
}
