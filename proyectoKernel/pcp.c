#include <conitos-estaCoverflow/common_sockets.h>
#include <conitos-estaCoverflow/conitos_protocol.h>
#include <stdint.h>
#include <stdbool.h>
#include "globals.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/sem.h>
#include <poll.h>
#include <errno.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include "accionesParaCPU.h"
#include <netinet/in.h>
#include "dispatcher.h"
#include "entradaSalida.h"

void sendHandshake(int cpuSocket) {
	uint32_t* i = malloc(sizeof(*i));
	*i = configuration.quantum;
	t_datosEnviar* handshakeOK = pedirPaquete(i,
	PCP_CPU_OK, sizeof(uint32_t));
	*i = configuration.retardo;
	aniadirAlPaquete(handshakeOK, i, sizeof(uint32_t));
	common_send(cpuSocket, handshakeOK, NULL);
	destruirPaquete(handshakeOK);
	free(i);
}

//NOW DO THE HARLEMSHAKE
void doHandshakeCPU(struct pollfd** socketsPCP, int* cantidadDeSockets,
		int cpuSocket) {
	t_datosEnviar* respuesta = common_receive(cpuSocket, NULL);
	if (respuesta == NULL) {
		debugTrackPCP(
				"[NEW CPU CONNECTION HANDLER] Connection error, handler will not include the socket %d on poll.",
				cpuSocket);
		destruirPaquete(respuesta);
		return;
	}
	if (respuesta->codigo_Operacion == HANDSHAKE) {
		t_cpu* nueva_cpu = malloc(sizeof *nueva_cpu);
		nueva_cpu->pid = *((t_pid*) respuesta->data);
		nueva_cpu->socket = cpuSocket;

		(*cantidadDeSockets)++;
		*socketsPCP = realloc(*socketsPCP,
				sizeof(struct pollfd) * (*cantidadDeSockets));
		(*socketsPCP + ((*cantidadDeSockets) - 1))->fd = cpuSocket;
		(*socketsPCP + ((*cantidadDeSockets) - 1))->events = POLLIN;
		debugTrackPCP(
				"[NEW CPU CONNECTION HANDLER] Successful connection to CPU whose PID is %d , on socket %d",
				*((int*) respuesta->data), cpuSocket);

		addCPUToCPUFreeList(nueva_cpu);
		sendHandshake(cpuSocket);
		free(nueva_cpu);
	} else {
		debugTrackPCP(
				"[NEW CPU CONNECTION HANDLER] Couldn't realize handshake correctly. Skipping and ending handler..");
		destruirPaquete(respuesta);
		return;
	}
	destruirPaquete(respuesta);
}

void newConnectionHandlerCPU(struct pollfd** socketsPCP, int* cantidadDeSockets) {
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int nuevaCPU = accept((*socketsPCP)->fd, (struct sockaddr *) &addr,
			&addrlen);
	debugTrackPCP(" [NEW CPU CONNECTION HANDLER] New CPU file descriptor: %i",
			nuevaCPU);
	doHandshakeCPU(socketsPCP, cantidadDeSockets, nuevaCPU);
}

void removeCPUFromKernel(int socketCPU, struct pollfd** socketsPCP,
		int* cantidadDeSockets) {
	bool buscarPorSocketCPU(void* cpu) {
		return (((t_cpu*) cpu)->socket) == socketCPU;
	}
	debugTrackPCP(
			"[CPU HANDLER] CPU is disconnecting. CPU was not executing anything. Proceeding to remove from List..");
	pthread_mutex_lock(&mutex_listaCPU);
	t_cpu* cpu = list_remove_by_condition(listaCPULibres->elements,
			buscarPorSocketCPU);
	free(cpu);
	pthread_mutex_unlock(&mutex_listaCPU);
	removeFDFromPoll(socketCPU, socketsPCP, cantidadDeSockets);
	listarCPUs();
}

void handlerCPU(struct pollfd** socketsPCP, int* cantidadDeSockets,
		int socketCPU) {
	t_datosEnviar* paquete = common_receive(socketCPU, NULL);
	t_cpu_action accion = dictionary_get(cpu_command_dictionary,
			string_itoa(paquete->codigo_Operacion));

	bool buscarPorSocketCPU(void* proceso) {
		return (((t_nodo_proceso_ejecutando*) proceso)->cpu.socket) == socketCPU;
	}

	debugTrackPCP("[CPU HANDLER] Searching for process executing on CPU");
	if (list_size(listaEjecutando) > 0) {
		pthread_mutex_lock(&mutex_listaEjecutando);
		t_nodo_proceso_ejecutando* procesoEjecutando = list_find(
				listaEjecutando, buscarPorSocketCPU);
		pthread_mutex_unlock(&mutex_listaEjecutando);
		debugTrackPCP(
				"[CPU HANDLER] Process found with ID [[%d]]  on CPU %d , executing action handler",
				procesoEjecutando->proceso.pcb.pid, procesoEjecutando->cpu.pid);
		accion(procesoEjecutando, socketsPCP, cantidadDeSockets, paquete);
	} else {
		debugTrackPCP("[CPU HANDLER] No process was found running on CPU");
		if (paquete->codigo_Operacion == CPU_PCP_DISCONNECTION) {
			removeCPUFromKernel(socketCPU, socketsPCP, cantidadDeSockets);
			listarCPUs();
		}
	}
	destruirPaquete(paquete);
}

t_nodo_hiloIO* createIONode(int index) {
	t_nodo_hiloIO *hilo = malloc(sizeof(t_nodo_hiloIO));
	hilo->dataHilo.retardo = atoi(configuration.valorHIO[index]);
	sem_init(&hilo->dataHilo.sem_io, 0, 0);
	pthread_mutex_init(&hilo->dataHilo.mutex_io, NULL);
	hilo->dataHilo.bloqueados = queue_create();
	hilo->dataHilo.nombre = strdup(configuration.idHIO[index]);
	return hilo;
}

void launchIOThreads() {
	int i = 0;
	while (configuration.idHIO[i] != '\0') {
		t_nodo_hiloIO *hilo = createIONode(i);
		pthread_create(&hilo->hiloID, NULL, &inOutThread,
				(void*) &hilo->dataHilo);
		dictionary_put(dictionaryIO, configuration.idHIO[i], hilo);
		debugTrackPCP(
				"[I/O Thread Launcher] Launching thread number %d belonging to device %s",
				i, hilo->dataHilo.nombre);
		i++;
	}
	debugTrackPCP("[I/O Thread Launcher] Finished launching threads.");
}

void* threadPCP(void* cosa) {
	debugTrackPCP("Starting PCP");
	struct pollfd *arraySocketsPCP = malloc(sizeof *arraySocketsPCP);

	int i, error, listeningSocketCPU, cantSocketsCPUAbiertos;
	pthread_t hilo_dispatcher;

	pthread_create(&hilo_dispatcher, NULL, *threadDispatcher, NULL);
	launchIOThreads();
	listeningSocketCPU = setup_listen(configuration.puertoCPU);
	arraySocketsPCP->fd = listeningSocketCPU;
	arraySocketsPCP->events = POLLIN;
	cantSocketsCPUAbiertos = 1;
	debugTrackPCP("Initializing POLL, listening socket is ready (%d)",
			listeningSocketCPU);
	listen(listeningSocketCPU, 1000);

	while (1) {
		error = poll(arraySocketsPCP, cantSocketsCPUAbiertos, -1);

		if (error == -1) {
			debugTrackPCP("Poll error %d, exiting kernel", errno);
			close(listeningSocketCPU);
			free(arraySocketsPCP);
			shutdownKernel(EXIT_FAILURE);
		}

		if (arraySocketsPCP[0].revents & POLLIN) {
			debugTrackPCP(
					"Listening socket is ready to receive a new connection, starting handler... \n");
			newConnectionHandlerCPU(&arraySocketsPCP, &cantSocketsCPUAbiertos);
		}
		for (i = 0; i < (cantSocketsCPUAbiertos - 1); i++) {
			if (arraySocketsPCP[i + 1].revents & POLLIN) {
				debugTrackPCP(
						"Activity detected on a CPU socket: %d, starting handler...\n",
						arraySocketsPCP[i + 1].fd);
				handlerCPU(&arraySocketsPCP, &cantSocketsCPUAbiertos,
						arraySocketsPCP[i + 1].fd);
			}

		}
	}
	pthread_join(hilo_dispatcher, NULL);
	return 0;
}

