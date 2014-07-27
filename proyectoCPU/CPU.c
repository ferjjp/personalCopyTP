#include "CPU.h"
#define HANDSHAKE_UMV 2

AnSISOP_funciones funciones ={
			.AnSISOP_asignar = asignar,
			.AnSISOP_definirVariable = definir_variable,
			.AnSISOP_dereferenciar = desreferenciar,
			.AnSISOP_finalizar = finalizar,
			.AnSISOP_imprimir = imprimir,
			.AnSISOP_imprimirTexto = imprimirTexto,
			.AnSISOP_irAlLabel = irALabel,
			.AnSISOP_llamarConRetorno = llamarConRetorno,
			.AnSISOP_llamarSinRetorno = llamarSinRetorno,
			.AnSISOP_obtenerPosicionVariable = obtenerPosicionVariable,
			.AnSISOP_retornar = retornar,
			.AnSISOP_obtenerValorCompartida = obtenerValorCompartida,
			.AnSISOP_asignarValorCompartida = asignarValorCompartida,
			.AnSISOP_entradaSalida = entradaSalida, };



AnSISOP_kernel funciones_kernel = { .AnSISOP_wait = p_wait, .AnSISOP_signal =
		p_signal };

char *diccionarioEtiquetas;
uint32_t retardo_asignado;
char *informacion_diccvar;
t_dictionary* diccionarioDeVariables;
t_pcb* currentPCB;
int pcpSocket;
int umvSocket;
sem_t sem_hotPlug;
bool desconexion = false;
t_log* logger;
bool terminar_proceso = false;
bool proceso_activo = false;
t_config *configuracion_CPU;

int main(int argc, char **argv) {
	pthread_t idHiloHotPlug;
	sem_init(&sem_hotPlug, 0, 0);
	signal(SIGUSR1, desconectarme);
	signal(SIGINT,desconectarme);
	pthread_create(&idHiloHotPlug, NULL,hot_plug, NULL);
	logger = log_create(argv[2], "CPU", true, LOG_LEVEL_DEBUG);
	log_info(logger, "Creado logger");
	uint32_t pidCPU = getpid();
	log_info(logger, "CPU número:%i",pidCPU);


	configuracion_CPU = config_create(argv[1]);
	char* IPKernel= config_get_string_value(configuracion_CPU, "IPKernel");
	char* IP_UMV = config_get_string_value(configuracion_CPU, "IPUMV");
	char* puertoKernel = config_get_string_value(configuracion_CPU, "PuertoKernel");
	char* puertoUMV = config_get_string_value(configuracion_CPU, "PuertoUMV");
	char* handshake = "Hola soy un CPU";
	char* mensaje_inicial = malloc(sizeof(uint32_t));
	memcpy(mensaje_inicial, &pidCPU, sizeof(uint32_t));

	umvSocket = connect_to(IP_UMV, puertoUMV);
	if (umvSocket == -1) {
		printf("Fallo conexion UMV");
		log_debug(logger,"Fallo conexion con UMV, me voy");
		close(umvSocket);
		log_destroy(logger);
		return EXIT_FAILURE;
	}
	log_debug(logger, "Conexion con UMV hecha");

	pcpSocket = connect_to(IPKernel, puertoKernel);
	if (pcpSocket == -1) {
		printf("Fallo conexion Kernel");
		log_debug(logger,"Fallo conexion con Kernel, me voy");
		close(pcpSocket);
		char* aviso = strdup("Ocurrio un error con el kernel");
		t_datosEnviar* paquete = pedirPaquete(aviso, CPU_PCP_EXCEPTION,
			sizeof(aviso));
		common_send(umvSocket,paquete,NULL);
		destruirPaquete(paquete);
		free(aviso);
		log_debug(logger,"Avise a la UMV de fallo con kernel");
		log_destroy(logger);
		close(umvSocket);
		return EXIT_SUCCESS;
		}


	log_debug(logger, "Conexion con Kernel hecha");
	char* dupHandshake = strdup(handshake);
	t_datosEnviar* paquete = pedirPaquete(dupHandshake, HANDSHAKE_UMV,
			string_length(dupHandshake) + 1);
	common_send(umvSocket, paquete, NULL);
	free(dupHandshake);
	destruirPaquete(paquete);
	paquete = common_receive(umvSocket,NULL);
	if(paquete->codigo_Operacion != UMV_CONTESTA_CPU_EXITO){
		log_debug(logger,"Fallo handshake");
	}
	destruirPaquete(paquete);
	log_debug(logger, "Handshake con UMV exitoso");
	t_datosEnviar* paquete2 = pedirPaquete(mensaje_inicial, HANDSHAKE,sizeof(mensaje_inicial));
	t_datosEnviar* recepcionHandshake;
	common_send(pcpSocket, paquete2, NULL);
	recepcionHandshake = common_receive(pcpSocket,NULL);
	if (recepcionHandshake->codigo_Operacion != HANDSHAKE) {
		log_debug(logger, "Fallo entrega Quantum, retardo");
	} else {
		log_debug(logger, "Handshake con Kernel exitoso");
	}
	uint32_t quantum_asignado;
	t_datosEnviar* recepcion_PCB;

	memcpy(&quantum_asignado, recepcionHandshake->data, sizeof(uint32_t));
	memcpy(&retardo_asignado, recepcionHandshake->data + sizeof(uint32_t),
			sizeof(uint32_t));
	log_debug(logger, "Recibi Quantum%i y Retardo%i",quantum_asignado,retardo_asignado);
	destruirPaquete(recepcionHandshake);
	destruirPaquete(paquete2);

	uint32_t quantum_aux;
	while (1) {
		currentPCB = conitos_malloc(sizeof(t_pcb));
		quantum_aux = quantum_asignado;
		terminar_proceso = false;
		recepcion_PCB = common_receive(pcpSocket, NULL);
		if (recepcion_PCB->codigo_Operacion != PCP_CPU_PROGRAM_TO_EXECUTE) {
			log_debug(logger, "Fallo entrega PCB");
		}
		log_debug(logger, "Recibi PCB");
		*currentPCB = *(t_pcb*) recepcion_PCB->data;
		destruirPaquete(recepcion_PCB);
		avisar_proceso_activo();
		log_debug(logger, "Aviso a la UMV de proceso activo");
		recreate_variable_dictionary();
		traer_segmento_etiquetas();
		printf("Empezo la ejecución");
		while (quantum_aux > 0 && !terminar_proceso) {
			quantum_aux = quantum_aux - 1;
			ejecutarInstruccion();
		}
		puts("TERMINE EJECUTAR");
		if (terminar_proceso && desconexion) {
			log_debug(logger,"Termino CPU, chau");
			log_destroy(logger);
			terminarCPU();
		} else {
			if (!terminar_proceso) {
				printf("Finalice Quantum");
				terminarProceso(CPU_PCP_FIN_QUANTUM,NULL,0);
				log_debug(logger,
						"Termine el quantum,devolví el PCB, quedo disponible para otro programa");
			}
		}
	}
}

void ejecutarInstruccion() {

	uint32_t offset_codeIndex = currentPCB->programCounter * sizeof(t_intructions);
	t_intructions* infoCodeIndex = (t_intructions*) solicitar_algo_UMV(currentPCB->codeIndex,offset_codeIndex,sizeof(t_intructions));
	log_debug(logger, "Busco instrucción en la UMV");
	char* instruccionReal = (char *) solicitar_algo_UMV(currentPCB->codeSegment,infoCodeIndex->start,infoCodeIndex->offset);
	log_debug(logger,"Encontre la instruccion: \n %s \n",instruccionReal);
 	analizadorLinea(instruccionReal, &funciones, &funciones_kernel);
	(currentPCB->programCounter)++;
	free(instruccionReal);
	free(infoCodeIndex);
	usleep(retardo_asignado * 1000);
}


void avisar_proceso_activo() {
	t_datosEnviar* paquete = pedirPaquete(&(currentPCB->pid), UMV_PROCESO_ACTIVO_CPU,
			sizeof(uint32_t));
	common_send(umvSocket, paquete, NULL);
	destruirPaquete(paquete);
	paquete = common_receive(umvSocket, NULL);
	if (paquete->codigo_Operacion != UMV_PROCESO_ACTIVO_CPU) {
		log_debug(logger, "No se pudo avisar el proceso activo");
	}
	destruirPaquete(paquete);
}


void terminarCPU() {
	printf("Me desconecto, chau");
	t_datosEnviar* pcbAEnviar = pedirPaquete(currentPCB, DESCONECTARSE,
			sizeof(t_pcb));
	t_datosEnviar* info = pedirPaquete(NULL, DESCONECTARSE, 0);
	common_send(umvSocket, info, NULL);
	common_send(pcpSocket, pcbAEnviar, NULL);
	close(umvSocket);
	close(pcpSocket);
	liberar_estructuras();
	config_destroy(configuracion_CPU);
	destruirPaquete(pcbAEnviar);
	destruirPaquete(info);
	exit(0);
}



void traer_segmento_etiquetas() {
	char* aux;
	printf("\n%d\n",currentPCB->tamanioIndiceEtiquetas);
	if (currentPCB->tamanioIndiceEtiquetas != 0) {
		aux = solicitar_algo_UMV(currentPCB->indiceEtiquetas, 0,
				currentPCB->tamanioIndiceEtiquetas);
		diccionarioEtiquetas = malloc(currentPCB->tamanioIndiceEtiquetas);
		memcpy(diccionarioEtiquetas, aux, currentPCB->tamanioIndiceEtiquetas);
		free(aux);
	}
}

void *hot_plug(void* sinUsar) {
	sem_wait(&sem_hotPlug);
	if (proceso_activo) {
		desconexion = true;
		log_debug(logger, "Me desconecto por SIGUSR1, me voy");
	} else {
		printf("Me tiraron la señal");
		t_datosEnviar* info = pedirPaquete("D", DESCONECTARSE, 2);
		common_send(umvSocket, info, NULL);
		common_send(pcpSocket, info, NULL);
		close(pcpSocket);
		close(umvSocket);
		sem_post(&sem_hotPlug);
		exit(EXIT_SUCCESS);
	}
	return NULL;
}

void desconectarme() {
	sem_post(&sem_hotPlug);
}
