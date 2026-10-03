#ifndef COMUM_H
#define COMUM_H

#define _XOPEN_SOURCE 700 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h> 

#include "../constantes/constantes.h"

// Estrutura de Mensagem
typedef struct {
    int tipo;                
    pid_t pid_origem;        
    char str_dados[TAM_STR]; 
    int val1;                // Valor generico 1 (distancia / % / PID)
    int val2;                // Valor generico 2 (id_servico)
} Pedido;

// Funções
void enviar_mensagem(char *fifo_dest, Pedido pedido);
char* obter_token_manual(char *str, char delim);

#endif