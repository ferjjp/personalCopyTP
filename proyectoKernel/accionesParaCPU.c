#include <conitos-estaCoverflow/common_sockets.h>
#include <conitos-estaCoverflow/conitos_protocol.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "globals.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/sem.h>
#include <poll.h>
#include <errno.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include <commons/log.h>
#include <string.h>
#include <signal.h>
#include "semaforos.h"
#include "entradaSalida.h"
#include "accionesParaCPU.h"

//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================
//=====================================================================================================================

void cpuFinishesQuantum(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{
	t_pcb* tmp = unserializePCB(paquete->data);
	debugTrackPCP(
			"Process just finished a quantum in CPU %d, updating PCB and removing the process from the executing list.",
			procesoEjecutando->cpu.pid);
	debugTrackPCP("Checksum PCB: PID %d \n CODE SEGMENT %d \n CODE INDEX %d \n STACK SEGMENT %d \n STACK CONTEXT SIZE %d\n ETIQUETAS SIZE %d\n"
			,tmp->pid,tmp->codeSegment,tmp->codeIndex,tmp->currentStack.base,tmp->currentStack.contextSize,tmp->tamanioIndiceEtiquetas);
	procesoEjecutando->proceso.pcb = *tmp;
	stopProcessing(procesoEjecutando);
	addToReadyQueue(&(procesoEjecutando->proceso));
	free(procesoEjecutando);
	free(tmp);
}

void signalRequest(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{
	t_semaforo* semaforo = dictionary_get(dictionarySemaphores, paquete->data);
	debugTrackPCP("Process sent a signal request on semaphore %s .",
			(char*) paquete->data);
	semaforo_signal(semaforo);
}

void sendWaitAnswer(int cpuSocket, int codOp)
{
	char* mensajeComun = strdup("o");
	t_datosEnviar* mensaje = pedirPaquete(mensajeComun, codOp,
			string_length(mensajeComun) + 1);
	common_send(cpuSocket, mensaje, NULL );
	destruirPaquete(mensaje);
}

t_pcb* onWaitGetProcessFromCPU(int socket)
{
	t_datosEnviar*respuesta = common_receive(socket, NULL );
	if (respuesta->codigo_Operacion == CPU_PCP_PROCESS_WAITING)
	{
		debugTrackPCP("Received PCB, proceeding to update...");
		t_pcb* nuevo = unserializePCB(respuesta);
		destruirPaquete(respuesta);
		return nuevo;
	}
	debugTrackPCP(
			"Received an incorrect Operation Code from PCB at waitRequest");
	destruirPaquete(respuesta);
	return NULL ;
}

void onWaitUpdatePCBAndCPU(t_pcb* nuevo,
		t_nodo_proceso_ejecutando* procesoEjecutando, t_semaforo* semaforo)
{
	procesoEjecutando->proceso.pcb = *nuevo;
	debugTrackPCP("Process %d is waiting on semaphore %s", nuevo->pid,
			semaforo->nombre);
	log_info(log_kernel, "Updating PCB, process %d is waiting on semaphore %s",
			nuevo->pid, semaforo->nombre);
	stopProcessing(procesoEjecutando);
}

void waitRequest(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{
	t_semaforo* semaforo = dictionary_get(dictionarySemaphores, paquete->data);
	debugTrackPCP("Process sent a wait request on semaphore %s",
			semaforo->nombre);
	if (semaforo_wait(semaforo, procesoEjecutando))
	{
		sendWaitAnswer(procesoEjecutando->cpu.socket, PCP_CPU_WAIT_NO_OK);
		t_pcb* nuevo = onWaitGetProcessFromCPU(procesoEjecutando->cpu.socket);
		onWaitUpdatePCBAndCPU(nuevo, procesoEjecutando, semaforo);
		free(nuevo);
	}
	else
	{
		sendWaitAnswer(procesoEjecutando->cpu.socket, PCP_CPU_WAIT_OK);
		debugTrackPCP("Process %d is using semaphore %s", semaforo->nombre);
		log_info(log_kernel, "Process %d is using semaphore %s",
				semaforo->nombre);
	}
}

void cpuFinishesProcess(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{
	debugTrackPCP("Process just finished running...");
	debugTrackPCP("Updating PCB.");
	t_pcb* pcb = unserializePCB(paquete->data);
	procesoEjecutando->proceso.pcb = *pcb;
	pcp_exitProcess(procesoEjecutando);
	//removeCPUFromPoll(&procesoEjecutando->cpu, socketsCPU, cantSocketsCpu);
	//kill(procesoEjecutando->cpu.pid, SIGUSR1);
	//debugTrackPCP(" Killing CPU.");
	free(procesoEjecutando);
	free(pcb);
}

void sharedVariableRequest(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{

	t_varCompartida* varBuscada = dictionary_get(dictionarySharedVariables,
			(char*) paquete->data);
	t_datosEnviar* mensaje = pedirPaquete(&varBuscada->valor, PCP_CPU_OK,
			sizeof(uint32_t));
	debugTrackPCP(" Request of the shared variable: %s.", mensaje->data);
	common_send(procesoEjecutando->cpu.socket, mensaje, NULL );
	destruirPaquete(mensaje);
}

void sharedVariableAssign(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{

	uint32_t valorVarCompartida;
	char* nombreVarCompartida = malloc(paquete->data_size - sizeof(uint32_t));
	memcpy(nombreVarCompartida, paquete->data,
			paquete->data_size - sizeof(uint32_t));
	memcpy(&valorVarCompartida,
			paquete->data + paquete->data_size - sizeof(uint32_t),
			sizeof(uint32_t));
	t_varCompartida* varBuscada = dictionary_get(dictionarySharedVariables,
			nombreVarCompartida);
	debugTrackPCP(
			" Request to assign to the shared variable %s the value %d. Current value: %d",
			nombreVarCompartida, valorVarCompartida, varBuscada->valor);
	varBuscada->valor = valorVarCompartida;
	free(nombreVarCompartida);
}

void printText(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{
	debugTrackPCP(" Request to print text \" %s \"", paquete->data);
	log_info(log_kernel,
			"Request to print text  \" %s \", requested by the CPU with pid %d\n",
			paquete->data, procesoEjecutando->cpu.pid);
	t_datosEnviar* mensaje = pedirPaquete(paquete->data,
			PROGRAMA_PAQUETE_PARA_IMPRIMIR, paquete->data_size);
	common_send(procesoEjecutando->proceso.soquet_prog, mensaje, NULL );
	destruirPaquete(mensaje);
}

void printVariable(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{
	debugTrackPCP(" Request to print variable: %d", *(int*)paquete->data);
	log_info(log_kernel,
			"Request to print variable \" %d \", requested by the CPU with pid %d\n",
			paquete->data, procesoEjecutando->cpu.pid);

	char* traduccion = string_itoa(*((int*) paquete->data));
	t_datosEnviar* mensaje = pedirPaquete(traduccion,
			PROGRAMA_PAQUETE_PARA_IMPRIMIR, string_length(traduccion) + 1);
	common_send(procesoEjecutando->proceso.soquet_prog, mensaje, NULL );
	destruirPaquete(mensaje);
	free(traduccion);
}

void inOut(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{
	t_pcb* pcb = unserializePCB(paquete->data);
	t_io* datos = unserializeIO(paquete->data + (sizeof *pcb),
			paquete->data_size - (sizeof *pcb));
	procesoEjecutando->proceso.pcb = *pcb;
	stopProcessing(procesoEjecutando);
	debugTrackPCP("Process requested I/O, executing I/O Handler...");
	log_info(log_kernel, "Process requested I/O, executing Handler...");
	doInOut(procesoEjecutando, datos);
	free(procesoEjecutando);
	free(datos);
	free(pcb);
}

void exceptionCPU(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{

	t_pcb* pcb = unserializePCB(paquete->data);
	char* textoError = malloc(paquete->data_size - sizeof(t_pcb));
	memcpy(textoError, paquete->data + sizeof(t_pcb),
			paquete->data_size - sizeof(t_pcb));

	debugTrackPCP(
			" Exception on the process with pid: %d, on the CPU  %d. Exception details: %s",
			pcb->pid, procesoEjecutando->cpu.pid, textoError);
	log_warning(log_kernel, "Exception on process. Exception details: %s",textoError);

	procesoEjecutando->proceso.pcb = *pcb;
	debugTrackPCP("Updating PCB.");
	t_datosEnviar* mensaje = pedirPaquete(textoError,
			PROGRAMA_PAQUETE_PARA_IMPRIMIR, string_length(textoError) + 1);
	common_send(procesoEjecutando->proceso.soquet_prog, mensaje, NULL );
	destruirPaquete(mensaje);
	pcp_exitProcess(procesoEjecutando);
	free(procesoEjecutando);
	free(textoError);
}

void cpuDisconnects(t_nodo_proceso_ejecutando* procesoEjecutando,
		struct pollfd** socketsCPU, int* cantSocketsCpu, t_datosEnviar* paquete)
{
	t_pcb* tmp = unserializePCB(paquete->data);
	debugTrackPCP(
			"CPU %d disconnected, updating PCB, removing the process from the executing list and removing CPU.",
			procesoEjecutando->cpu.pid);
	log_warning(log_kernel,
			"CPU %d disconnected, updating PCB, removing the process from the executing list and removing CPU.",
			procesoEjecutando->cpu.pid);
	procesoEjecutando->proceso.pcb = *tmp;
	removeFromExecutingList(procesoEjecutando);
	removeCPUFromPoll(&procesoEjecutando->cpu, socketsCPU, cantSocketsCpu);
	addToReadyQueue(&(procesoEjecutando->proceso));
	free(procesoEjecutando);
	free(tmp);
}
