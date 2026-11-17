#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

typedef struct {
    size_t clientes;
    size_t recursos;
    int *available;
    int *maximum;
    int *allocation;
    int *need;
} Estado;

typedef enum {
    PEDIDO_ACEITO,
    EXCEDE_MAXIMO,
    RECURSOS_INSUFICIENTES,
    ESTADO_INSEGURO,
    ERRO_MEMORIA
} ResultadoPedido;

/* Retorna 1 se seguro, 0 se inseguro e -1 em falha de memoria.
   A simulacao nao modifica as matrizes reais. */
static int estado_seguro(const Estado *estado)
{
    int *work = malloc(estado->recursos * sizeof *work);
    unsigned char *finish = calloc(estado->clientes, sizeof *finish);
    if (work == NULL || finish == NULL) {
        free(work);
        free(finish);
        return -1;
    }

    memcpy(work, estado->available, estado->recursos * sizeof *work);
    size_t concluidos = 0;
    int avancou;
    do {
        avancou = 0;
        for (size_t i = 0; i < estado->clientes; i++) {
            if (finish[i])
                continue;
            size_t j;
            for (j = 0; j < estado->recursos; j++) {
                if (estado->need[i * estado->recursos + j] > work[j])
                    break;
            }
            if (j != estado->recursos)
                continue;
            for (j = 0; j < estado->recursos; j++)
                work[j] += estado->allocation[i * estado->recursos + j];
            finish[i] = 1;
            concluidos++;
            avancou = 1;
        }
    } while (avancou && concluidos < estado->clientes);

    free(finish);
    free(work);
    return concluidos == estado->clientes;
}

static ResultadoPedido solicitar_recursos(Estado *estado, size_t cliente, const int *pedido)
{
    size_t base = cliente * estado->recursos;
    for (size_t j = 0; j < estado->recursos; j++) {
        if (pedido[j] > estado->need[base + j])
            return EXCEDE_MAXIMO;
    }
    for (size_t j = 0; j < estado->recursos; j++) {
        if (pedido[j] > estado->available[j])
            return RECURSOS_INSUFICIENTES;
    }
    for (size_t j = 0; j < estado->recursos; j++) {
        estado->available[j] -= pedido[j];
        estado->allocation[base + j] += pedido[j];
        estado->need[base + j] -= pedido[j];
    }
    int seguro = estado_seguro(estado);
    if (seguro == 1)
        return PEDIDO_ACEITO;

    /* Uma recusa ou falha na simulacao deve restaurar todo o estado. */
    for (size_t j = 0; j < estado->recursos; j++) {
        estado->available[j] += pedido[j];
        estado->allocation[base + j] -= pedido[j];
        estado->need[base + j] += pedido[j];
    }
    return seguro < 0 ? ERRO_MEMORIA : ESTADO_INSEGURO;
}

static int liberar_recursos(Estado *estado, size_t cliente, const int *liberacao)
{
    size_t base = cliente * estado->recursos;
    for (size_t j = 0; j < estado->recursos; j++) {
        if (liberacao[j] > estado->allocation[base + j])
            return 0;
    }
    for (size_t j = 0; j < estado->recursos; j++) {
        estado->allocation[base + j] -= liberacao[j];
        estado->need[base + j] += liberacao[j];
        estado->available[j] += liberacao[j];
    }
    return 1;
}

static int *ler_clientes(const char *nome, size_t recursos, size_t *clientes)
{
    FILE *arquivo = fopen(nome, "r");
    if (arquivo == NULL) {
        perror(nome);
        return NULL;
    }

    char *linha = NULL;
    size_t capacidade = 0;
    int *maximum = NULL;
    ssize_t tamanho;
    *clientes = 0;

    while ((tamanho = getline(&linha, &capacidade, arquivo)) != -1) {
        char *cursor = linha;

        /* Rejeita caracteres nulos dentro da linha. */
        if (memchr(linha, '\0', (size_t) tamanho) != NULL)
            goto formato_invalido;

        /* Ignora o BOM UTF-8 presente no arquivo fornecido. */
        if (*clientes == 0 && tamanho >= 3 &&
            memcmp(linha, "\xEF\xBB\xBF", 3) == 0)
            cursor += 3;

        if (*clientes >= SIZE_MAX / recursos / sizeof *maximum) {
            fprintf(stderr, "Matriz de clientes grande demais.\n");
            goto falha;
        }

        int *novo = realloc(maximum,
                            (*clientes + 1) * recursos * sizeof *maximum);
        if (novo == NULL) {
            fprintf(stderr, "Erro ao reservar memoria para os clientes.\n");
            goto falha;
        }
        maximum = novo;

        for (size_t j = 0; j < recursos; j++) {
            char *fim;
            errno = 0;
            long valor = strtol(cursor, &fim, 10);

            if (errno == ERANGE || fim == cursor ||
                valor < 0 || valor > INT_MAX)
                goto formato_invalido;

            while (isspace((unsigned char) *fim))
                fim++;

            if (j + 1 < recursos) {
                if (*fim != ',')
                    goto formato_invalido;
                cursor = fim + 1;
            } else if (*fim != '\0') {
                goto formato_invalido;
            }

            maximum[*clientes * recursos + j] = (int) valor;
        }

        (*clientes)++;
    }

    if (!feof(arquivo) || *clientes == 0) {
        fprintf(stderr, "Erro de leitura ou arquivo de clientes vazio.\n");
        goto falha;
    }

    free(linha);
    fclose(arquivo);
    return maximum;

formato_invalido:
    fprintf(stderr, "%s: linha %zu invalida; esperados %zu inteiros "
                    "nao negativos separados por virgula.\n",
            nome, *clientes + 1, recursos);
falha:
    free(linha);
    free(maximum);
    fclose(arquivo);
    *clientes = 0;
    return NULL;
}

static void imprimir_vetor(FILE *saida, const int *vetor, size_t tamanho)
{
    for (size_t j = 0; j < tamanho; j++)
        fprintf(saida, "%s%d", j == 0 ? "" : " ", vetor[j]);
}

static size_t largura_vetor(const int *vetor, size_t tamanho)
{
    size_t largura = tamanho - 1;
    for (size_t j = 0; j < tamanho; j++)
        largura += (size_t) snprintf(NULL, 0, "%d", vetor[j]);
    return largura;
}

static void preencher_coluna(FILE *saida, size_t usado, size_t largura)
{
    while (usado < largura) {
        fputc(' ', saida);
        usado++;
    }
    fputs("|   ", saida);
}

static void imprimir_estado(FILE *saida, const Estado *estado)
{
    size_t largura = 0;
    for (size_t i = 0; i < estado->clientes; i++) {
        size_t atual = largura_vetor(estado->maximum + i * estado->recursos,
                                    estado->recursos);
        if (atual > largura)
            largura = atual;
    }
    size_t primeira = largura + 2 > 10 ? largura + 2 : 10;
    size_t segunda = largura + 2 > 12 ? largura + 2 : 12;
    fputs("MAXIMUM", saida);
    preencher_coluna(saida, 7, primeira);
    fputs("ALLOCATION", saida);
    preencher_coluna(saida, 10, segunda);
    fputs("NEED\n", saida);
    for (size_t i = 0; i < estado->clientes; i++) {
        size_t base = i * estado->recursos;
        imprimir_vetor(saida, estado->maximum + base, estado->recursos);
        preencher_coluna(saida, largura_vetor(estado->maximum + base, estado->recursos), primeira);
        imprimir_vetor(saida, estado->allocation + base, estado->recursos);
        preencher_coluna(saida, largura_vetor(estado->allocation + base, estado->recursos), segunda);
        imprimir_vetor(saida, estado->need + base, estado->recursos);
        fputs(" \n", saida);
    }
    fputs("AVAILABLE ", saida);
    imprimir_vetor(saida, estado->available, estado->recursos);
    fputs(" \n", saida);
}

/* A validacao de um comando inteiro antecede qualquer alteracao do estado. */
static int interpretar_comando(char *cursor, const Estado *estado,
                              char *operacao, size_t *cliente, int *valores)
{
    while (isspace((unsigned char) *cursor))
        cursor++;
    if (*cursor == '*') {
        *operacao = '*';
        cursor++;
        while (isspace((unsigned char) *cursor))
            cursor++;
        return *cursor == '\0';
    }
    char *fim_token = cursor;
    while (*fim_token && !isspace((unsigned char) *fim_token))
        fim_token++;
    if (fim_token - cursor != 2 || cursor[0] != 'R' ||
        (cursor[1] != 'Q' && cursor[1] != 'L'))
        return 0;
    *operacao = cursor[1];
    cursor = fim_token;
    while (isspace((unsigned char) *cursor))
        cursor++;
    if (*cursor == '-')
        return 0;
    errno = 0;
    char *fim;
    uintmax_t indice = strtoumax(cursor, &fim, 10);
    if (errno == ERANGE || fim == cursor || indice >= estado->clientes ||
        (*fim && !isspace((unsigned char) *fim)))
        return 0;
    *cliente = (size_t) indice;
    cursor = fim;
    for (size_t j = 0; j < estado->recursos; j++) {
        errno = 0;
        long valor = strtol(cursor, &fim, 10);
        if (errno == ERANGE || fim == cursor || valor < 0 || valor > INT_MAX ||
            (*fim && !isspace((unsigned char) *fim)))
            return 0;
        valores[j] = (int) valor;
        cursor = fim;
    }
    while (isspace((unsigned char) *cursor))
        cursor++;
    return *cursor == '\0';
}

static int processar_comandos(Estado *estado)
{
    FILE *entrada = fopen("commands.txt", "r");
    if (entrada == NULL) {
        perror("commands.txt");
        return 0;
    }
    FILE *saida = fopen("result.txt", "w");
    if (saida == NULL) {
        perror("result.txt");
        fclose(entrada);
        return 0;
    }
    int *valores = malloc(estado->recursos * sizeof *valores);
    char *linha = NULL;
    size_t capacidade = 0, numero = 0;
    ssize_t tamanho;
    int sucesso = 0;
    if (valores == NULL) {
        fprintf(stderr, "Erro ao reservar memoria para os comandos.\n");
        goto fim;
    }
    while ((tamanho = getline(&linha, &capacidade, entrada)) != -1) {
        numero++;
        char *cursor = linha;
        if (numero == 1 && tamanho >= 3 && memcmp(linha, "\xEF\xBB\xBF", 3) == 0)
            cursor += 3;
        char operacao;
        size_t cliente = 0;
        if (memchr(linha, '\0', (size_t) tamanho) != NULL ||
            !interpretar_comando(cursor, estado, &operacao, &cliente, valores)) {
            fprintf(stderr, "commands.txt: linha %zu invalida; use RQ/RL, cliente valido e %zu quantidades, ou *.\n",
                    numero, estado->recursos);
            goto fim;
        }
        if (operacao == '*') {
            imprimir_estado(saida, estado);
        } else if (operacao == 'L') {
            if (!liberar_recursos(estado, cliente, valores)) {
                fprintf(stderr, "commands.txt: linha %zu: liberacao excede a alocacao do cliente %zu.\n", numero, cliente);
                goto fim;
            }
            fprintf(saida, "Release from customer %zu the resources ", cliente);
            imprimir_vetor(saida, valores, estado->recursos);
            fputc('\n', saida);
        } else {
            ResultadoPedido resultado = solicitar_recursos(estado, cliente, valores);
            if (resultado == ERRO_MEMORIA) {
                fprintf(stderr, "Erro de memoria durante o teste de seguranca (linha %zu).\n", numero);
                goto fim;
            }
            if (resultado == PEDIDO_ACEITO) {
                fprintf(saida, "Allocate to customer %zu the resources ", cliente);
                imprimir_vetor(saida, valores, estado->recursos);
                fputc('\n', saida);
            } else if (resultado == RECURSOS_INSUFICIENTES) {
                fputs("The resources ", saida);
                imprimir_vetor(saida, estado->available, estado->recursos);
                fprintf(saida, " are not enough to customer %zu request ", cliente);
                imprimir_vetor(saida, valores, estado->recursos);
                fputs(" \n", saida);
            } else {
                fprintf(saida, "The customer %zu request ", cliente);
                imprimir_vetor(saida, valores, estado->recursos);
                fputs(resultado == EXCEDE_MAXIMO ?
                      " was denied because exceed its maximum allocation\n" :
                      " was denied because result in an unsafe state\n", saida);
            }
        }
        if (ferror(saida)) {
            fprintf(stderr, "Erro ao escrever result.txt.\n");
            goto fim;
        }
    }
    if (!feof(entrada)) {
        fprintf(stderr, "Erro ao ler commands.txt.\n");
        goto fim;
    }
    sucesso = 1;
fim:
    free(linha);
    free(valores);
    if (fclose(entrada) != 0) {
        perror("commands.txt");
        sucesso = 0;
    }
    if (fclose(saida) != 0) {
        perror("result.txt");
        sucesso = 0;
    }
    return sucesso;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Uso: %s recurso1 recurso2 ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    int quantidade_recursos = argc - 1;

    /* Reserva uma posição para cada tipo de recurso. */
    int *available = malloc(quantidade_recursos * sizeof *available);

    if (available == NULL) {
        fprintf(stderr, "Erro ao reservar memoria para os recursos.\n");
        return EXIT_FAILURE;
    }

    /* Converte os argumentos e preenche o vetor. */
    for (int i = 0; i < quantidade_recursos; i++) {
        char *fim;
        const char *argumento = argv[i + 1];

        errno = 0;
        long valor = strtol(argumento, &fim, 10);

        if (errno == ERANGE ||
            fim == argumento ||
            *fim != '\0' ||
            valor < 0 ||
            valor > INT_MAX) {
            fprintf(stderr, "Quantidade de recurso invalida: %s\n",
                    argumento);
            free(available);
            return EXIT_FAILURE;
        }

        available[i] = (int) valor;
    }

    size_t quantidade_clientes = 0;
    int *maximum = ler_clientes("customer.txt", quantidade_recursos, &quantidade_clientes);

    if (maximum == NULL) {
        free(available);
        return EXIT_FAILURE;
    }

    size_t total_elementos =
        quantidade_clientes * (size_t) quantidade_recursos;

    int *allocation = calloc(total_elementos, sizeof *allocation);
    int *need = malloc(total_elementos * sizeof *need);

    if (allocation == NULL || need == NULL) {
        fprintf(stderr, "Erro ao reservar memoria para as matrizes.\n");
        free(need);
        free(allocation);
        free(maximum);
        free(available);
        return EXIT_FAILURE;
    }

    /* Inicialmente, nenhum recurso foi alocado aos clientes. */
    for (size_t pos = 0; pos < total_elementos; pos++) {
        need[pos] = maximum[pos];
    }

    Estado estado = {quantidade_clientes, (size_t) quantidade_recursos,
                     available, maximum, allocation, need};
    int sucesso = processar_comandos(&estado);

    free(need);
    free(allocation);
    free(maximum);
    free(available);
    return sucesso ? EXIT_SUCCESS : EXIT_FAILURE;
}

