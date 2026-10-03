#include "cliente.h"

int main(int argc, char *argv[]) {
    if(argc<2) { printf("./app_cliente <user>\n"); return 1; }
    
    Cliente c;
    if(cliente_iniciar(&c, argv[1])==0) {
        cliente_executar(&c);
        cliente_limpar(&c);
    } else cliente_limpar(&c);
    
    return 0;
}