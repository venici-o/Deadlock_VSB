# Algoritmo do Banqueiro

Implementação em C do algoritmo do banqueiro para controlar solicitações e liberações de recursos, evitando estados inseguros. O código está em um único arquivo: `banker.c`.

## Compilar e executar

Requisitos: Linux, compilador C com suporte a C11 e `make`.

```sh
make
./banker 10 5 7
cat result.txt
```

Os argumentos representam as quantidades inicialmente disponíveis de cada recurso.

Antes de executar, coloque no mesmo diretório:

- `customer.txt`: uma linha por cliente, com suas demandas máximas separadas por vírgulas (exemplo: `7,5,3`).
- `commands.txt`: comandos a processar, um por linha.

## Comandos

- `RQ 0 0 2 1`: cliente 0 solicita os recursos `0 2 1`.
- `RL 0 0 1 1`: cliente 0 libera os recursos `0 1 1`.
- `*`: registra as matrizes `MAXIMUM`, `ALLOCATION`, `NEED` e o vetor `AVAILABLE`.

Os clientes são numerados a partir de zero. Cada comando de recursos deve informar uma quantidade por tipo de recurso.

As decisões e consultas são gravadas em `result.txt`, sobrescrito a cada execução. Solicitações que excedem a demanda máxima, a disponibilidade ou deixam o sistema inseguro são recusadas. Erros de entrada são informados na saída de erro.

Para remover o executável:

```sh
make clean
```

O arquivo `evidencias.log` contém os registros de desenvolvimento e testes.
