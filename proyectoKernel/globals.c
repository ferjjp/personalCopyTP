#include "globals.h"
#include "semaforos.h"
#include "entradaSalida.h"
#include <string.h>
#include <commons/string.h>
#include <commons/log.h>

#define pcpDebugStart "[DEBUG] - PCP - "
#define plpDebugStart "[DEBUG] - PLP - "
#define kernelDebugStart "[DEBUG] - KERNEL - "

char* copy_on_string(char* string, char* otherString, va_list arguments) {
	char* aux = string_from_vformat(string,arguments);
	char* copy = string_duplicate(otherString);
	string_append(&aux,"\n");
	string_append(&copy, aux);
	free(aux);
	return copy;
}

#define debugTrack(debugName, messageStart,logger)\
			void debugName(char* string, ...) {	\
					va_list arguments;	\
					va_start(arguments, string);	\
					char* temp = copy_on_string(string,messageStart,arguments);\
					if (debugOn) puts(temp);\
					log_debug(logger,temp);\
					va_end(arguments);	\
					free(temp);\
				}	\


debugTrack(debugTrackPCP, pcpDebugStart,log_kernel)

debugTrack(debugTrackPLP, plpDebugStart,log_kernel)

debugTrack(debugTrackKERNEL, kernelDebugStart,log_kernel)

t_pcb* unserializePCB(void* data) {
	t_pcb* pcb = malloc(sizeof *pcb);
	memcpy(pcb, data, sizeof *pcb);
	return pcb;
}

void removeFromExecutingList(t_nodo_proceso_ejecutando* process) {
	bool checkByProcessID(void* nodo) { // o por socket cpu, da igual jaja
		return ((t_nodo_proceso_ejecutando*) nodo)->proceso.pcb.pid
				== process->proceso.pcb.pid;
	}
	pthread_mutex_lock(&mutex_listaEjecutando);
	debugTrackPCP("Removing process from executing list.");
	list_remove_by_condition(listaEjecutando, checkByProcessID);
	pthread_mutex_unlock(&mutex_listaEjecutando);
	listarEjecutando();
}

void addCPUToCPUFreeList(t_cpu* cpu) {
	t_cpu* copia = malloc(sizeof *copia);
	memcpy(copia, cpu, sizeof *copia);
	pthread_mutex_lock(&mutex_listaCPU);
	debugTrackPCP("Adding the CPU to the free CPU list.");
	queue_push(listaCPULibres, copia);
	pthread_mutex_unlock(&mutex_listaCPU);
	sem_post(&sem_listaCpu);
	listarCPUs();
}

t_nodo_proceso* copyProcessNode(t_nodo_proceso* proceso) {
	t_nodo_proceso* copia = malloc(sizeof *copia);
	memcpy(copia, proceso, sizeof *copia);
	return copia;
}

void addToReadyQueue(t_nodo_proceso* proceso) {
	t_nodo_proceso* copia = copyProcessNode(proceso);
	pthread_mutex_lock(&mutex_listaListos);
	debugTrackPCP("Proceeding to move the process to ready queue.");
	queue_push(listaListos, copia);
	pthread_mutex_unlock(&mutex_listaListos);
	sem_post(&sem_listaListos);
	listarListos();
}

void pcp_exitProcess(t_nodo_proceso_ejecutando* proceso) {
	t_nodo_proceso* copia = copyProcessNode(&(proceso->proceso));
	pthread_mutex_lock(&mutex_listaTerminados);
	queue_push(listaTerminados, copia);
	debugTrackPCP("Adding process to finished queue.");
	pthread_mutex_unlock(&mutex_listaTerminados);
	sem_post(&sem_listaTerminados);
	sem_post(&sem_multiprog);
	listarTerminados();
	addCPUToCPUFreeList(&(proceso->cpu));
	removeFromExecutingList(proceso);
}

void stopProcessing(t_nodo_proceso_ejecutando* procesoEjecutando) {
	removeFromExecutingList(procesoEjecutando);
	addCPUToCPUFreeList(&(procesoEjecutando->cpu));
}

void imprimirFranjaDeConitosSuperior() {
	debugTrackKERNEL(
			"\e[38;5;166m/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\\e[0m\n");
}

void imprimirFranjaDeConitosInferior() {
	debugTrackKERNEL(
			"\e[38;5;166m\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\e[0m\n");
}

void removeCPUFromPoll(t_cpu* cpu, struct pollfd** arraySocketsPCP,
		int* cantSocketsCPUAbiertos) {
	int i = 0;

	while ((i < *cantSocketsCPUAbiertos)
			&& ((*arraySocketsPCP)[i].fd != cpu->socket)) {
		i++;
	};
	debugTrackPCP("Closing socket:%i on poll position: %i\n", cpu->socket, i);
	close(cpu->socket);
	if (i != *cantSocketsCPUAbiertos - 1) {
		memcpy(&arraySocketsPCP[i], &arraySocketsPCP[i + 1],
				sizeof(struct pollfd) * (*cantSocketsCPUAbiertos - i - 1));
	}
	(*cantSocketsCPUAbiertos)--;
	*arraySocketsPCP = realloc(*arraySocketsPCP,
			sizeof(struct pollfd) * (*cantSocketsCPUAbiertos));
}

void listarListos() {
	pthread_mutex_lock(&mutex_listaListos);
	debugTrackKERNEL("\n\e[38;5;166m|>\e[0m COLA LISTOS     Tamaño: %d", queue_size(listaListos));
	int i;
	imprimirFranjaDeConitosSuperior();
	for (i = 0; i < queue_size(listaListos); i++) {
		t_nodo_proceso *programa = list_get(listaListos->elements, i);
		debugTrackKERNEL(
				"\e[38;5;166m|\e[0m NODO: %i, ID_PROCESO: %i, SEGMENTO_CODIGO: %i, SEGMENTO_PILA: %i",
				i, programa->pcb.pid, programa->pcb.codeSegment,
				programa->pcb.currentStack.base);
	}
	pthread_mutex_unlock(&mutex_listaListos);
	imprimirFranjaDeConitosInferior();
}

void listarTerminados() {
	pthread_mutex_lock(&mutex_listaTerminados);
	debugTrackKERNEL("\n\e[38;5;166m|>\e[0m COLA TERMINADOS     Tamaño: %d", queue_size(listaTerminados));
	int i;
	imprimirFranjaDeConitosSuperior();
	for (i = 0; i < queue_size(listaTerminados); i++) {
		t_nodo_proceso *programa = list_get(listaTerminados->elements, i);
		debugTrackKERNEL(
				"\e[38;5;166m|\e[0m NODO: %i, ID_PROCESO: %i, SEGMENTO_CODIGO: %i, SEGMENTO_PILA: %i",
				i, programa->pcb.pid, programa->pcb.codeSegment,
				programa->pcb.currentStack.base);
	}
	pthread_mutex_unlock(&mutex_listaTerminados);
	imprimirFranjaDeConitosInferior();
}

void listarNuevos() {
	pthread_mutex_lock(&mutex_listaNuevos);
	debugTrackKERNEL("\n\e[38;5;166m|>\e[0m LISTA NUEVOS     Tamaño: %d", list_size(listaNuevos));
	int i;
	imprimirFranjaDeConitosSuperior();
	for (i = 0; i < list_size(listaNuevos); i++) {
		t_nodo_proceso *programa = list_get(listaNuevos, i);
		debugTrackKERNEL(
				"\e[38;5;166m|\e[0m NODO: %i, ID_PROCESO: %i, SEGMENTO_CODIGO: %i, SEGMENTO_PILA: %i",
				i, programa->pcb.pid, programa->pcb.codeSegment,
				programa->pcb.currentStack.base);
	}
	pthread_mutex_unlock(&mutex_listaNuevos);
	imprimirFranjaDeConitosInferior();
}

void listarEjecutando() {
	pthread_mutex_lock(&mutex_listaEjecutando);
	debugTrackKERNEL("\n\e[38;5;166m|>\e[0m LISTA EJECUTANDO     Tamaño: %d", list_size(listaEjecutando));
	int i;
	imprimirFranjaDeConitosSuperior();
	for (i = 0; i < list_size(listaEjecutando); i++) {
		t_nodo_proceso_ejecutando *programa = list_get(listaEjecutando, i);
		debugTrackKERNEL(
				"\e[38;5;166m|\e[0m NODO: %i, ID_PROCESO: %i, SEGMENTO_CODIGO: %i, SEGMENTO_PILA: %i",
				i, programa->proceso.pcb.pid, programa->proceso.pcb.codeSegment,
				programa->proceso.pcb.currentStack.base);
	}
	pthread_mutex_unlock(&mutex_listaEjecutando);
	imprimirFranjaDeConitosInferior();
}

void listarSemaforos() {
	void imprimir(char*clave, t_semaforo* semaforo) {
		debugTrackKERNEL("\n\e[38;5;166m|\e[0m CLAVE: %s, NOMRE SEMAFORO: %s, VALOR SEMAFORO: %i", clave,
				semaforo->nombre, semaforo->valor);
	}
	imprimirFranjaDeConitosSuperior();
	dictionary_iterator(dictionarySemaphores, (void*) imprimir);
	imprimirFranjaDeConitosInferior();
}

void listarCPUs() {
	pthread_mutex_lock(&mutex_listaCPU);
	debugTrackKERNEL("\n\e[38;5;166m|>\e[0m LISTA CPU's LIBRES     Tamaño: %d", queue_size(listaCPULibres));
	int i;
	t_cpu* cpu;
	imprimirFranjaDeConitosSuperior();
	for (i = 0; i < queue_size(listaCPULibres); i++) {
		cpu = list_get(listaCPULibres->elements, i);
		debugTrackKERNEL("\e[38;5;166m|\e[0m CPU ID: %d     SOCKET: %i", cpu->pid, cpu->socket);
	}
	pthread_mutex_unlock(&mutex_listaCPU);
	imprimirFranjaDeConitosInferior();
}

void listarIO() {
	void imprimirHilosIO(char*clave, t_nodo_hiloIO* hiloIO) {
		debugTrackKERNEL("| NOMBRE HILO: %s, PROCESOS BLOQUEADOS:",
				hiloIO->dataHilo.nombre);
		int i;
		pthread_mutex_lock(&hiloIO->dataHilo.mutex_io);
		for (i = 0; i < queue_size(hiloIO->dataHilo.bloqueados); i++) {
			t_nodo_proceso *programa = list_get(
					hiloIO->dataHilo.bloqueados->elements, i);
			debugTrackKERNEL("\n\e[38;5;166m|\e[0m NODO: %i PID: %i", i, programa->pcb.pid);
		}
		pthread_mutex_unlock(&hiloIO->dataHilo.mutex_io);
	}
	imprimirFranjaDeConitosSuperior();
	dictionary_iterator(dictionaryIO, (void*) imprimirHilosIO);
	imprimirFranjaDeConitosInferior();
}

void destruirListas() {

}

void cerrarSemaforos() {
	pthread_mutex_destroy(&mutex_listaCPU);
	pthread_mutex_destroy(&mutex_listaTerminados);
	pthread_mutex_destroy(&mutex_semaforos);
	pthread_mutex_destroy(&mutex_listaListos);
	sem_destroy(&sem_listaNuevos);
	sem_destroy(&sem_listaListos);
	sem_destroy(&sem_multiprog);
	sem_destroy(&sem_semaforos);
}

void shutdownKernel() {
	destruirListas();
	cerrarSemaforos();
	exit(EXIT_FAILURE);
}
