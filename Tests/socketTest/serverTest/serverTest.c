/*
 * serverTest.c
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
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>

int socketEscuchando;
int socketConectado;

void terminar() {
	error_show("\nOcurrio un error, terminando el programa\n");
	close(socketEscuchando);
	close(socketConectado);
	exit(0);
}

int main() {
	socketEscuchando = setup_listen("5000");
	if (socketEscuchando == -1) {
		return EXIT_FAILURE;
	}
	printf("\nPreparando para recibir: \n");

	listen(socketEscuchando,50);

	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	socketConectado = accept(socketEscuchando, (struct sockaddr *) &addr, &addrlen);

	t_datosEnviar* nuevo = common_receive(socketConectado,terminar);
	t_datosEnviar* respuesta;

	if (!strcmp((char*) nuevo->data, "123456789") ){
		printf("\n10 bytes: Good\n");
		respuesta = pedirPaquete("Good",10,5);
		common_send(socketConectado,respuesta,terminar);
	} else {
		printf("\n10 bytes: Bad\n");
		respuesta = pedirPaquete("Bad",10,4);
		common_send(socketConectado,respuesta,terminar);

	}

	printf("\nProbando con 128 bytes \n");
	destruirPaquete(nuevo);
	free(respuesta);
	nuevo = common_receive(socketConectado,terminar);

	if (nuevo->data_size == 128) {
		printf("\n128 bytes: Good\n");
		respuesta = pedirPaquete("Good",10,16);
		common_send(socketConectado,respuesta,terminar);
	} else {
		printf("\n128 bytes: Bad\n");
		respuesta = pedirPaquete("Bad",10,15);
		common_send(socketConectado,respuesta,terminar);
	}
	destruirPaquete(nuevo);
	free(respuesta);

	nuevo = common_receive(socketConectado,terminar);
	printf("\nRecibido: %d bytes \n Mensaje: %s \n",nuevo->data_size,(char*) nuevo->data);
	destruirPaquete(nuevo);

	printf("\nFin del test. Adios\n!");
	close(socketConectado);
	close(socketEscuchando);
	return EXIT_SUCCESS;
}
