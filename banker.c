#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

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

    printf("MAXIMUM | ALLOCATION | NEED\n");

    for (size_t i = 0; i < quantidade_clientes; i++) {
        for (int j = 0; j < quantidade_recursos; j++) {
            printf("%d ", maximum[i * quantidade_recursos + j]);
        }

        printf("| ");

        for (int j = 0; j < quantidade_recursos; j++) {
            printf("%d ", allocation[i * quantidade_recursos + j]);
        }

        printf("| ");

        for (int j = 0; j < quantidade_recursos; j++) {
            printf("%d ", need[i * quantidade_recursos + j]);
        }

        printf("\n");
    }

    /* Mostra o estado inicial dos recursos disponíveis. */
    printf("AVAILABLE");
    for (int i = 0; i < quantidade_recursos; i++) {
        printf(" %d", available[i]);
    }
    printf("\n");

    free(need);
    free(allocation);
    free(maximum);
    free(available);
    return EXIT_SUCCESS;
}

