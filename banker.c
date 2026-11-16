#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

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

    /* Mostra o estado inicial dos recursos disponíveis. */
    printf("AVAILABLE");
    for (int i = 0; i < quantidade_recursos; i++) {
        printf(" %d", available[i]);
    }
    printf("\n");

    free(available);
    return EXIT_SUCCESS;
}