/*
 * clienteTest.c
 *
 *  Created on: Jun 21, 2014
 *      Author: root
 */
#include <conitos-estaCoverflow/common_sockets.h>
#include <stdlib.h>
#include <commons/error.h>
#include <stdio.h>
#include <conitos-estaCoverflow/conitos_generic.h>
#include <unistd.h>
#include <commons/string.h>

int socket;

void terminar() {
	error_show("\nOcurrio un error, terminando el programa\n");
	close(socket);
	exit(0);
}

int main() {
	socket = connect_to("localhost","5000");
	if (socket == -1) {
		return EXIT_FAILURE;
	}
	printf("\nProbando con 10 bytes\n");
	t_datosEnviar* paquete = pedirPaquete("123456789",10,10);
	common_send(socket,paquete,terminar);
	free(paquete);
	paquete = common_receive(socket,terminar);
	printf("\nStatus del paquete de 10: %s\n",(char*) paquete->data);
	destruirPaquete(paquete);

	printf("\nProbando con 128 bytes\n");
	char* temp = conitos_malloc(128);
	paquete = pedirPaquete(temp,128,128);
	common_send(socket,paquete,terminar);
	destruirPaquete(paquete);
	paquete = common_receive(socket,terminar);
	printf("\nStatus del paquete de 128: %s\n",(char*) paquete->data);
	destruirPaquete(paquete);

	printf("\nProbando con un string pasado por stdin\n");
	temp = conitos_malloc(1024);
	fgets(temp, 1024, stdin);
	paquete = pedirPaquete(temp,10,string_length(temp) + 1 );
	common_send(socket,paquete,terminar);
	destruirPaquete(paquete);

	close(socket);
	printf("\nFin del test. Adios!\n");
	return EXIT_SUCCESS;
}
