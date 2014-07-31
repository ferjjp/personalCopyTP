#include "plp.h"
void *
threadPLP(void* vacio)
{
  int PIDActual = 0;
  setvbuf(stdout, NULL, _IONBF, 0);
  puts("[KERNEL] - [PLP] - ESCUCHANDO\n");

  pthread_t hilo_PLP_destruccion;
  pthread_t hilo_PLP_multiprogramacion;

  pthread_create(&hilo_PLP_destruccion, NULL, hiloDestruccion, NULL );
  pthread_create(&hilo_PLP_multiprogramacion, NULL, hiloMultiprogramacion,
      NULL );

  int socketEscucha;
  socketEscucha = setup_listen(configuration.puertoProg);
  while (1)
    {
      setvbuf(stdout, NULL, _IONBF, 0);
      t_datosEnviar * paquete;
      listen(socketEscucha, 1000);
      struct sockaddr_in addr;
      socklen_t addrlen = sizeof(addr);
      int socketCliente;
      socketCliente = accept(socketEscucha, (struct sockaddr *) &addr,
          &addrlen);


      paquete = common_receive(socketCliente, NULL );
      atenderProgramaEntrante(paquete, socketCliente, &PIDActual);

    }
  pthread_join(hilo_PLP_destruccion, NULL );
  pthread_join(hilo_PLP_multiprogramacion, NULL );
  return NULL ;

}

void
encolarPCB(t_pcb * pcbNuevo, int peso, int socketCliente)
{
  t_nodo_proceso * nodoNuevo = malloc(sizeof(t_nodo_proceso));
  nodoNuevo->pcb = *pcbNuevo;
  nodoNuevo->peso = peso;
  nodoNuevo->soquet_prog = socketCliente;
  pthread_mutex_lock(&mutex_listaNuevos);
  list_add(listaNuevos, (void*) nodoNuevo);
  list_sort(listaNuevos, algoritmoSJF);
  pthread_mutex_unlock(&mutex_listaNuevos);
  sem_post(&sem_listaNuevos);

}

int
generarPid(int* PID)
{
  *PID = *PID + 1;
  return *PID;
}

bool
algoritmoSJF(void * nodo1, void * nodo2)
{
  return (((t_nodo_proceso *) nodo1)->peso < ((t_nodo_proceso *) nodo2)->peso);
}

void*
hiloDestruccion()
{
  t_nodo_proceso * nodoARemover;
  while (1)
    {
      sem_wait(&sem_listaTerminados);
      pthread_mutex_lock(&mutex_listaTerminados);
      nodoARemover = queue_pop(listaTerminados);
      pthread_mutex_unlock(&mutex_listaTerminados);
      int pidARemover = nodoARemover->pcb.pid;
      debugTrackPLP("[Achievement] Removing process %d from the system.",pidARemover);
      debugTrackPLP("Requesting segment destroy...");
      t_datosEnviar * paquete = pedirPaquete((void*) &pidARemover,
          KERNEL_DESTRUIR_SEGMENTOS_PROGRAMA, sizeof(int));
      common_send(socketUMV, paquete, NULL );
      destruirPaquete(paquete);
      paquete = pedirPaquete("T", PROGRAMA_CERRAR, 2);
      common_send(nodoARemover->soquet_prog, paquete, NULL );
      destruirPaquete(paquete);
      free(nodoARemover);
    }
  return NULL ;

}

void*
hiloMultiprogramacion(void* vacio)
{

  t_nodo_proceso* nodoAListo;
  while (1)
    {

      sem_wait(&sem_multiprog);
      sem_wait(&sem_listaNuevos);
      pthread_mutex_lock(&mutex_listaNuevos);
      nodoAListo = list_remove(listaNuevos, 0);
      pthread_mutex_unlock(&mutex_listaNuevos);
      debugTrackPLP("[Achievement] Moving process %d to ready queue.",nodoAListo->pcb.pid);
      pthread_mutex_lock(&mutex_listaListos);
      queue_push(listaListos, nodoAListo);
      pthread_mutex_unlock(&mutex_listaListos);
      sem_post(&sem_listaListos);
      listarNuevos();
    }
  return NULL ;

}

void
atenderProgramaEntrante(t_datosEnviar* paquete, int socketCliente,
    int * PIDActual)
{
  if (paquete->codigo_Operacion == HANDSHAKE)
    {
      destruirPaquete(paquete);
      t_datosEnviar *paqueteEscribir;
      void *dataGenerica;
      paquete = pedirPaquete("K", PLP_PROGRAMA_CONFIRMAR_CONECCION, 2);
      common_send(socketCliente, paquete, NULL );
      destruirPaquete(paquete);
      paquete = common_receive(socketCliente, NULL );
      if (paquete->codigo_Operacion == ENVIAR_SCRIPT)
        {

          debugTrackPLP(
              "[Achievement] Programa autenticado, recibiendo script");
          char* programa = malloc(paquete->data_size);
          memcpy(programa, paquete->data, paquete->data_size);

          t_metadata_program * metadataDelPrograma;
          metadataDelPrograma = metadata_desde_literal((const char*) programa);
          int peso;
          peso = metadataDelPrograma->cantidad_de_funciones * 3
              + metadataDelPrograma->cantidad_de_etiquetas * 5
              + metadataDelPrograma->instrucciones_size;

          int error = 0;
          t_pcb * pcbNuevo = malloc(sizeof(t_pcb));

          pcbNuevo->pid = generarPid(PIDActual);

          int direccionStack, direccionCodigo, direccionIndiceEtiquetas,
              direccionIndiceCodigo;

          destruirPaquete(paquete);
          error = 0;

          debugTrackPLP("[Accion] Pidiendo segmentos en a la UMV");

          direccionCodigo = pedirSegmento(&error, strlen(programa) + 1,
              pcbNuevo->pid);
          direccionStack = pedirSegmento(&error, configuration.tamanio_pila,
              pcbNuevo->pid);
          direccionIndiceEtiquetas = pedirSegmento(&error,
              metadataDelPrograma->etiquetas_size, pcbNuevo->pid);
          direccionIndiceCodigo = pedirSegmento(&error,
              metadataDelPrograma->instrucciones_size * sizeof(t_intructions),
              pcbNuevo->pid);

          if (error == 1)
            {
              debugTrackPLP(
                  "[Error] No se pudo crear la totalidad de segmentos");
              paquete = pedirPaquete("A", PLP_PROGRAMA_NO_SE_PUDO_CREAR, 2);
              common_send(socketCliente, paquete, NULL );
              destruirPaquete(paquete);
              void * pidMomentaneo = malloc(sizeof(int));
              memcpy(pidMomentaneo, &(pcbNuevo->pid), sizeof(int));
              paquete = pedirPaquete(pidMomentaneo,
                  KERNEL_DESTRUIR_SEGMENTOS_PROGRAMA, sizeof(int));
              common_send(socketUMV, paquete, NULL );
              debugTrackPLP(
                  "[Accion] Eliminando el resto de los segmentos pedidos");
              PIDActual--;
              destruirPaquete(paquete);
              free(pidMomentaneo);
            }
          else
            {
              debugTrackPLP(
                  "[PID:%d]-[Achievement] Se pudo alocar todos los segmentos para el proceso ",
                  pcbNuevo->pid);
              pcbNuevo->codeIndex = direccionIndiceCodigo;
              pcbNuevo->codeSegment = direccionCodigo;
              pcbNuevo->indiceEtiquetas = direccionIndiceEtiquetas;
              pcbNuevo->programCounter =
                  metadataDelPrograma->instruccion_inicio;
              pcbNuevo->tamanioIndiceEtiquetas =
                  metadataDelPrograma->etiquetas_size;
              pcbNuevo->currentStack.base = direccionStack;
              pcbNuevo->currentStack.contextSize = 0;
              pcbNuevo->currentStack.size = configuration.tamanio_pila;
              pcbNuevo->currentStack.stack_pointer = direccionStack;

              if (pcbNuevo->codeSegment != -1)
                {
                  debugTrackPLP(
                      "[PID:%d]-[Achievement] Alocado segmento de codigo en la direccion virtual %d",
                      pcbNuevo->pid, pcbNuevo->codeSegment);

                  dataGenerica = malloc(
                      3 * sizeof(uint32_t) + strlen(programa) + 1);
                  int tamanioData = strlen(programa) + 1;

                  memcpy(dataGenerica, &(pcbNuevo->pid), sizeof(uint32_t));
                  memcpy(dataGenerica + sizeof(uint32_t), &tamanioData,
                      sizeof(uint32_t));
                  memcpy(dataGenerica + 2 * sizeof(uint32_t),
                      &(pcbNuevo->codeSegment), sizeof(uint32_t));
                  memcpy(dataGenerica + 3 * sizeof(uint32_t), programa,
                      tamanioData);

                  paqueteEscribir = pedirPaquete(dataGenerica,
                      KERNEL_ESCRIBIR_SEGMENTO_UMV,
                      3 * sizeof(uint32_t) + strlen(programa) + 1);
                  common_send(socketUMV, paqueteEscribir, NULL );
                  destruirPaquete(paqueteEscribir);
                  paqueteEscribir = common_receive(socketUMV, NULL );
                  free(dataGenerica);
                  destruirPaquete(paqueteEscribir);

                }
              if (pcbNuevo->codeIndex != -1)
                {
                  debugTrackPLP(
                      "[PID:%d]-[Achievement] Alocado segmento de indice de codigo en la direccion virtual %d",
                      pcbNuevo->pid, pcbNuevo->codeIndex);
                  int tamanioData = metadataDelPrograma->instrucciones_size
                      * sizeof(t_intructions);
                  paqueteEscribir = pedirPaquete(&(pcbNuevo->pid),
                      KERNEL_ESCRIBIR_SEGMENTO_UMV, sizeof(int));
                  aniadirAlPaquete(paqueteEscribir, &(tamanioData),
                      sizeof(int));
                  aniadirAlPaquete(paqueteEscribir, &(pcbNuevo->codeIndex),
                      sizeof(int));
                  aniadirAlPaquete(paqueteEscribir,
                      metadataDelPrograma->instrucciones_serializado,
                      tamanioData);
                  common_send(socketUMV, paqueteEscribir, NULL );
                  destruirPaquete(paqueteEscribir);
                  paqueteEscribir = common_receive(socketUMV, NULL );
                  destruirPaquete(paqueteEscribir);

                }
              if (pcbNuevo->indiceEtiquetas != -1)
                {

                  debugTrackPLP(
                      "[PID:%d]-[Achievement] Alocado segmento de indice de etiquetas en la direccion virtual %d",
                      pcbNuevo->pid, pcbNuevo->indiceEtiquetas);

                  int tamanioData = metadataDelPrograma->etiquetas_size;
                  dataGenerica = malloc(
                      (tamanioData + 1) * sizeof(char) + 3 * sizeof(int));
                  memcpy(dataGenerica, &(pcbNuevo->pid), sizeof(int));
                  memcpy(dataGenerica + sizeof(int), &tamanioData, sizeof(int));
                  memcpy(dataGenerica + 2 * sizeof(int),
                      &(pcbNuevo->indiceEtiquetas), sizeof(int));
                  memcpy(dataGenerica + 3 * sizeof(int),
                      metadataDelPrograma->etiquetas, tamanioData);
                  paqueteEscribir = pedirPaquete(dataGenerica,
                      KERNEL_ESCRIBIR_SEGMENTO_UMV, (tamanioData + 1) * sizeof(char) + 3 * sizeof(int));
                  common_send(socketUMV, paqueteEscribir, NULL );
                  destruirPaquete(paqueteEscribir);
                  paqueteEscribir = common_receive(socketUMV, NULL );
                  destruirPaquete(paqueteEscribir);
                  free(dataGenerica);

                }

              debugTrackPLP("[PID:%d]-[Achievement] PCB listo para encolarse",
                  pcbNuevo->pid);

              paquete = pedirPaquete("A", PLP_PROGRAMA_ENCOLADO, 2);
              common_send(socketCliente, paquete, NULL );
              free(programa);
              encolarPCB(pcbNuevo, peso, socketCliente);

            }
        }

      else
        {
          destruirPaquete(paquete);
          pedirPaquete("NK", PROGRAMA_CERRAR, 3);
          puts("No Validado");
          common_send(socketCliente, paquete, NULL );
          destruirPaquete(paquete);
        }
    }

}

int
pedirSegmento(int * error, int tamanio, int pid)
{
  int tam = tamanio;

  t_datosEnviar * paquete;
  void * pedidoSegmentoUMV = malloc(3 * sizeof(int));
  memcpy(pedidoSegmentoUMV, &pid, sizeof(int));
  memcpy(pedidoSegmentoUMV + sizeof(int), &tam, sizeof(int));

  paquete = pedirPaquete(pedidoSegmentoUMV, KERNEL_PEDIR_SEGMENTO_UMV, //Pedir Segmento
      2 * sizeof(int));
  common_send(socketUMV, paquete, NULL );

  free(paquete->data);

  paquete = common_receive(socketUMV, NULL );

  if (paquete->codigo_Operacion == ERROR_NO_CREO_SEGMENTO)
    {
      *error = 1;
    }
  memcpy(&tam, paquete->data, sizeof(int));
  destruirPaquete(paquete);
  free(pedidoSegmentoUMV);
  return tam; //Guardado la base
}

