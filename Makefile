CC = gcc

# Target por defeito
all: controlador cliente veiculo

# Controlador
controlador: app_controlador

app_controlador: main_controlador.o controlador.o comum.o
	$(CC) main_controlador.o controlador.o comum.o -o app_controlador -pthread

# Cliente
cliente: app_cliente

app_cliente: main_cliente.o cliente.o comum.o
	$(CC) main_cliente.o cliente.o comum.o -o app_cliente

# Veículo
veiculo: app_veiculo

app_veiculo: veiculos_main.o veiculos.o comum.o
	$(CC) veiculos_main.o veiculos.o comum.o -o app_veiculo -pthread

# Compilação dos objetos
main_controlador.o: controlador/main_controlador.c
	$(CC) -c controlador/main_controlador.c -pthread

controlador.o: controlador/controlador.c
	$(CC) -c controlador/controlador.c -pthread

main_cliente.o: cliente/main_cliente.c
	$(CC) -c cliente/main_cliente.c

cliente.o: cliente/cliente.c
	$(CC) -c cliente/cliente.c

veiculos_main.o: veiculo/veiculos_main.c
	$(CC) -c veiculo/veiculos_main.c -pthread

veiculos.o: veiculo/veiculos.c
	$(CC) -c veiculo/veiculos.c -pthread

comum.o: comum/comum.c
	$(CC) -c comum/comum.c

# Limpeza
clean:
	rm -f app_controlador app_cliente app_veiculo
	rm -f *.o
	rm -f FIFO_* controlador.lock

.PHONY: all controlador cliente veiculo clean
