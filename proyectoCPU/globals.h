/* globals.h
 *
 *  Created on: Jul 9, 2014
 *      Author: root
 */

#ifndef GLOBALS_H_
#define GLOBALS_H_
#include <sys/socket.h>
#include <sys/types.h>
#include <parser/parser.h>
#include <stdbool.h>
#include "estructuras.h"
#include <commons/log.h>


/*
typedef uint32_t t_puntero;
typedef char t_nombre_variable;
typedef uint32_t t_size;
typedef t_nombre_variable* t_nombre_semaforo;
typedef t_nombre_variable* t_nombre_etiqueta;
typedef t_nombre_variable* t_nombre_compartida;
typedef t_nombre_variable* t_nombre_dispositivo;
typedef uint32_t t_valor_variable;
*/


extern	AnSISOP_funciones funciones;
extern	AnSISOP_kernel funciones_kernel;
extern	t_dictionary* diccionarioDeVariables;
extern	t_pcb* currentPCB;
extern	int pcpSocket;
extern	int umvSocket;
extern	char *informacion_diccvar;
extern	char *diccionarioEtiquetas;
extern bool terminar_proceso;
extern bool proceso_activo;
extern t_log *logger;




#endif /* GLOBALS_H_ */
