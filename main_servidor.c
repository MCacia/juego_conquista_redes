#include "servidor.h"




int main(){

    int fd, new_sd; // fd: descriptor del socket de servidor
                    // new_sd: descriptor de los sockets que se crean cada vez que accept acepta a un cliente
    struct sockaddr_in sockname, from; // estructura con info del socket de servidor (sockname) y cliente (from)
    socklen_t from_len; // indicamos el tamanio de la estructura from para bind
    char buffer[MSG_SIZE];
    fd_set auxfds;
    int i, j, recibidos;
    int on = 1;
    srand(time(NULL));     // iicializamos la semilla para que las casillas de los jugadores sean aleatorias en cada partida

    fd = socket (AF_INET, SOCK_STREAM, 0);
    // Definimos AF_INET para el tipo de direcciones IPv4
    // Definimos SOCK_STREAM para TCP, si fuera UDP usariamos SOCK_DGRAM

    if (fd == -1){
        perror("Error al abrir el socket de servidor \n");
        exit(1);
    }

    // permite relanzar el servidor sin esperar a que se libere el puerto
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // definimos la informacion de la estructura del servidor para poder usar bind
    sockname.sin_family = AF_INET; // familia de direcciones
    sockname.sin_port = htons(PUERTO); // puerto
    sockname.sin_addr.s_addr = INADDR_ANY; // conexiones que se aceptan (INADOR_ANY: escucha de cualquier interfaz de red de la maquina)

    if (bind(fd, (struct sockaddr *)&sockname, sizeof(sockname)) == -1){
    // bind solo acepta formato sockaddr asi que lo casteamos
        perror("Error en la operacion bind");
        exit(1);
    }

    // definimos tamanio de from
    from_len = sizeof(from);

    // marca el socket de servidor como pasivo, a la escucha de nuevas conexiones
    // el MAX_CLIENTES es la cola de espera ( cuantas conexiones pueden estar en espera )
    if (listen(fd, MAX_CLIENTES) == -1){
        perror("Error en la operacion listen \n");
        exit(1);
    }


    // Inicializamos el conjunto de sockets a vigilar (vacia el conjunto readfds con 0)
    FD_ZERO(&readfds);
    // hacemos un push en el conjunto de fd
    FD_SET(fd, &readfds);

    printf("Servidor iniciado en el puerto %d\n", PUERTO);

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
                if (i == fd){
                    // caso 1, si el socket con actividad es el de escucha significa que alguien nuevo quiere conectarse
                    new_sd = accept(fd, (struct sockaddr *)&from, &from_len);
                    if (new_sd == -1){
                        perror("Error aceptando peticiones");
                        continue;
                    }

                    if (numClientes < MAX_CLIENTES){
                        clientes[numClientes].fd = new_sd;
                        clientes[numClientes].estado = 0; // no logueado
                        clientes[numClientes].partida = -1;
                        clientes[numClientes].jugador = -1;
                        // rellenamos con 0 el espacio para el nombre del cliente, todavia no lo sabemos
                        bzero(clientes[numClientes].nombre, sizeof(clientes[numClientes].nombre));
                        numClientes++;
                        FD_SET(new_sd, &readfds); // a partir de ahora vigilamos new_sd tambien

                        printf("Cliente conectado desde %s:%d (socket %d)\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), new_sd);
                        enviar(new_sd, "+Ok. Usuario conectado");
                        cmdAyudaInicio(new_sd);
                    } else {
                        enviar(new_sd, "-Err. Demasiados Clientes");
                        sleep(1); // sleep para evitar condiciones de carrera (damos tiempo a enviar el mensaje antes de cerrar el socket)
                        close(new_sd);
                    }

                } else {
                    // caso 2, la actividad es de un socket diferente al de escucha, entonces querra escribir o que se ha desconectado
                    bzero(buffer, sizeof(buffer));
                    recibidos = recv(i, buffer, sizeof(buffer) - 1, 0);
                    
                    // buscamos que indice de 'clientes' corresponde con este socket 'i'
                    int idx = -1;
                    for (j = 0; j < numClientes; j++){
                        if (clientes[j].fd == i){ idx = j; break;}
                    }

                    if (recibidos > 0 && idx != -1){
                        buffer[strcspn(buffer, "\r\n")] = '\0';

                        if (strlen(buffer) > 200) {
                            enviar(i, "-Err. Mensaje demasiado largo (maximo 200 caracteres)");
                            continue;
                        }

                        printf("Socket %d envia: %s\n", i, buffer);
                        procesarMensaje(&clientes[idx], buffer);
                    } else {
                        // recibidos == 0 -> cliente cerro la conexion
                        // recibidos == -1 -> error en la lectura
                        eliminarCliente(i);
                    }
                }
            }
        }
    }

    close(fd);
    return 0;

}