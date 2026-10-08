#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
 //definimos puerto y tamaño del buffer de mensajes
#define PUERTO 2026
#define MSG_SIZE 250

int main( int argc, char *argv[]){
    int sd, fin = 0; //definimos el socket del cliente, fin es un interruptor, minetras que sea 0 el programa seguira funcionando
    struct sockaddr_in sockname;
    char buffer[MSG_SIZE]; //creamos el buffer 
    fd_set readfds, auxfds; //creamos los descriptores, que sirven para leer la entrada de teclado y el socket

    sd = socket(AF_INET, SOCK_STREAM, 0); //definimos el socket paea el uso del protocolo TCP
    if (sd == 1) {
        perror("No se puede abrir el socket cliente"); 
        exit(1);
    }
    //definimos los parametros de conexion
    sockname.sin_family = AF_INET;
    sockname.sin_port = htons(PUERTO);
    sockname.sin_addr.s_addr = inet_addr(argc > 1 ? argv[1] : "127.0.0.1");

    if (connect(sd, (struct sockaddr *)&sockname, sizeof(sockname)) == -1) { //realizamos un checkeo de la conexion
        perror("Error de conexion"); exit(1);
    }

    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    FD_SET(sd, &readfds);

    do {
        auxfds = readfds;
        select(sd + 1, &auxfds, NULL, NULL, NULL); //usamos select para que el socket lea el teclado y la entrada de conexion
        
        if(FD_ISSET(sd, &auxfds)) {
            bzero(buffer, sizeof(buffer)); //limpiamos el buffer
            int n = recv(sd, buffer, sizeof(buffer) - 1, 0); //recive la info del servidor y la guarda en el buffer
            if (n <= 0) { 
                printf("Conexion cerrada por el servidor \n"); 
                break;
            }
            printf("%s", buffer); //printeamos en pantalla el mensaje del servidor
            if (strstr(buffer, "Demasiados Clientes") || strstr(buffer, "Desconexion servidor")) { //si se detectan cualquiera de estos mensajes se cierra la conexion
                fin = 1;
            }           
        }
        if (FD_ISSET(0, &auxfds)) {
            bzero(buffer, sizeof(buffer));
            if (!fgets(buffer, sizeof(buffer), stdin)) { //leemos la entrada de teclado y la almacenamos en el buffer
                strcpy(buffer, "SALIR\n"); 
            }
            if (strcmp(buffer, "SALIR\n") == 0){ //si se teclea salir se cierra la conexion con el servidor
                fin = 1;
            }
            send(sd, buffer, strlen(buffer), 0);
        }
    } while (!fin);

    close(sd); //cerramos el socket
    return 0;
}