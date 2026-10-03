#include "cliente.h"
#include <signal.h>
#include <errno.h>

// Ponteiro global para permitir acesso dentro do handler de sinais
static Cliente *ref_cliente_global = NULL;


void trata_sinal_cliente(int sinal) {
    if (sinal == SIGINT && ref_cliente_global) {
        // write é async-signal-safe (printf não é)
        const char msg[] = "\n[CLIENTE] A sair...\n";
        write(STDOUT_FILENO, msg, sizeof(msg)-1);
        ref_cliente_global->ativo = 0; // Quebra o loop principal
    }
}

int cliente_iniciar(Cliente *c, char *nome) {
    ref_cliente_global = c;

    struct sigaction sa;
    sa.sa_handler = trata_sinal_cliente;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // 0 garante que chamadas de sistema (read/select) sejam interrompidas

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("[ERRO] Falha ao registar sigaction");
        return -1;
    }
  
    // Inicialização da Estrutura
    memset(c, 0, sizeof(Cliente));
    c->meu_pid = getpid();
    c->ativo = 1;
    strncpy(c->username, nome, TAM_STR - 1);
    
    // Verificação do Servidor
    if (access(FIFO_SRV, F_OK) != 0) {
        printf("[ERRO] Servidor offline ou nao encontrado.\n"); 
        return -1;
    }
    
    // Criação do FIFO do Cliente
    sprintf(c->meu_fifo, FIFO_CLI_FMT, c->meu_pid);
    if (mkfifo(c->meu_fifo, 0666) == -1 && errno != EEXIST) {
        perror("[ERRO] Falha ao criar FIFO de cliente"); 
        return -1;
    }
    
    c->fd_meu_fifo = open(c->meu_fifo, O_RDWR | O_NONBLOCK);
    if (c->fd_meu_fifo == -1) { 
        perror("[ERRO] Falha ao abrir FIFO de cliente");
        unlink(c->meu_fifo); 
        return -1; 
    }

    // --- Envio de Pedido de LOGIN ---
    Pedido p;
    memset(&p, 0, sizeof(Pedido));
    p.tipo = LOGIN; 
    p.pid_origem = c->meu_pid;
    strncpy(p.str_dados, nome, TAM_STR - 1);
    
    enviar_mensagem(FIFO_SRV, p);
    
    // --- Aguarda Resposta (Login) ---
    printf("A tentar entrar no sistema...\n");
    
    fd_set fds; 
    struct timeval tv;
    tv.tv_sec = 2; // Timeout de 2 segundos
    tv.tv_usec = 0;
    
    FD_ZERO(&fds); 
    FD_SET(c->fd_meu_fifo, &fds);
    
    int res = select(c->fd_meu_fifo + 1, &fds, NULL, NULL, &tv);
    
    if (res > 0) {
        read(c->fd_meu_fifo, &p, sizeof(Pedido));
        if (p.tipo == OP_OK) {
            printf("[LOGIN] Sucesso. Bem-vindo, %s.\n", nome); 
            return 0;
        } else {
            printf("[LOGIN] Recusado: %s\n", p.str_dados);
        }
    } else {
        printf("[ERRO] Sem resposta do servidor (Timeout).\n");
    }
    
    // Falha: Limpar e sair
    close(c->fd_meu_fifo); 
    unlink(c->meu_fifo);
    return -1;
}

void cliente_executar(Cliente *c) {
    fd_set fds; 
    char buffer_teclado[TAM_STR]; 
    Pedido p;
    
    printf("\n--- Painel Cliente %s ---\n", c->username);
    printf("Comandos: agendar <hora> <local> <dist> | consultar | cancelar <id> | sair\n");
    printf("CLIENTE> "); fflush(stdout); 

    while (c->ativo) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(c->fd_meu_fifo, &fds);
        
        // Calculo do MAX
        int max_fd = STDIN_FILENO;
        if (c->fd_meu_fifo > max_fd) {
            max_fd = c->fd_meu_fifo;
        }

        int res = select(max_fd + 1, &fds, NULL, NULL, NULL);
        
        // Se foi interrompido por sinal (Ctrl+C), continua
        if (res == -1 && errno == EINTR) continue;
        if (res < 0) break; 

        if (res > 0) {
            // --- 1. LER DO SERVIDOR ---
            if (FD_ISSET(c->fd_meu_fifo, &fds)) {
                if (read(c->fd_meu_fifo, &p, sizeof(Pedido)) > 0) {
                    // Limpar linha atual para efeito visual
                    printf("\r\033[K"); 

                    // --- INICIO DO TRATAMENTO DE MENSAGENS ---
                    if (p.tipo == OP_ERRO) {
                        // Verifica se é SHUTDOWN
                        if (strcmp(p.str_dados, "SHUTDOWN") == 0) {
                            printf("\n[CRITICO] O Servidor encerrou. A desligar cliente...\n"); 
                            c->ativo = 0; // Isto pára o loop while(c->ativo)
                        } else {
                            printf("[ERRO] %s\n", p.str_dados);
                        }
                    }
                    else if (p.tipo == OP_INFO) {
                        if (p.val1 > 0) printf("[STATUS] %s (%d%%)\n", p.str_dados, p.val1);
                        else printf("%s\n", p.str_dados);
                    }
                    else if (p.tipo == OP_OK) {
                        printf("[OK] %s %d\n", p.str_dados, p.val1);
                    }
                    // --- FIM DO TRATAMENTO ---
                    
                    // Repor o prompt apenas se ainda estiver ativo
                    if (c->ativo) { printf("CLIENTE> "); fflush(stdout); }
                }
            }

            // --- 2. LER DO TECLADO ---
            if (c->ativo && FD_ISSET(STDIN_FILENO, &fds)) {
                memset(buffer_teclado, 0, TAM_STR);
                
                if (read(STDIN_FILENO, buffer_teclado, TAM_STR - 1) > 0) {
                    
                    char *comando = obter_token_manual(buffer_teclado, ' ');
                    
                    if (comando) {
                        p.pid_origem = c->meu_pid; 
                        
                        if (strcmp(comando, "sair") == 0) {
                            c->ativo = 0;
                        }
                        else if (strcmp(comando, "consultar") == 0) {
                            p.tipo = CONSULTAR; 
                            enviar_mensagem(FIFO_SRV, p);
                        }
                        else if (strcmp(comando, "cancelar") == 0) {
                            char *id_str = obter_token_manual(NULL, ' ');
                            if (id_str) { 
                                p.tipo = CANCELAR; 
                                p.val1 = atoi(id_str); 
                                enviar_mensagem(FIFO_SRV, p); 
                            } else {
                                printf("Erro: Falta indicar o ID.\n");
                            }
                        }
                        else if (strcmp(comando, "agendar") == 0) {
                            char *hora_str = obter_token_manual(NULL, ' ');
                            char *local_str = obter_token_manual(NULL, ' ');
                            char *dist_str = obter_token_manual(NULL, ' ');
                            
                            if (hora_str && local_str && dist_str) {
                                p.tipo = AGENDAR; 
                                p.val1 = atoi(hora_str); 
                                p.val2 = atoi(dist_str);
                                strncpy(p.str_dados, local_str, TAM_STR - 1);
                                enviar_mensagem(FIFO_SRV, p);
                            } else {
                                printf("Erro: use agendar <hora> <local> <distancia>\n");
                            }
                        }
                        else {
                            printf("Comando desconhecido.\n");
                        }
                    }
                    // Repor prompt após comando
                    if (c->ativo) { printf("> "); fflush(stdout); }
                }
            }
        }
    }
}

void cliente_limpar(Cliente *c) {
    if (c->fd_meu_fifo != -1) {
        
        Pedido p; 
        memset(&p, 0, sizeof(Pedido));
        p.tipo = TERMINAR_CLI; 
        p.pid_origem = c->meu_pid;
        
        // Só tenta enviar se o servidor parecer estar vivo
        if (access(FIFO_SRV, F_OK) == 0) {
            enviar_mensagem(FIFO_SRV, p);
        }
        
        close(c->fd_meu_fifo);
    }
    
    unlink(c->meu_fifo);
    printf("Cliente encerrado com sucesso.\n");
}