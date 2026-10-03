#include "veiculos.h"

int main(int argc, char *argv[]) {
    if(argc<5) return 1;
    int arg1,arg2,arg3,arg4; 
    arg1=atoi(argv[1]),
    arg2=atoi(argv[2]), 
    arg3=atoi(argv[3]),
    arg4=atoi(argv[4]);
    veiculo_simular(arg1,arg2,arg3,arg4);
    
    return 0;
}