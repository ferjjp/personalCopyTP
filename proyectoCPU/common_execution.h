/*
 * common_execution.h
 *
 *  Created on: Jul 8, 2014
 *      Author: root
 */

#ifndef COMMON_EXECUTION_H_
#include <parser/parser.h>
#include "stack.h"
#include <conitos-estaCoverflow/conitos_protocol.h>
#include "globals.h"
#include "estructuras.h"
#define COMMON_EXECUTION_H_

void terminarProceso(int codigoDeOperacion,void* datosExtra, t_size sizeExtra);
void ejecutarSiSePerdioConexion();
void ejecutarSiHuboSegmentationFault();
void clean_variable_dictionary();
void recreate_variable_dictionary();
void ejecutarSiHuboStackOverflow();
void ejecutarSiHuboErrorEnUnaSyscall();
void noHacerNada();
void* solicitar_algo_UMV(t_puntero,int32_t,uint32_t);
void llenar_diccionario();
void liberar_estructuras();
char* charToString(char element);

extern int pcpSocket;
extern int umvSocket;



#endif /* COMMON_EXECUTION_H_ */
