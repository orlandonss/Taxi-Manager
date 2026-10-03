#ifndef CONTROLADOR_H
#define CONTROLADOR_H

#include "../comum/comum.h"

// Dados Internos
typedef struct { 
    char nome[TAM_STR]; 
    pid_t pid; 
    int ocupado; 
} UserInfo;

typedef struct { 
    int index; 
    pid_t pid_processo; 
    int pipe_leitura; 
    int ativo; 
    pthread_t t_monitor;
    int perc_atual; 
    int kms_totais; 
    int kms_viagem_atual;
    int id_servico;
} VeiculoInfo;

typedef struct {
    int id;
    int estado;
    int distancia;
    pid_t pid_cliente;
    char local[TAM_STR];
    int hora;
} ServicoInfo;


typedef struct {
    UserInfo utilizadores[MAX_USERS];
    ServicoInfo servicos[MAX_SERVICOS];
    VeiculoInfo frota[MAX_VEICULOS];
    volatile int sistema_ativo;
    int descritor_fifo;
    int total_servicos;
    int tempo_simulado;
    int kms_total_sistema;

    pthread_mutex_t mutex_dados;
    pthread_t tarefa_clientes, tarefa_tempo, tarefa_admin;
} EstadoControlador;


void controlador_iniciar();
void controlador_executar();
void controlador_limpar();
int lancar_veiculo(int id, int dist, pid_t pid);
void enviar_msg_cli(pid_t pid, int tipo, char *txt, int val);
void verificar_agendamentos_seguro();
const char* estado_str(int estado);
#endif
