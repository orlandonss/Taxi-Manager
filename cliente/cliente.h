#ifndef CLIENTE_H
#define CLIENTE_H

#include "../comum/comum.h"

typedef struct {
    pid_t meu_pid;
    char username[TAM_STR];
    char meu_fifo[TAM_STR];
    int fd_meu_fifo;
    int ativo;
} Cliente;

int cliente_iniciar(Cliente *cliente, char *nome);
void cliente_executar(Cliente *cliente);
void cliente_limpar(Cliente *cliente);

#endif