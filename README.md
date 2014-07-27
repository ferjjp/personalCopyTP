<h1>tp-2014-1c-conitos-team</h1>


Nuevas librerias!

(Cada vez que se actualicen hay que hacer esto)

Como se compilan:
<ul>
<li>Entran en la carpeta git/tp-2014-1c-conitos-team/conitos_library
<li>Tocan f4, y en esa consola que se les abrió escriben "sudo chmod u+x instalacion.sh" y luego ./instalacion.sh



La biblioteca se incorpora como #include conitos_library en su archivo de C.

La interfaz es la siguiente:

<ul>
<li>int escucharEn(char * puerto)
<li>int recibirEncabezado(int socketCliente, int* codOp, int* tamanioMensaje);)
<li>int recibirContenido(int socketCliente,int tamanioMensaje,char* contenido);
<li>int conectarA(char ip, char puerto)
<li>int enviarA(int serverSocket, int codigoOperacion, char* contenido)


Ejemplo de llamadas (newSurgence y newSurgenceServer) :

<h2>CLIENTE</h2>

                #include <conitos_library/sockets.h>

                int main(void) {
                                setvbuf(stdout, NULL, _IONBF, 0);
                                char * mensaje = "hola";
                                int serverSocket, status;
                                serverSocket = conectarA("localhost", "6666");
                                status = enviarA(serverSocket, 2, mensaje);
                                if (status == 8 + strlen(mensaje) + 1) {
                                       puts("\nEnvio Exitoso");
                                } else {
                                       puts("\nEnvio Fallido: ");
                                       printf("\nEnviado %d, esperado %d", status, 8 + strlen(mensaje) + 1);
                                }
                }                                            
<h2>SERVER</h2>

                #include <conitos_library/sockets.h>

                int main(void) {
                    setvbuf(stdout, NULL, _IONBF, 0);
                    int socketEscucha;
                    socketEscucha = escucharEn("6666");
                    while (1) {
                                        listen(socketEscucha, 1000);
                                        struct sockaddr_in addr;
                                        socklen_t addrlen = sizeof(addr);
                                        int socketCliente;
                                        socketCliente = accept(socketEscucha, (struct sockaddr *) &addr,&addrlen);
                                        int status, codigoOperacion, tamanioMensaje;
                                        status = recibirEncabezado(socketCliente, &codigoOperacion,&tamanioMensaje);
                                        char* mensaje = malloc(tamanioMensaje);
                                        status = recibirContenido(socketCliente, tamanioMensaje, mensaje);
                                        printf("\n%d\n", codigoOperacion);
                                        printf("\n%d\n\n", tamanioMensaje);
                                        puts(mensaje);
                    }
                }
