/* common_execution.c
 *
 *  Created on: Jul 8, 2014
 *      Author: root
 */

#include "common_execution.h"

void imprimirDiccionario(char* key,uint32_t *data){
	uint32_t offset;
	offset = *data + 1 - currentPCB->currentStack.base;
	char* mensaje = solicitar_algo_UMV(currentPCB->currentStack.base,
								offset,
								sizeof(t_valor_variable));
	log_info(logger," Variable:  %s \n Valor:  %d",key,*(mensaje));
	free(mensaje);
}


void terminarProceso(int codigoDeOperacion,void* datosExtra, t_size sizeExtra) {
	t_datosEnviar* paquete = pedirPaquete(currentPCB,codigoDeOperacion,sizeof(t_pcb));
	if(datosExtra != NULL){
		aniadirAlPaquete(paquete,datosExtra,sizeExtra);
	}
	log_debug(logger,"Termiando proceso, codOP: %d",codigoDeOperacion);
	terminar_proceso = true;
	proceso_activo = false;
	log_info(logger,"Estado variables:\n ");
	dictionary_iterator(diccionarioDeVariables,(void*)imprimirDiccionario);
	liberar_estructuras();
	common_send(pcpSocket,paquete,NULL);
	destruirPaquete(paquete);
	free(currentPCB);
}


void ejecutarSiSePerdioConexion() {

}

void ejecutarSiHuboSegmentationFault() {
	char* mensaje = strdup("SEGMENTATION FAULT");
	terminarProceso(CPU_PCP_EXCEPTION,mensaje,strlen(mensaje) + 1);
	free(mensaje);
}

void clean_variable_dictionary() {
	dictionary_destroy_and_destroy_elements(diccionarioDeVariables,free);
}

void recreate_variable_dictionary() {
		char* aux;
		uint32_t offset, tamanio;
		diccionarioDeVariables = dictionary_create();
		tamanio = currentPCB->currentStack.contextSize * (sizeof(t_valor_variable) + sizeof(t_nombre_variable));
		offset = currentPCB->currentStack.stack_pointer
				- currentPCB->currentStack.base; //TODO verificar que esto este bien
		aux = solicitar_algo_UMV(currentPCB->currentStack.base, offset, tamanio); //FIXME ver esto
		printf("\n - %d - %d - %d - \n", currentPCB->currentStack.base, offset, tamanio);
		informacion_diccvar = conitos_malloc(tamanio);
		memcpy(informacion_diccvar, aux, tamanio);
		llenar_diccionario();
		free(informacion_diccvar);
}


char* charToString(char element) {
	char* new = conitos_malloc(2);
	*new = element;
	*(new + 1) = '\0';
	return new;
}

void ejecutarSiHuboStackOverflow() {
	char* mensaje = strdup("STACK_OVERFLOW");
	terminarProceso(CPU_PCP_EXCEPTION,mensaje,strlen(mensaje) + 1);
	free(mensaje);
}

void noHacerNada() {

}

void* solicitar_algo_UMV(t_puntero base, int32_t offset, uint32_t tamanio) {
	t_datosEnviar* paquete = paquete_leerUMV(base, offset, tamanio);
	common_send(umvSocket, paquete, NULL);
	destruirPaquete(paquete);
	paquete = common_receive(umvSocket, NULL);
	if (paquete->codigo_Operacion != UMV_MENSAJE_CPU_LEER) {
		log_debug(logger,"Hubo un error de lectura, cierro la ejecución del programa");
		char* mensaje = strdup("Segmentation Fault");
		terminarProceso(CPU_PCP_EXCEPTION,mensaje,strlen(mensaje) + 1);
		free(mensaje);
	} else {
		void* copy = malloc(paquete->data_size);
		memcpy(copy,paquete->data,paquete->data_size);
		destruirPaquete(paquete);
		return copy;
	}
	destruirPaquete(paquete);
	return NULL;
}

void llenar_diccionario(){
	int aux, offset;
		aux = 0;
		char nombre_variable;
		char* key;
		t_valor_variable* value;
		while (aux < currentPCB->currentStack.contextSize) {
			offset = currentPCB->currentStack.stack_pointer - currentPCB->currentStack.base + aux * (sizeof(t_valor_variable) + sizeof(t_nombre_variable));
			(aux)++;
			memcpy(&nombre_variable,informacion_diccvar + offset,sizeof(char));
			key = charToString(nombre_variable);
			log_debug(logger,"OFFSET: %d",offset);
			value = conitos_malloc(sizeof *value);
			(*value) = currentPCB->currentStack.stack_pointer + offset;
			log_debug(logger,"Valor de la variable %s      %d",key,value);
			dictionary_put(diccionarioDeVariables, key, value);
			free(key);
		}


}

void liberar_estructuras() {
	if (diccionarioEtiquetas != NULL)
		free(diccionarioEtiquetas);
	if (informacion_diccvar != NULL)
		free(informacion_diccvar);
	dictionary_destroy_and_destroy_elements(diccionarioDeVariables,free);
}

