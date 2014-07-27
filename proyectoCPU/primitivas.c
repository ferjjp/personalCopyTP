/* primitivas.c
 *
 *  Created on: Jun 15, 2014
 *      Author: root
 */

#include "primitivas.h"
#include "common_execution.h"
#include "globals.h"


/* ========================================================================================
 * ============================Uso Comun en todo el .c=====================================
 * ========================================================================================
 */

t_pc buscarEtiqueta(t_nombre_etiqueta etiqueta) {
	log_debug(logger, "buscarEtiqueta // Etiqueta: %s", etiqueta);
	t_pc new_programCounter = metadata_buscar_etiqueta(etiqueta, diccionarioEtiquetas,
			currentPCB->tamanioIndiceEtiquetas);
	log_debug(logger,"Buscar etiqueta encontro el program counter %d",new_programCounter);
	return --new_programCounter; //1 menos porque despues le sumas en ejecutar instruccion
}

void resetContextAndSetStackPointerTakingCareOfTheExtraBytesThatShouldNotBeThere(
		t_nombre_etiqueta etiqueta, int extraBytes) {
	log_debug(logger, "resetContextAndSetStackPointerTakingCareOfTheExtraBytesThatShouldNotBeThere // Etiqueta: %s - extraBytes %d", etiqueta, extraBytes);
	stack_changeContext(&(currentPCB->currentStack),extraBytes);
//	currentPCB->programCounter = buscarEtiqueta(etiqueta);
	irALabel(etiqueta);
	clean_variable_dictionary();
	diccionarioDeVariables = dictionary_create();
}

void change_context_common_push(stack* stack) {
	//--------------------
	//Stackout current stack pointer
	//--------------------
	stack_push(stack, &stack->stack_pointer, sizeof(t_puntero), umvSocket,
			stack_offsetFromContext(stack), ejecutarSiSePerdioConexion,
			ejecutarSiHuboStackOverflow);
	log_debug(logger, "Stacking stack_pointer: %d",stack->stack_pointer);
	//--------------------
	//Stackout PC
	//--------------------
	log_debug(logger, "Stacking program counter: %d",currentPCB->programCounter);
	stack_push(stack, &currentPCB->programCounter, sizeof(t_pc), umvSocket,
			stack_offsetFromContext(stack) - 1, ejecutarSiSePerdioConexion,
			ejecutarSiHuboStackOverflow);
}

/* ========================================================================================
 * ========================================================================================
 * ========================================================================================
 */


t_puntero definir_variable(t_nombre_variable nombre) {
	log_debug(logger, "Definiendo la variable: %c", nombre);
	char* key = charToString(nombre);
	stack_push(&(currentPCB->currentStack), &nombre, sizeof(t_nombre_variable),
			umvSocket, stack_offsetFromContext(&(currentPCB->currentStack)),
			ejecutarSiSePerdioConexion, ejecutarSiHuboStackOverflow);
	t_puntero* i = conitos_malloc(sizeof *i);
	*i = stack_lastVariablePosition(&(currentPCB->currentStack));
	dictionary_put(diccionarioDeVariables, key,i);
	free(key);
	return stack_lastVariablePosition(&(currentPCB->currentStack)) + 1; //TODO Fijarse: +1 para excluir el caracter.. no se si hay que hacer esto
}

t_puntero obtenerPosicionVariable(t_nombre_variable nombre) {
	char* key = charToString(nombre);
	t_puntero* posicionTotal = dictionary_get(diccionarioDeVariables, key);
	free(key);
	if (posicionTotal != NULL) {
		t_puntero posicion = *posicionTotal + 1; //+1 para excluir el caracter
		log_debug(logger, "Obtengo la posicion de la variable: %c . Su posicion es: %d",nombre,posicion);
		return posicion;
	}
	log_debug(logger,"No pude encontrar la posicion de la variable: %c en mi diccionario",nombre);
	return -1;
}

t_valor_variable desreferenciar(t_puntero direccion) {
	log_debug(logger, "Desreferencio la direccion: %d",direccion);
	t_datosEnviar* paquete = paquete_leerUMV((currentPCB->currentStack.base),
			direccion - currentPCB->currentStack.base,
			sizeof(t_valor_variable));
	t_datosEnviar* respuesta = enviar_y_esperar_respuesta_con_paquete(umvSocket,
			paquete, ERROR_FALLO_SEGMENTO, ejecutarSiSePerdioConexion,
			ejecutarSiHuboSegmentationFault);
	destruirPaquete(paquete);
	t_valor_variable retorno = *((t_valor_variable*)respuesta->data);
	log_debug(logger, "El valor encontrado fue: %d",retorno);
	destruirPaquete(respuesta);
	return retorno;
}

void asignar(t_puntero direccion, t_valor_variable valor) {
	log_debug(logger, "Asigno en la direccion %d el valor: %d",direccion, valor);
	t_datosEnviar* paquete = paquete_escribirUMV((currentPCB->currentStack.base),
			direccion - (currentPCB->currentStack.base),
			sizeof(t_valor_variable), &valor);
	t_datosEnviar* nuevo_paquete = enviar_y_esperar_respuesta_con_paquete(
			umvSocket, paquete, ERROR_FALLO_SEGMENTO,
			ejecutarSiSePerdioConexion, ejecutarSiHuboSegmentationFault);
	destruirPaquete(paquete);
	destruirPaquete(nuevo_paquete);
}

t_valor_variable obtenerValorCompartida(t_nombre_compartida variable) {
	log_debug(logger, "obtenerValorCompartida // variable: %s",variable);
	t_datosEnviar* respuesta = enviar_y_esperar_respuesta(pcpSocket,
			(void*) variable, CPU_PCP_PEDIR_VARIABLE_COMPARTIDA,
			string_length(variable) + 1, NO_HAY_ERROR_POSIBLE,
			ejecutarSiSePerdioConexion, noHacerNada);
	t_valor_variable retorno = *(t_valor_variable*)respuesta->data;
	destruirPaquete(respuesta); // otra vez :D
	return retorno;
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable,
		t_valor_variable valor) {
	log_debug(logger, "asignarValorCompartida // variable: %s - valor: %d",variable, valor);
	t_datosEnviar* paquete = paquete_asignarCompartida(variable, valor);
	common_send(pcpSocket, paquete, ejecutarSiSePerdioConexion);
	destruirPaquete(paquete);
	return valor;
}

void irALabel(t_nombre_etiqueta etiqueta) {
	char* etiquetaABuscar = string_substring_until(etiqueta,string_length(etiqueta) - 1);
	currentPCB->programCounter = buscarEtiqueta(etiquetaABuscar);
	free(etiquetaABuscar);
}

void llamarSinRetorno(t_nombre_etiqueta etiqueta) {
	log_debug(logger, "llamarSinRetorno // etiqueta: %s ",etiqueta);
	change_context_common_push(&(currentPCB->currentStack));
	//--------------------
	//Reset contextSize and set new stack_pointer
	//--------------------
	//Tiene 2 bytes de mas asi que..
	resetContextAndSetStackPointerTakingCareOfTheExtraBytesThatShouldNotBeThere(
			etiqueta, 2);

}

void llamarConRetorno(t_nombre_etiqueta etiqueta,t_puntero dondeRetornar) {
	log_debug(logger, "llamarConRetorno // etiqueta: %s - dondeRetornar: %d",etiqueta, dondeRetornar);
	change_context_common_push(&(currentPCB->currentStack));
	//--------------------
	//Stackout return address
	//--------------------
	stack_push(&(currentPCB->currentStack), &dondeRetornar, sizeof(t_puntero), umvSocket,
			stack_offsetFromContext(&(currentPCB->currentStack)) - 2, ejecutarSiSePerdioConexion,
			ejecutarSiHuboStackOverflow);
	log_debug(logger,"Where to return: %d", dondeRetornar);
	//--------------------
	//Reset contextSize and set new stack_pointer
	//--------------------
	//Tiene 3 bytes de mas asi que..
	resetContextAndSetStackPointerTakingCareOfTheExtraBytesThatShouldNotBeThere(
			etiqueta, 3);
}

void finalizar() {
	log_debug(logger, "Finalized executing context.");
	if (currentPCB->currentStack.base
			== currentPCB->currentStack.stack_pointer) {
		terminarProceso(CPU_PCP_FIN_PROCESO, NULL, 0);
	} else {
		t_pc* programCounter = stack_pop(&(currentPCB->currentStack),
				-sizeof(t_pc), sizeof(t_pc), umvSocket,
				ejecutarSiSePerdioConexion, ejecutarSiHuboSegmentationFault);
		log_debug(logger,"Returning program counter: %d",*programCounter);
		t_puntero* stack_pointer = stack_pop(&(currentPCB->currentStack),
				-sizeof(t_pc) - sizeof(t_puntero), sizeof(t_puntero), umvSocket,
				ejecutarSiSePerdioConexion, ejecutarSiHuboSegmentationFault);
		log_debug(logger,"Returning stack_pointer: %d",*stack_pointer);
		currentPCB->programCounter = *programCounter;
		stack_resetContext(&currentPCB->currentStack,stack_pointer);
		free(programCounter);
		free(stack_pointer);
		clean_variable_dictionary();
		recreate_variable_dictionary();
	}
}

void retornar(t_valor_variable retorno) {
	log_debug(logger, "Returning the value: %d",retorno);
	t_puntero* direccion_de_retorno = stack_pop(&(currentPCB->currentStack),
			-sizeof(t_puntero), sizeof(t_puntero), umvSocket,
			ejecutarSiSePerdioConexion, ejecutarSiHuboSegmentationFault);
	log_debug(logger,"Returning Address: %d",*direccion_de_retorno);
	asignar(*direccion_de_retorno,retorno);
	currentPCB->currentStack.stack_pointer -= sizeof(t_puntero);
	finalizar();
}

//syscall
void imprimir(t_valor_variable valor_mostrar) {
	log_debug(logger,"[Syscall] Enviando variable a imprimir: %d",valor_mostrar);
	t_datosEnviar* paquete = pedirPaquete(&valor_mostrar,
	CPU_PCP_IMPRIMIR_VARIABLE, sizeof(t_valor_variable));
	common_send(pcpSocket, paquete, ejecutarSiSePerdioConexion);
	destruirPaquete(paquete);
}

//syscall
void imprimirTexto(char* Texto) {
	log_debug(logger, "(SYSCALL) imprimirTexto // Texto: %s",Texto);
	t_datosEnviar* paquete = pedirPaquete(Texto, CPU_PCP_IMPRIMIR_TEXTO,
			string_length(Texto) + 1);
	common_send(pcpSocket, paquete, ejecutarSiSePerdioConexion);
	destruirPaquete(paquete);
}

//syscall
void entradaSalida(t_nombre_dispositivo dispositivo, int tiempo) {
	log_debug(logger, "(SYSCALL) entradaSalida // dispositivo: %s - tiempo: %d",dispositivo, tiempo);
	t_size sizeString = string_length(dispositivo) + 1;
	void* datosExtra = conitos_malloc(sizeString + sizeof(int));
	memcpy(datosExtra, dispositivo, sizeString);
	memcpy(datosExtra + sizeString, &tiempo, sizeof(int));
	terminarProceso(CPU_PCP_ENTRADA_SALIDA, datosExtra,
			sizeString + sizeof(int));
	free(datosExtra);
}

void p_signal(t_nombre_semaforo identificador_semaforo) {
	log_debug(logger, "(SYSCALL) p_signal // identificador_semaforo: %s",identificador_semaforo);
	t_datosEnviar* paquete = pedirPaquete(identificador_semaforo,
	CPU_PCP_SIGNAL, string_length(identificador_semaforo) + 1);
	common_send(pcpSocket, paquete,NULL);
	destruirPaquete(paquete);
}

void p_wait(t_nombre_semaforo identificador_semaforo) {
	log_debug(logger, "(SYSCALL) p_signal // identificador_semaforo: %s",identificador_semaforo);
	t_datosEnviar* paquete = pedirPaquete(identificador_semaforo, CPU_PCP_WAIT,
			string_length(identificador_semaforo) + 1);
	t_datosEnviar* respuesta = enviar_y_esperar_respuesta_con_paquete(pcpSocket,
			paquete, NO_HAY_ERROR_POSIBLE, ejecutarSiSePerdioConexion,
			noHacerNada);
	destruirPaquete(paquete);
	if (respuesta->codigo_Operacion != PCP_CPU_WAIT_OK)
		terminarProceso(CPU_PCP_PROCESS_WAITING, NULL, 0);
	destruirPaquete(respuesta);
}
