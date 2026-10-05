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

int main(){

    int sd, new_sd; // sd: descriptor del socket de servidor
                    // new_sd: descriptor de los sockets que se crean cada vez que accept acepta a un cliente
    struct sockaddr_in sockname, from; // estructura con info del socket de servidor (sockname) y cliente (from)
    socklen_t from_len; // indicamos el tamanio de la estructura from para bind
    int salir = 0;
    char buffer[100];


    sd = socket (AF_INET, SOCK_STREAM, 0);
    // Definimos AF_INET para el tipo de direcciones IPv4
    // Definimos SOCK_STREAM para TCP, si fuera UDP usariamos SOCK_DGRAM

    if (sd == -1){
        perror("Error al abrir el socket de servidor \n");
        exit(1);
    }

    // definimos la informacion de la estructura del servidor para poder usar bind
    sockname.sin_family = AF_INET; // familia de direcciones
    sockname.sin_port = htons(2026); // puerto
    sockname.sin_addr.s_addr = INADDR_ANY; // conexiones que se aceptan (INADOR_ANY: escucha de cualquier interfaz de red de la maquina)

    if (bind(sd, (struct sockaddr *)&sockname, sizeof(sockname)) == -1){
    // bind solo acepta formato sockaddr asi que lo casteamos
        perror("Error en la operacion bind");
        exit(1);
    }

    // definimos tamanio de from
    from_len = sizeof(from);

    // marca el socket de servidor como pasivo, a la escucha de nuevas conexiones
    // el 1 es la cola de espera ( cuantas conexiones pueden estar en espera )
    if (listen(sd, 1) == -1){
        perror("Error en la operacion listen \n");
        exit(1);
    }

    // bucle para aceptar peticiones
    while (1){
        if ((new_sd = accept(sd, (struct sockaddr *)&from, &from_len)) == -1){
            perror("Error aceptando peticiones \n");
            exit(1);
        }

        printf("Cliente conectado desde %s:%d\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));

        do {
            salir = 0;
            // pone el buffer a 0 antes del siguiente mensaje, asi no hay basura en el siguiente
            bzero(buffer, sizeof(buffer));
            if (recv(new_sd, buffer, sizeof(buffer), 0) == -1){
                perror("Error en la operacion de recv");
                exit(1);
            }

            if (strcmp(buffer, "FIN") == 0){
                salir = 1;
            }

            printf("El mensaje recibido fue: %s\n", buffer);

            // Enviar respuesta al cliente
            bzero(buffer, sizeof(buffer));
            strcpy(buffer, "Ok. Mensaje recibido");
            if (send(new_sd, buffer, sizeof(buffer), 0) == -1){
                perror("Error en la operacion de send");
                exit(1);
            }

        } while (!salir);

        close(new_sd);

    }

    close(sd);
    return 0;

}
    





















