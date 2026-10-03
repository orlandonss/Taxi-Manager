#include "controlador.h"
#include "../comum/comum.h" 

//para mostrar de forma correta os valores
const char* estado_str(int estado) {
    switch (estado) {
        case ESTADO_AGENDADO:  return "AGENDADO";
        case ESTADO_EM_CURSO:  return "EM CURSO";
        case ESTADO_CONCLUIDO: return "CONCLUIDO";
        case ESTADO_CANCELADO: return "CANCELADO";
        default:               return "DESCONHECIDO";
    }
}


// Única variável global
EstadoControlador ctrl_estado;

void preencher_pedido(Pedido *p, int tipo, int valor1, int valor2, char *texto) {
    memset(p, 0, sizeof(Pedido));
    p->tipo = tipo;
    p->val1 = valor1;
    p->val2 = valor2;
    if (texto) {
        strncpy(p->str_dados, texto, TAM_STR - 1);
    }
}

// Helper para configurar slot da frota
void configurar_veiculo(int indice, pid_t pid, int descritor_leitura, int id_servico) {
    ctrl_estado.frota[indice].ativo = 1;
    ctrl_estado.frota[indice].index = indice;
    ctrl_estado.frota[indice].pid_processo = pid;
    ctrl_estado.frota[indice].pipe_leitura = descritor_leitura;
    ctrl_estado.frota[indice].perc_atual = 0;
    ctrl_estado.frota[indice].kms_viagem_atual = 0;
    ctrl_estado.frota[indice].id_servico = id_servico;
}

// Função Principal de Envio (Abstrai o preenchimento)
void enviar_msg_cliente(pid_t pid, int tipo, char *texto, int valor) {
    char caminho_fifo[50];
    sprintf(caminho_fifo, FIFO_CLI_FMT, pid);
    
    Pedido p;
    preencher_pedido(&p, tipo, valor, 0, texto);
    
    enviar_mensagem(caminho_fifo, p);
}

void trata_sinal_servidor(int sinal) {
    if (sinal == SIGINT) {
        ctrl_estado.sistema_ativo = 0;
        const char msg[] = "\n[SISTEMA] Sinal recebido. A encerrar...\n";
        write(STDOUT_FILENO, msg, sizeof(msg)-1);
    }
}


void servidor_login(Pedido p) {
    int posicao_livre = -1;
    int nome_existe = 0;

    for (int i = 0; i < MAX_USERS; i++) {
        if (ctrl_estado.utilizadores[i].ocupado) {
            if (strcmp(ctrl_estado.utilizadores[i].nome, p.str_dados) == 0) nome_existe = 1;
        } else {
            if (posicao_livre == -1) posicao_livre = i;
        }
    }

    if (nome_existe) {
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);
        enviar_msg_cliente(p.pid_origem, OP_ERRO, "Nome ja existe!", 0);
        return;
    }

    if (posicao_livre != -1) {
        ctrl_estado.utilizadores[posicao_livre].ocupado = 1;
        ctrl_estado.utilizadores[posicao_livre].pid = p.pid_origem;
        strncpy(ctrl_estado.utilizadores[posicao_livre].nome, p.str_dados, TAM_STR - 1);
        
        pthread_mutex_unlock(&ctrl_estado.mutex_dados); 
        enviar_msg_cliente(p.pid_origem, OP_OK, "Login com Sucesso", 0);
    } else {
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);
        enviar_msg_cliente(p.pid_origem, OP_ERRO, "Servidor Cheio", 0);
    }
}

void servidor_agendar(Pedido p) {
    int utilizador_logado = 0;
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (ctrl_estado.utilizadores[i].ocupado && ctrl_estado.utilizadores[i].pid == p.pid_origem) {
            utilizador_logado = 1; break;
        }
    }

    if (!utilizador_logado) { 
        pthread_mutex_unlock(&ctrl_estado.mutex_dados); 
        return; 
    }


    // p.val1 contém a hora agendada. Se for menor ou igual ao tempo atual, erro.
    if (p.val1 <= ctrl_estado.tempo_simulado) {
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);
        char erro_msg[60];
        sprintf(erro_msg, "Hora invalida! Atual: %d", ctrl_estado.tempo_simulado);
        enviar_msg_cliente(p.pid_origem, OP_ERRO, erro_msg, 0);
        return;
    }


    /*
     Verifica se já está numa viagem (Em Curso)
    for (int i = 0; i < MAX_SERVICOS; i++) {
        if (ctrl_estado.servicos[i].id != 0 && 
            ctrl_estado.servicos[i].pid_cliente == p.pid_origem &&
            ctrl_estado.servicos[i].estado == ESTADO_EM_CURSO) {
            
            // Se encontrou uma viagem em curso, rejeita
            pthread_mutex_unlock(&ctrl_estado.mutex_dados);
            enviar_msg_cliente(p.pid_origem, OP_ERRO, "Ja esta numa viagem! Termine a atual.", 0);
            return;
        }
    }*/

    int slot = -1;
    for (int i = 0; i < MAX_SERVICOS; i++) {
        if (ctrl_estado.servicos[i].id == 0) { slot = i; break; }
    }

    if (slot != -1) {
        ctrl_estado.servicos[slot].id = ++ctrl_estado.total_servicos;
        ctrl_estado.servicos[slot].pid_cliente = p.pid_origem;
        ctrl_estado.servicos[slot].distancia = p.val2;
        ctrl_estado.servicos[slot].hora = p.val1;
        ctrl_estado.servicos[slot].estado = ESTADO_AGENDADO;
        strncpy(ctrl_estado.servicos[slot].local, p.str_dados, TAM_STR - 1);
        
       // verificar_agendamentos_seguro(); validar se é memso necessário, está a dar erro bro...
        
        int id_criado = ctrl_estado.servicos[slot].id;
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);
        enviar_msg_cliente(p.pid_origem, OP_OK, "Agendado ID", id_criado);
    } else {
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);
        enviar_msg_cliente(p.pid_origem, OP_ERRO, "Lista de Servicos Cheia", 0);
    }
}

void servidor_consultar(Pedido p) {
    char buffer[TAM_MSG] = "Os Meus Servicos:\n";
    int contador = 0;
    
    for (int i = 0; i < MAX_SERVICOS; i++) {
        if (ctrl_estado.servicos[i].id != 0 && ctrl_estado.servicos[i].pid_cliente == p.pid_origem) {
            char linha[60];
        sprintf(linha, "- ID %d (%d km) Estado: %s\n",ctrl_estado.servicos[i].id,ctrl_estado.servicos[i].distancia,estado_str(ctrl_estado.servicos[i].estado));
            strcat(buffer, linha); contador++;
        }
    }
    
    if (contador == 0) strcpy(buffer, "Nenhum servico encontrado.");
    
    pthread_mutex_unlock(&ctrl_estado.mutex_dados);
    enviar_msg_cliente(p.pid_origem, OP_INFO, buffer, 0);
}

void servidor_cancelar(Pedido p) {
    int cancelados = 0;

    if (p.val1 == 0) {
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (ctrl_estado.servicos[i].id != 0 &&
                ctrl_estado.servicos[i].pid_cliente == p.pid_origem &&
                ctrl_estado.servicos[i].estado == ESTADO_AGENDADO) {

                ctrl_estado.servicos[i].estado = ESTADO_CANCELADO;
                cancelados++;
            }
        }

        pthread_mutex_unlock(&ctrl_estado.mutex_dados);

        if (cancelados > 0)
            enviar_msg_cliente(p.pid_origem, OP_OK,
                               "Servicos agendados cancelados:", cancelados);
        else
            enviar_msg_cliente(p.pid_origem, OP_INFO,
                               "Nao existem servicos agendados para cancelar.", 0);

        return;
    }

    for (int i = 0; i < MAX_SERVICOS; i++) {
        if (ctrl_estado.servicos[i].id == p.val1 &&
            ctrl_estado.servicos[i].pid_cliente == p.pid_origem) {

            if (ctrl_estado.servicos[i].estado == ESTADO_AGENDADO) {
                ctrl_estado.servicos[i].estado = ESTADO_CANCELADO;

                pthread_mutex_unlock(&ctrl_estado.mutex_dados);
                enviar_msg_cliente(p.pid_origem, OP_OK,
                                   "Servico cancelado:", p.val1);
                return;
            }

            if (ctrl_estado.servicos[i].estado == ESTADO_EM_CURSO) {
                pthread_mutex_unlock(&ctrl_estado.mutex_dados);
                enviar_msg_cliente(p.pid_origem, OP_ERRO,
                                   "Servico em execucao. Apenas o administrador pode cancelar.", 0);
                return;
            }
            pthread_mutex_unlock(&ctrl_estado.mutex_dados);
            enviar_msg_cliente(p.pid_origem, OP_INFO,
                               "Servico ja terminado ou cancelado.", 0);
            return;
        }
    }
    pthread_mutex_unlock(&ctrl_estado.mutex_dados);
    enviar_msg_cliente(p.pid_origem, OP_ERRO,
                       "Servico nao encontrado.", 0);
}


void processar_logica_comando(Pedido p) {
    pthread_mutex_lock(&ctrl_estado.mutex_dados);
    switch (p.tipo) {
        case LOGIN:        servidor_login(p); break;
        case AGENDAR:      servidor_agendar(p); break;
        case CONSULTAR:    servidor_consultar(p); break;
        case CANCELAR:     servidor_cancelar(p); break;
        case TERMINAR_CLI: 
            for (int i = 0; i < MAX_USERS; i++) {
                if (ctrl_estado.utilizadores[i].pid == p.pid_origem) ctrl_estado.utilizadores[i].ocupado = 0;
            }
            pthread_mutex_unlock(&ctrl_estado.mutex_dados);
            break;
        default: 
            pthread_mutex_unlock(&ctrl_estado.mutex_dados); 
            break;
    }
}

void* tarefa_monitor(void* arg) {
    VeiculoInfo *v = (VeiculoInfo*)arg;
    int v_idx = v->index;
    Pedido pv; 

    pthread_mutex_lock(&ctrl_estado.mutex_dados);
    int descritor = -1;
    if (ctrl_estado.frota[v_idx].ativo) descritor = ctrl_estado.frota[v_idx].pipe_leitura;
    pthread_mutex_unlock(&ctrl_estado.mutex_dados);

    if (descritor == -1) return NULL;

    while (ctrl_estado.sistema_ativo) {
        if (read(descritor, &pv, sizeof(Pedido)) <= 0) break;

        pthread_mutex_lock(&ctrl_estado.mutex_dados);
        int s_idx = -1;
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (ctrl_estado.servicos[i].id == pv.val2) { s_idx = i; break; }
        }

        if (s_idx != -1) {
            if (pv.tipo == V_STATUS_INICIO) {
                ctrl_estado.frota[v_idx].perc_atual = 0;
                ctrl_estado.frota[v_idx].kms_viagem_atual = ctrl_estado.servicos[s_idx].distancia;
                
                pthread_mutex_unlock(&ctrl_estado.mutex_dados);
                enviar_msg_cliente(ctrl_estado.servicos[s_idx].pid_cliente, OP_INFO, "Condutor a caminho.", 0);
                pthread_mutex_lock(&ctrl_estado.mutex_dados);
            }
            else if (pv.tipo == V_STATUS_ANDAMENTO) {
                ctrl_estado.frota[v_idx].perc_atual = pv.val1;
            }
            else if (pv.tipo == V_STATUS_CONCLUIDO) {
                ctrl_estado.kms_total_sistema += ctrl_estado.frota[v_idx].kms_viagem_atual;
                ctrl_estado.servicos[s_idx].estado = ESTADO_CONCLUIDO;
                ctrl_estado.frota[v_idx].kms_totais += ctrl_estado.frota[v_idx].kms_viagem_atual;
                ctrl_estado.frota[v_idx].perc_atual = 100;
                
                pid_t pid_wait = ctrl_estado.frota[v_idx].pid_processo;
                int pipe_close = ctrl_estado.frota[v_idx].pipe_leitura;
                
                pthread_mutex_unlock(&ctrl_estado.mutex_dados);
                waitpid(pid_wait, NULL, 0); 
                close(pipe_close);
                pthread_mutex_lock(&ctrl_estado.mutex_dados);
                
                ctrl_estado.frota[v_idx].ativo = 0;
                
                pthread_mutex_unlock(&ctrl_estado.mutex_dados);
                enviar_msg_cliente(ctrl_estado.servicos[s_idx].pid_cliente, OP_INFO, "Viagem concluida.", 100);
                pthread_exit(NULL);
            }
            else if (pv.tipo == V_STATUS_CANCELADO) {
            ctrl_estado.servicos[s_idx].estado = ESTADO_CANCELADO;
            ctrl_estado.frota[v_idx].perc_atual = pv.val1;
            int kms_feitos = (ctrl_estado.frota[v_idx].kms_viagem_atual * pv.val1) / 100;
            ctrl_estado.frota[v_idx].kms_totais += kms_feitos;
            ctrl_estado.kms_total_sistema += kms_feitos;

            pid_t pid_wait = ctrl_estado.frota[v_idx].pid_processo;
            int pipe_close = ctrl_estado.frota[v_idx].pipe_leitura;

            pthread_mutex_unlock(&ctrl_estado.mutex_dados);
            waitpid(pid_wait, NULL, 0);
            close(pipe_close);
            pthread_mutex_lock(&ctrl_estado.mutex_dados);

            ctrl_estado.frota[v_idx].ativo = 0;

            pthread_mutex_unlock(&ctrl_estado.mutex_dados);
            enviar_msg_cliente(ctrl_estado.servicos[s_idx].pid_cliente, OP_ERRO, "Servico cancelado pelo controlador.", pv.val1);
            pthread_exit(NULL);
            
            }
        }
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);
    }
    
    pthread_mutex_lock(&ctrl_estado.mutex_dados);
    if (ctrl_estado.frota[v_idx].ativo) { 
        waitpid(ctrl_estado.frota[v_idx].pid_processo, NULL, 0);
        close(ctrl_estado.frota[v_idx].pipe_leitura);
        ctrl_estado.frota[v_idx].ativo = 0;
    }
    pthread_mutex_unlock(&ctrl_estado.mutex_dados);
    return NULL;
}

int lancar_veiculo(int id, int dist, pid_t pid) {
    int indice = -1;
    for (int i = 0; i < MAX_VEICULOS; i++) {
        if (!ctrl_estado.frota[i].ativo) { indice = i; break; }
    }
    if (indice == -1) return -1;

    int tubagem[2];
    if (pipe(tubagem) == -1) return -1;

    pid_t pid_filho = fork();
    if (pid_filho == 0) {
        close(tubagem[0]); 
        char args[4][12];
        sprintf(args[0], "%d", id); 
        sprintf(args[1], "%d", dist); 
        sprintf(args[2], "%d", pid); 
        sprintf(args[3], "%d", tubagem[1]);
        execl("./app_veiculo", "app_veiculo", args[0], args[1], args[2], args[3], NULL);
        exit(1);
    } else {
        close(tubagem[1]);
        configurar_veiculo(indice, pid_filho, tubagem[0], id);        
        for (int k = 0; k < MAX_SERVICOS; k++) 
            if (ctrl_estado.servicos[k].id == id) ctrl_estado.servicos[k].estado = ESTADO_EM_CURSO;
        
        pthread_create(&ctrl_estado.frota[indice].t_monitor, NULL, tarefa_monitor, &ctrl_estado.frota[indice]);
        pthread_detach(ctrl_estado.frota[indice].t_monitor);
        return 0;
    }
}

void verificar_agendamentos_seguro() {
    for (int i = 0; i < MAX_SERVICOS; i++) {
        if (ctrl_estado.servicos[i].id && ctrl_estado.servicos[i].estado == ESTADO_AGENDADO) {
            lancar_veiculo(ctrl_estado.servicos[i].id, ctrl_estado.servicos[i].distancia, ctrl_estado.servicos[i].pid_cliente);
        }
    }
}


void* tarefa_clientes(void* arg) {
    (void)arg;
    Pedido requisicao;
    while (ctrl_estado.sistema_ativo) {
        if (read(ctrl_estado.descritor_fifo, &requisicao, sizeof(Pedido)) > 0) {
            processar_logica_comando(requisicao);
        } else {
            close(ctrl_estado.descritor_fifo);
            ctrl_estado.descritor_fifo = open(FIFO_SRV, O_RDONLY);
        }
    }
    return NULL;
}

void* tarefa_interface_admin(void* arg) {
    (void)arg;
    char buffer[100];
    printf("Comandos Admin: terminar, km, cancelar, hora, utiliz, listar, frota\n");
    printf("SERVIDOR> "); fflush(stdout);

    while (ctrl_estado.sistema_ativo) {
        if (fgets(buffer, 99, stdin) == NULL) break;
        
        buffer[strcspn(buffer, "\n")] = 0;
        buffer[strcspn(buffer, "\r")] = 0; 

        if (strlen(buffer) == 0) {
             if (ctrl_estado.sistema_ativo) { printf("SERVIDOR> "); fflush(stdout); }
             continue;
        }

        pthread_mutex_lock(&ctrl_estado.mutex_dados);
        
        int comando_valido = 0;
        if (strcmp(buffer, "terminar") == 0) {
            ctrl_estado.sistema_ativo = 0;
            comando_valido = 1;
        } 
        else if (strcmp(buffer, "hora") == 0) {
            printf("[ADMIN] Hora: %d\n", ctrl_estado.tempo_simulado);
            comando_valido = 1;
        } 
        else if (strcmp(buffer, "utiliz") == 0) {
            printf("[ADMIN] Utilizadores Online:\n");
            int count = 0;
            for (int i = 0; i < MAX_USERS; i++) {
                if (ctrl_estado.utilizadores[i].ocupado) {
                    printf(" - Nome: %s | PID: %d\n", ctrl_estado.utilizadores[i].nome, ctrl_estado.utilizadores[i].pid);
                    count++;
                }
            }
            if (count == 0) printf("   (Nenhum utilizador logado)\n");
            comando_valido = 1;
        }
        else if (strcmp(buffer, "listar") == 0) {
            printf("[ADMIN] Lista de Servicos:\n");
            for (int i = 0; i < MAX_SERVICOS; i++) {
                if (ctrl_estado.servicos[i].id) 
                printf(" - ID: %d | User: %d | Hora: %d | Dist: %d | Estado: %s\n",
                    ctrl_estado.servicos[i].id,
                    ctrl_estado.servicos[i].pid_cliente,
                    ctrl_estado.servicos[i].hora,
                    ctrl_estado.servicos[i].distancia,
                    estado_str(ctrl_estado.servicos[i].estado));
                }
            comando_valido = 1;
        } 
        else if (strcmp(buffer, "frota") == 0) {
            printf("[ADMIN] Estado da Frota:\n");
            for (int i = 0; i < MAX_VEICULOS; i++) {
                if (ctrl_estado.frota[i].ativo) 
                    printf(" - Slot %d: %d%%\n", i, ctrl_estado.frota[i].perc_atual);
            }
            comando_valido = 1;
        }
        else if (strcmp(buffer, "km") == 0) {
            printf("[ADMIN] Total de quilometros percorridos: %d km\n",
                ctrl_estado.kms_total_sistema);
            comando_valido = 1;
        }
        else if (strncmp(buffer, "cancelar ", 9) == 0) {
            int id = atoi(buffer + 9);
            comando_valido = 1;
            
        if (id == 0) {
        // cancelar todos
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (ctrl_estado.servicos[i].id == 0) continue;

            int sid = ctrl_estado.servicos[i].id;
            int estado = ctrl_estado.servicos[i].estado;

            if (estado == ESTADO_AGENDADO) {
                ctrl_estado.servicos[i].estado = ESTADO_CANCELADO;
            } else if (estado == ESTADO_EM_CURSO) {
                // encontrar veículo e enviar SIGUSR1
                for (int v = 0; v < MAX_VEICULOS; v++) {
                    if (ctrl_estado.frota[v].ativo && ctrl_estado.frota[v].id_servico == sid) {
                        kill(ctrl_estado.frota[v].pid_processo, SIGUSR1);
                        break;
                    }
                }
            }
        }
        printf("[ADMIN] Cancelamento global pedido.\n");
    } else {
        // cancelar um id específico
        int encontrado = 0;
        for (int i = 0; i < MAX_SERVICOS; i++) {
            if (ctrl_estado.servicos[i].id != id) continue;
            encontrado = 1;

            if (ctrl_estado.servicos[i].estado == ESTADO_AGENDADO) {
                ctrl_estado.servicos[i].estado = ESTADO_CANCELADO;
                printf("[ADMIN] Servico %d cancelado (agendado).\n", id);
            } else if (ctrl_estado.servicos[i].estado == ESTADO_EM_CURSO) {
                for (int v = 0; v < MAX_VEICULOS; v++) {
                    if (ctrl_estado.frota[v].ativo && ctrl_estado.frota[v].id_servico == id) {
                        kill(ctrl_estado.frota[v].pid_processo, SIGUSR1);
                        printf("[ADMIN] SIGUSR1 enviado ao veiculo do servico %d.\n", id);
                        break;
                    }
                }
            } else {
                printf("[ADMIN] Servico %d ja terminou/cancelado.\n", id);
            }
            break;
        }
            if (!encontrado) printf("[ADMIN] Servico %d nao existe.\n", id);
         }
        }

        
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);
        
        if (!comando_valido && ctrl_estado.sistema_ativo) printf("[SERVIDOR] Comando desconhecido: '%s'\n", buffer);
        if (ctrl_estado.sistema_ativo) { printf("SERVIDOR> "); fflush(stdout); }
    }
    return NULL;
}
/*
void* tarefa_tempo(void* arg) {
    (void)arg;
    while (ctrl_estado.sistema_ativo) {
        sleep(1);
        pthread_mutex_lock(&ctrl_estado.mutex_dados);
        ctrl_estado.tempo_simulado++;
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);
    }
    return NULL;
}
*/

void* tarefa_tempo(void* arg) {
    (void)arg;
    while (ctrl_estado.sistema_ativo) {
        sleep(1);

        // 1) avançar tempo
        pthread_mutex_lock(&ctrl_estado.mutex_dados);
        ctrl_estado.tempo_simulado++;
        int agora = ctrl_estado.tempo_simulado;
        pthread_mutex_unlock(&ctrl_estado.mutex_dados);

        // 2) lançar serviços cuja hora chegou
        for (int i = 0; i < MAX_SERVICOS; i++) {

            pthread_mutex_lock(&ctrl_estado.mutex_dados);

            int pode_lancar =
                (ctrl_estado.servicos[i].id != 0) &&
                (ctrl_estado.servicos[i].estado == ESTADO_AGENDADO) &&
                (ctrl_estado.servicos[i].hora == agora);

            int id = 0, dist = 0;
            pid_t pid_cli = 0;

            if (pode_lancar) {
                id = ctrl_estado.servicos[i].id;
                dist = ctrl_estado.servicos[i].distancia;
                pid_cli = ctrl_estado.servicos[i].pid_cliente;

                // marca já como "em curso" para não tentar lançar duas vezes
                ctrl_estado.servicos[i].estado = ESTADO_EM_CURSO;
            }

            pthread_mutex_unlock(&ctrl_estado.mutex_dados);

            if (pode_lancar) {
                // tenta lançar fora do mutex (fork/pipe/etc)
                if (lancar_veiculo(id, dist, pid_cli) != 0) {
                    // se falhar (ex: frota cheia), volta a agendado
                    pthread_mutex_lock(&ctrl_estado.mutex_dados);
                    for (int k = 0; k < MAX_SERVICOS; k++) {
                        if (ctrl_estado.servicos[k].id == id) {
                            ctrl_estado.servicos[k].estado = ESTADO_AGENDADO;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&ctrl_estado.mutex_dados);
                }
            }
        }
    }
    return NULL;
}

void controlador_iniciar() {
    ctrl_estado.kms_total_sistema = 0;
    int resultado_mutex = pthread_mutex_init(&ctrl_estado.mutex_dados, NULL);
    if (resultado_mutex != 0) { fprintf(stderr, "[ERRO] Falha Mutex.\n"); exit(1); }

    ctrl_estado.sistema_ativo = 1;
    ctrl_estado.total_servicos = 0;
    ctrl_estado.tempo_simulado = 0;

    if (access(FIFO_SRV, F_OK) == 0) { fprintf(stderr, "[ERRO] Controlador ja em execucao.\n"); exit(1); }
    if (mkfifo(FIFO_SRV, 0666) == -1) { perror("[ERRO] Falha mkfifo"); exit(1); }
    
    ctrl_estado.descritor_fifo = open(FIFO_SRV, O_RDWR);
    if (ctrl_estado.descritor_fifo == -1) { perror("[ERRO] Falha open FIFO"); unlink(FIFO_SRV); exit(1); }

    struct sigaction sa;
    sa.sa_handler = trata_sinal_servidor;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    printf("--- Servidor LIGADO (PID %d) ---\n", getpid());
}

void controlador_executar() {
    pthread_create(&ctrl_estado.tarefa_clientes, NULL, tarefa_clientes, NULL);
    pthread_create(&ctrl_estado.tarefa_tempo, NULL, tarefa_tempo, NULL);
    pthread_create(&ctrl_estado.tarefa_admin, NULL, tarefa_interface_admin, NULL);

    while (ctrl_estado.sistema_ativo) sleep(1);
}

void controlador_limpar() {
    printf("\nA Encerrar...\n");
    pthread_mutex_lock(&ctrl_estado.mutex_dados);
    ctrl_estado.sistema_ativo = 0;

    for (int i = 0; i < MAX_USERS; i++) 
        if (ctrl_estado.utilizadores[i].ocupado) enviar_msg_cliente(ctrl_estado.utilizadores[i].pid, OP_ERRO, "SHUTDOWN", 0);
    
    sleep(1); // 0.1s para envio das msgs
    
    for (int i = 0; i < MAX_VEICULOS; i++) 
        if (ctrl_estado.frota[i].ativo) kill(ctrl_estado.frota[i].pid_processo, SIGKILL);
    
    pthread_cancel(ctrl_estado.tarefa_clientes);
    pthread_cancel(ctrl_estado.tarefa_tempo);
    pthread_cancel(ctrl_estado.tarefa_admin);

    close(ctrl_estado.descritor_fifo);
    unlink(FIFO_SRV); 
    
    pthread_mutex_unlock(&ctrl_estado.mutex_dados);
    pthread_mutex_destroy(&ctrl_estado.mutex_dados);
    printf("Fim.\n");
}