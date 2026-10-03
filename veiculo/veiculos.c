#include "veiculos.h"
//validar
static volatile sig_atomic_t veiculo_cancelado = 0;

static void trata_sigusr1(int s) {
    (void)s;
    veiculo_cancelado = 1;
}
void veiculo_simular(int id, int dist, pid_t pid, int fd) {
    (void)pid; // pid nao usado na simulacao basica mas passado por norma
    Pedido p; int km=0;
    struct sigaction sa;
    sa.sa_handler = trata_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    // Inicio
    memset(&p, 0, sizeof(Pedido));
    p.tipo = V_STATUS_INICIO; p.val2 = id;
    write(fd, &p, sizeof(Pedido));
    sleep(1);

    // Andamento
    while(km < dist) {
        if (veiculo_cancelado) {
           memset(&p, 0, sizeof(Pedido));
            p.tipo = V_STATUS_CANCELADO;
            p.val1 = (dist > 0) ? (km * 100) / dist : 0;
            p.val2 = id;
            write(fd, &p, sizeof(Pedido));
            close(fd);
            return;
        }
        sleep(1);
        km++;
        p.tipo = V_STATUS_ANDAMENTO;
        p.val1 = (km*100)/dist; 
        p.val2 = id;
        write(fd, &p, sizeof(Pedido));
    }
    
    // Fim
    sleep(1);
    p.tipo = V_STATUS_CONCLUIDO; p.val1 = 100; p.val2 = id;
    write(fd, &p, sizeof(Pedido));
    close(fd);
}