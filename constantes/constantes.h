#ifndef CONSTANTES_H
#define CONSTANTES_H

#define MAX_USERS 30
#define MAX_SERVICOS 50
#define MAX_VEICULOS 10

#define TAM_STR 100
#define TAM_MSG 256

#define FIFO_SRV "fifo_servidor"
#define FIFO_CLI_FMT "fifo_cli_%d"

#define LOGIN        1
#define AGENDAR      2
#define CONSULTAR    3
#define CANCELAR     4
#define TERMINAR_CLI 5

#define OP_OK           10
#define OP_ERRO         11
#define OP_INFO         12

#define V_STATUS_INICIO    20
#define V_STATUS_ANDAMENTO 21 // Envia %
#define V_STATUS_CONCLUIDO 22
#define V_STATUS_CANCELADO 23

#define ESTADO_AGENDADO  0
#define ESTADO_EM_CURSO  1
#define ESTADO_CONCLUIDO 2
#define ESTADO_CANCELADO 3

#endif