#include "semaforos.h"

void blockProcess(t_queue* blockedQueue,
		t_nodo_proceso_ejecutando* procesoEjecutando)
{
	t_nodo_proceso* copia = malloc(sizeof *copia);
	memcpy(copia, &(procesoEjecutando->proceso), sizeof *copia);
	queue_push(blockedQueue, copia);
//	stopProcessing(procesoEjecutando);
}

t_semaforo *semaforo_create(char*nombre, int32_t valor)
{
	t_semaforo *semaforo = malloc(sizeof(t_semaforo));
	semaforo->nombre = strdup(nombre);
	semaforo->valor = valor;
	semaforo->bloqueados = queue_create();
	return semaforo;
}

void semaforo_signal(t_semaforo* semaforo)
{
	semaforo->valor++;
	debugTrackPCP("Semaphore current value: %d",semaforo->valor);
	t_nodo_proceso* blockedProcess = queue_pop(semaforo->bloqueados);
	if (blockedProcess != NULL )
	{
		debugTrackPCP("Unblocking process %d, moving to ready.",
				blockedProcess->pcb.pid);
		addToReadyQueue(blockedProcess);
		free(blockedProcess);
	}
}

bool semaforo_wait(t_semaforo* semaforo,
		t_nodo_proceso_ejecutando* procesoEjecutando)
{
	if (semaforo->valor > 0)
	{
		debugTrackPCP("Wait successful.");
		semaforo->valor--;
		return false;
	}
	debugTrackPCP("Wait could not be satisfied, blocking process.");
	blockProcess(semaforo->bloqueados, procesoEjecutando);
	return true;
}
