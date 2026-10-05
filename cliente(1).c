#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PUERTO 2026
#define MSG_SIZE 250

int main( int argc, char *argv[]){
    int sd, fin = 0;
    struct sockaddr_in sockname;
    char buffersize[MSG_SIZE];
    fd_set readfds, auxfds;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == 1) {perror("No se puede abrir el socket cliente"); exit(1);}

    sockname.sin_family = AF_INET;
    sockname.sin_port = htons(PUERTO);
    sockname.sin_addr.s_addr = inet_addr(argc > 1 ? argv[1] : "127.0.0.1");

    if (connect(sd, (struct sockaddr *)&sockname, sizeof(sockname)) == -1) {
        perror("Error de conexion"); exit(1);
    }

    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    FD_SET(sd, &readfds);

    do {
        auxfds = readfds;
        select(sd + 1, &auxfds, NULL, NULL, NULL);
        
        if(FD_ISSET(sd, &auxfds)) {
            bzero(buffer, sizeof(buffer));
            int n = recv(sd, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) { 
                printf("Conexion cerrada por el servidor \n"); 
                break;
            }
            printf("%s", buffer);
            if (strstr(buffer, "Demasiados Clientes") || strstr(buffer, "Desconexion servidor")) {
                fin = 1;
            }           
        }
        if (FD_ISSET(0, &auxfds)) {
            bzero(buffer, sizeof(buffer));
            if (!fgets(buffer, sizeof(buffer), stdin)) { 
                strcpy(buffer, "SALIR\n"); 
            }
            if (strcmp(buffer, "SALIR\n") == 0){
            fin = 1;
            }
            send(sd, buffer, strlen(buffer), 0);
        }
    } while (!fin);

    close(sd);
    return 0;
}