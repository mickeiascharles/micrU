#include "orquestrador.h"

#include <stdio.h>
#include <string.h>

#include "lexico.h"
#include "preprocessador.h"
#include "sintatico.h"

#define TAMANHO_CAMINHO 1024

typedef enum {
    ETAPA_CONCLUIDA,
    ETAPA_REJEITADA,
    ETAPA_FALHOU
} ResultadoEtapa;

typedef enum {
    ART_PRE,
    ART_LEX,
    ART_TS,
    ART_LEX_ERR,
    ART_SIN_ERR,
    ART_DOT,
    TOTAL_ARTEFATOS
} Artefato;

typedef struct {
    const char *extensao;
    const char *modo;
} DescricaoArtefato;

static const DescricaoArtefato ARTEFATOS[TOTAL_ARTEFATOS] = {
    {".pre", "w+"},
    {".lex", "w+"},
    {".ts", "w"},
    {".lex.err", "w"},
    {".sin.err", "w"},
    {".dot", "w"}
};

typedef struct {
    const char *caminho_entrada;
    FILE *entrada;
    char caminhos[TOTAL_ARTEFATOS][TAMANHO_CAMINHO];
    FILE *arquivos[TOTAL_ARTEFATOS];
} Contexto;

typedef ResultadoEtapa (*ExecutarEtapa)(Contexto *contexto);

typedef struct {
    const char *nome;
    ExecutarEtapa executar;
} Etapa;

static int rebobinar(Contexto *contexto, Artefato artefato) {
    FILE *arquivo = contexto->arquivos[artefato];
    if (fflush(arquivo) != 0 || fseek(arquivo, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "Erro: falha ao reposicionar '%s'.\n",
                contexto->caminhos[artefato]);
        return 0;
    }
    return 1;
}

/* Etapa 1: remove comentarios e normaliza espacos (.asm -> .pre). */
static ResultadoEtapa etapa_preprocessamento(Contexto *contexto) {
    if (!PreProcessamento(contexto->entrada, contexto->arquivos[ART_PRE])) {
        return ETAPA_FALHOU;
    }
    return rebobinar(contexto, ART_PRE) ? ETAPA_CONCLUIDA : ETAPA_FALHOU;
}

/* Etapa 2: reconhece os tokens (.pre -> .lex, .ts, .lex.err). */
static ResultadoEtapa etapa_lexica(Contexto *contexto) {
    int resultado = AnaliseLexica(contexto->arquivos[ART_PRE],
                                  contexto->arquivos[ART_LEX],
                                  contexto->arquivos[ART_TS],
                                  contexto->arquivos[ART_LEX_ERR]);

    if (resultado == LEXICO_FALHA) {
        return ETAPA_FALHOU;
    }
    if (resultado == LEXICO_COM_ERROS) {
        fprintf(contexto->arquivos[ART_SIN_ERR],
                "Análise sintática não executada: existem erros léxicos "
                "(consulte %s).\n", contexto->caminhos[ART_LEX_ERR]);
        printf("ANÁLISE SINTÁTICA NÃO EXECUTADA: EXISTEM ERROS LÉXICOS.\n");
        return ETAPA_REJEITADA;
    }
    return rebobinar(contexto, ART_LEX) ? ETAPA_CONCLUIDA : ETAPA_FALHOU;
}

/* Etapa 3: valida a gramatica e gera a arvore (.lex -> .sin.err, .dot). */
static ResultadoEtapa etapa_sintatica(Contexto *contexto) {
    int resultado = AnaliseSintatica(contexto->arquivos[ART_LEX],
                                     contexto->arquivos[ART_DOT],
                                     contexto->arquivos[ART_SIN_ERR]);

    if (resultado == SINTATICO_VALIDO) {
        printf("PROGRAMA SINTATICAMENTE VÁLIDO\n");
        return ETAPA_CONCLUIDA;
    }
    if (resultado == SINTATICO_INVALIDO) {
        printf("PROGRAMA SINTATICAMENTE INVÁLIDO\n");
        printf("Erros registrados em %s.\n", contexto->caminhos[ART_SIN_ERR]);
        return ETAPA_REJEITADA;
    }
    return ETAPA_FALHOU;
}

static const Etapa PIPELINE[] = {
    {"pré-processamento", etapa_preprocessamento},
    {"análise léxica", etapa_lexica},
    {"análise sintática", etapa_sintatica}
};

static int montar_caminhos(Contexto *contexto, const char *prefixo) {
    size_t i;

    if (prefixo[0] == '\0') {
        return 0;
    }
    for (i = 0U; i < TOTAL_ARTEFATOS; ++i) {
        int escritos = snprintf(contexto->caminhos[i], TAMANHO_CAMINHO, "%s%s",
                                prefixo, ARTEFATOS[i].extensao);
        if (escritos < 0 || escritos >= TAMANHO_CAMINHO) {
            return 0;
        }
    }
    return 1;
}

static int entrada_seria_sobrescrita(const Contexto *contexto) {
    size_t i;

    for (i = 0U; i < TOTAL_ARTEFATOS; ++i) {
        if (strcmp(contexto->caminho_entrada, contexto->caminhos[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int abrir_arquivos(Contexto *contexto) {
    size_t i;

    contexto->entrada = fopen(contexto->caminho_entrada, "r");
    if (contexto->entrada == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s'.\n",
                contexto->caminho_entrada);
        return 0;
    }
    for (i = 0U; i < TOTAL_ARTEFATOS; ++i) {
        contexto->arquivos[i] = fopen(contexto->caminhos[i], ARTEFATOS[i].modo);
        if (contexto->arquivos[i] == NULL) {
            fprintf(stderr, "Erro: nao foi possivel criar '%s'.\n",
                    contexto->caminhos[i]);
            return 0;
        }
    }
    return 1;
}

static int fechar_arquivos(Contexto *contexto) {
    int ok = 1;
    size_t i;

    if (contexto->entrada != NULL) {
        fclose(contexto->entrada);
        contexto->entrada = NULL;
    }
    for (i = 0U; i < TOTAL_ARTEFATOS; ++i) {
        if (contexto->arquivos[i] != NULL) {
            if (fclose(contexto->arquivos[i]) != 0) {
                fprintf(stderr, "Erro: falha ao gravar '%s'.\n",
                        contexto->caminhos[i]);
                ok = 0;
            }
            contexto->arquivos[i] = NULL;
        }
    }
    return ok;
}

static int executar_pipeline(Contexto *contexto) {
    size_t i;

    for (i = 0U; i < sizeof(PIPELINE) / sizeof(PIPELINE[0]); ++i) {
        ResultadoEtapa resultado = PIPELINE[i].executar(contexto);

        if (resultado == ETAPA_REJEITADA) {
            return MICRU_REJEITADO;
        }
        if (resultado == ETAPA_FALHOU) {
            fprintf(stderr, "Erro: falha operacional durante %s.\n",
                    PIPELINE[i].nome);
            return MICRU_FALHA;
        }
    }
    return MICRU_VALIDO;
}

int Orquestrar(const char *caminho_entrada, const char *prefixo) {
    Contexto contexto;
    int codigo = MICRU_FALHA;

    memset(&contexto, 0, sizeof(contexto));
    contexto.caminho_entrada = caminho_entrada;

    if (!montar_caminhos(&contexto, prefixo)) {
        fprintf(stderr, "Erro: prefixo de saida invalido ou muito longo.\n");
        return MICRU_FALHA;
    }
    if (entrada_seria_sobrescrita(&contexto)) {
        fprintf(stderr, "Erro: o arquivo de entrada seria sobrescrito.\n");
        return MICRU_FALHA;
    }

    if (abrir_arquivos(&contexto)) {
        codigo = executar_pipeline(&contexto);
    }
    if (!fechar_arquivos(&contexto)) {
        codigo = MICRU_FALHA;
    }
    return codigo;
}
