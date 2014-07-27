#include "globals.h"
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <conitos-estaCoverflow/common_sockets.h>
#include <conitos-estaCoverflow/conitos_protocol.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

t_nodo_proceso* getProcessToDispatch()
{
	debugTrackPCP("[Dispatcher] Waiting on ready list...");
	sem_wait(&sem_listaListos);
	pthread_mutex_lock(&mutex_listaListos);
	t_nodo_proceso* processToDispatch = queue_pop(listaListos);
	pthread_mutex_unlock(&mutex_listaListos);
	listarListos();
	return processToDispatch;
}

t_cpu* getCPUToProcess()
{
	debugTrackPCP("[Dispatcher] Waiting for a CPU...");
	sem_wait(&sem_listaCpu);
	pthread_mutex_lock(&mutex_listaCPU);
	t_cpu *cpu = queue_pop(listaCPULibres);
	pthread_mutex_unlock(&mutex_listaCPU);
	listarCPUs();
	return cpu;

}

void addProcessToExecutingList(t_nodo_proceso_ejecutando* proceso)
{
	pthread_mutex_lock(&mutex_listaEjecutando);
	debugTrackPCP("[Dispatcher] Adding process to executing list.");
	list_add(listaEjecutando, proceso);
	pthread_mutex_unlock(&mutex_listaEjecutando);
	listarEjecutando();
}

void dispatch(t_cpu* cpu, t_nodo_proceso* proceso)
{
	t_datosEnviar* mensajeCPU = pedirPaquete(&(proceso->pcb),
			PCP_CPU_PROGRAM_TO_EXECUTE, sizeof(t_pcb));
	common_send(cpu->socket, mensajeCPU, NULL );
	debugTrackPCP("[Dispatcher] Dispatching process %d to CPU %d .", proceso->pcb.pid,
			cpu->pid);
	log_info(log_kernel, "[Dispatcher] Dispatching process %d to CPU %d .", proceso->pcb.pid,
			cpu->pid);
	destruirPaquete(mensajeCPU);
}

void addToExecutingList(t_cpu* cpu, t_nodo_proceso* proceso)
{
	t_nodo_proceso_ejecutando* nuevo = malloc(sizeof *nuevo);
	nuevo->cpu = *cpu;
	nuevo->proceso = *proceso;
	addProcessToExecutingList(nuevo);
}

void *threadDispatcher(void *sinUso)
{
	debugTrackPCP("[Dispatcher] Dispatcher is ON Bitches");
	while (1)
	{
		t_nodo_proceso* processToDispatch = getProcessToDispatch();
		t_cpu *cpu = getCPUToProcess();
		dispatch(cpu, processToDispatch);
		addToExecutingList(cpu, processToDispatch);
		free(cpu);
		free(processToDispatch);
	}
	return NULL ;
}
