#include "comum.h"

// Tokenizador Manual (Sem strtok)
char* obter_token_manual(char *str, char delim) {
    static char *atual = NULL;
    if(str != NULL) atual = str;
    if(atual == NULL || *atual == '\0') return NULL;

    while(*atual == delim) atual++;
    if(*atual == '\0') return NULL;

    char *inicio = atual;
    while(*atual != '\0' && *atual != delim && *atual != '\n') {
        atual++;
    }
    
    if(*atual != '\0') {
        *atual = '\0';
        atual++;
    }
    return inicio;
}

// Envio não bloqueante para Named Pipe
void enviar_mensagem(char *fifo_dest, Pedido pedido) {
    int fd = open(fifo_dest, O_WRONLY | O_NONBLOCK);
    if(fd != -1) {
        if(write(fd, &pedido, sizeof(Pedido))){}
        close(fd);
    }
}