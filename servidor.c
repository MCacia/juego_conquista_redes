#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PUERTO 2026
#define MSG_SIZE 100
#define MAX_CLIENTES 10

int main(){

    int sd, new_sd; // sd: descriptor del socket de servidor
                    // new_sd: descriptor de los sockets que se crean cada vez que accept acepta a un cliente
    struct sockaddr_in sockname, from; // estructura con info del socket de servidor (sockname) y cliente (from)
    socklen_t from_len; // indicamos el tamanio de la estructura from para bind
    int salir = 0;
    char buffer[MSG_SIZE];
    fd_set readfds, auxfds;
    int clientes[MAX_CLIENTES];
    int numClientes = 0;
    int i, j, recibidos;


    sd = socket (AF_INET, SOCK_STREAM, 0);
    // Definimos AF_INET para el tipo de direcciones IPv4
    // Definimos SOCK_STREAM para TCP, si fuera UDP usariamos SOCK_DGRAM

    if (sd == -1){
        perror("Error al abrir el socket de servidor \n");
        exit(1);
    }

    // definimos la informacion de la estructura del servidor para poder usar bind
    sockname.sin_family = AF_INET; // familia de direcciones
    sockname.sin_port = htons(PUERTO); // puerto
    sockname.sin_addr.s_addr = INADDR_ANY; // conexiones que se aceptan (INADOR_ANY: escucha de cualquier interfaz de red de la maquina)

    if (bind(sd, (struct sockaddr *)&sockname, sizeof(sockname)) == -1){
    // bind solo acepta formato sockaddr asi que lo casteamos
        perror("Error en la operacion bind");
        exit(1);
    }

    // definimos tamanio de from
    from_len = sizeof(from);

    // marca el socket de servidor como pasivo, a la escucha de nuevas conexiones
    // el MAX_CLIENTES es la cola de espera ( cuantas conexiones pueden estar en espera )
    if (listen(sd, MAX_CLIENTES) == -1){
        perror("Error en la operacion listen \n");
        exit(1);
    }


    // Inicializamos el conjunto de sockets a vigilar (vacia el conjunto readfds con 0)
    FD_ZERO(&readfds);
    // hacemos un push en el conjunto de sd
    FD_SET(sd, &readfds);

    // bucle para aceptar peticiones y escuchar a los sockets ya aceptados
    while(1) {
        // como select modifica el conjunto que le pasamos, necesitamos un auxiliar para no borrar la lista de los sockets que queremos vigilar
        auxfds = readfds;

        if (select(FD_SETSIZE, &auxfds, NULL, NULL, NULL) == -1){
            perror("Error en select");
            exit(1);
        }

        // como select puede haber marcado mas de un socket con actividad, recorremos todos aquellos que haya marcado
        for (i = 0; i < FD_SETSIZE; i++){
            // con FD_ISSET preguntamos si el socket que hemos recorrido en el conjunto es uno con actividad
            if (FD_ISSET(i, &auxfds)){
                if (i == sd){
                    // caso 1, si el socket con actividad es el de escucha significa que alguien nuevo quiere conectarse
                    new_sd = accept(sd, (struct sockaddr *)&from, &from_len);
                    if (new_sd == -1){
                        perror("Error aceptando peticiones");
                        continue;
                    }

                    if (numClientes < MAX_CLIENTES){
                        clientes[numClientes] = new_sd;
                        numClientes++;
                        FD_SET(new_sd, &readfds); // a partir de ahora vigilamos new_sd tambien

                        printf("Cliente conectado desde %s:%d (socket %d)\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), new_sd);
                    } else {
                        bzero(buffer, sizeof(buffer));
                        strcpy(buffer, "Demasiados Clientes\n");
                        send(new_sd, buffer, strlen(buffer), 0);
                        close(new_sd);
                    }
                } else {
                    // caso 2, la actividad es de un socket diferente al de escucha, entonces querra escribir o que se ha desconectado
                    bzero(buffer, sizeof(buffer));
                    recibidos = recv(i, buffer, sizeof(buffer) - 1, 0);
                    if (recibidos > 0){
                        buffer[strcspn(buffer, "\n")] = '\0';
                        printf("El mensaje recibido del socket %d fue: %s\n", i, buffer);
                        bzero(buffer, sizeof(buffer));
                        strcpy(buffer, "Ok. Mensaje recibido");
                        send(i, buffer, strlen(buffer), 0);
                    } else {
                        // recibidos == 0 -> cliente cerro la conexion
                        // recibidos == -1 -> error en la lectura
                        printf("El cliente del socket %d se ha desconectado\n", i);
                        close(i);
                        FD_CLR(i, &readfds);

                        // buscamos donde esta el cliente que se ha desconectado
                        for (j = 0; j < numClientes; j++){
                            if (clientes[j] == i) break;
                        }
                        // borramos sobreescribiendo los sockets uno a uno, pasando todos una posicion a la izquierda desde el que se ha desconectado
                        for (; j< numClientes - 1; j++){
                            clientes[j] = clientes[j + 1];
                        }
                        numClientes--;
                    }
                }
            }
        }
    }

    close(sd);
    return 0;

}
    





















