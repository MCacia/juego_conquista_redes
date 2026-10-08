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
#define MAX_CLIENTES 4

#define MAX_USER 32
#define FICHERO_USUARIOS "usuarios.txt"

typedef struct {
    int fd; // descriptor del socket del cliente
    char nombre[50]; // Nombre de usuario en proceso de login o validado
    int estado; // 0: no logueado(ST_CONECTADO), 1: Esperando password(ST_USUARIO_OK), 2: Logueado(ST_VALIDADO)
} Cliente;

// ------ ARREGLO -----
// arreglar codigo con enviar, algunas veces usamos enviar y otras veces lo hacemos a mano
// --------------------

void enviar(int fd, const char *msg)
{
    char buf[MSG_SIZE];
    snprintf(buf, sizeof(buf), "%s\n", msg);
    send(fd, buf, strlen(buf), 0);
}


int existe_usuario(const char *user)
{
    FILE *f = fopen(FICHERO_USUARIOS, "r"); // abrimos el fichero de usuarios
    char u[MAX_USER], p[MAX_USER];          // creamos las variables de usuario y contraseña
    if (!f)
        return 0; // comprobamos que se ha abierto el fichero correctamente
    while (fscanf(f, "%31s %31s", u, p) == 2)
        if (strcmp(u, user) == 0)
        { // comprobamos que el usuario existe comparando u(el archivo) con user(entrada de teclado)
            fclose(f);
            return 1; // si no es correcto devuelve 1
        } //
    fclose(f); // cerramos el archivo
    return 0;  // si es correcto devuelve 0
}

int passwordCorrecta(const char *user, const char *pass)
{
    FILE *f = fopen(FICHERO_USUARIOS, "r");
    char u[MAX_USER], p[MAX_USER];
    if (!f)
        return 0;
    while (fscanf(f, "%31s %31s", u, p) == 2)
        if (strcmp(u, user) == 0)
        {                                // verificamos que el usuario coincide
            fclose(f);                   // cerramos el archivo
            return strcmp(p, pass) == 0; // comprobamos que la contraseña del archivo coincida con la que le ha escrito el usuario
        }
    fclose(f);
    return 0;
}

void login(Cliente *c, char *cmd, char *arg)
{

    // Comprobamos el usuario
    if (strcmp(cmd, "USUARIO") == 0){
        if (c->estado != 0){
            enviar(c->fd, "-Err. Ya estas identificado");
            return;
        }
        if (arg[0] == '\0' || strchr(arg, ' ') || strlen(arg) >= MAX_USER){
            enviar(c->fd, "-Err. Formato: USUARIO usuario");
            return;
        }

        if (existe_usuario(arg)){
            strcpy(c->nombre, arg);   // guardamos quién es
            c->estado = 1; // ahora toca la contraseña
            enviar(c->fd, "+Ok. Usuario correcto");
        }
        else{
            enviar(c->fd, "-Err. Usuario incorrecto. Si no tienes cuenta usa: REGISTRO usuario password");
        }
    }

    // Comprobamos la contraseña
    else if (strcmp(cmd, "PASSWORD") == 0){
        if (c->estado != 1){
            enviar(c->fd, "-Err. Primero debes enviar USUARIO");
            return;
        }

        if (passwordCorrecta(c->nombre, arg)){
            c->estado = 2;
            enviar(c->fd, "+Ok. Usuario validado");
        }
        else{
            c->estado = 0; // vuelve a empezar
            c->nombre[0] = '\0';
            enviar(c->fd, "-Err. Error en la validacion");
        }
    }
}

int registrar_usuario(char *usuario, char *password) {
    FILE *f;
    if (existe_usuario(usuario)){
        return 0; //Si el usuario existe salimos
    }

    //Comprobamos que usuario y contrasenna sean validos
    if (usuario[0] == '\0' || strchr(usuario, ' ') != NULL || strlen(usuario) >= MAX_USER){ 
        return -1; 
    }
    if (password[0] == '\0' || strchr(password, ' ') != NULL || strlen(password) >= MAX_USER){ 
        return -1; 
    }

    // Abrimos el fichero en modo escritura
    f = fopen(FICHERO_USUARIOS, "a");

    if (f == NULL){
        return -1; 
    }

    // Guardamos usuario y contraseña
    fprintf(f, "%s %s\n", usuario, password);
    fclose(f);

    return 1; // Registro correcto
}


void procesarMensaje(Cliente *c, char *msg)
{
    char cmd[MSG_SIZE] = "", arg[MSG_SIZE] = "";
    char *sp = strchr(msg, ' ');

    if (sp)
    {
        *sp = '\0';
        strcpy(cmd, msg);
        strcpy(arg, sp + 1);
    }
    else
        strcpy(cmd, msg);

    if (strcmp(cmd, "USUARIO") == 0 || strcmp(cmd, "PASSWORD") == 0)
        login(c, cmd, arg);
    else
        enviar(c->fd, "-Err. Comando no reconocido");
}

int main(){

    int fd, new_sd; // fd: descriptor del socket de servidor
                    // new_sd: descriptor de los sockets que se crean cada vez que accept acepta a un cliente
    struct sockaddr_in sockname, from; // estructura con info del socket de servidor (sockname) y cliente (from)
    socklen_t from_len; // indicamos el tamanio de la estructura from para bind
    int salir = 0;
    char buffer[MSG_SIZE];
    fd_set readfds, auxfds;

    Cliente clientes[MAX_CLIENTES]; //borrar
    int numClientes = 0; //borrar
    int i, j, recibidos;


    fd = socket (AF_INET, SOCK_STREAM, 0);
    // Definimos AF_INET para el tipo de direcciones IPv4
    // Definimos SOCK_STREAM para TCP, si fuera UDP usariamos SOCK_DGRAM

    if (fd == -1){
        perror("Error al abrir el socket de servidor \n");
        exit(1);
    }

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
                        // rellenamos con 0 el espacio para el nombre del cliente, todavia no lo sabemos
                        bzero(clientes[numClientes].nombre, sizeof(clientes[numClientes].nombre));
                        numClientes++;
                        FD_SET(new_sd, &readfds); // a partir de ahora vigilamos new_sd tambien

                        printf("Cliente conectado desde %s:%d (socket %d)\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), new_sd);
                        send(new_sd, "+Ok. Usuario conectado\n", 23, 0);
                    } else {
                        bzero(buffer, sizeof(buffer));
                        strcpy(buffer, "-Err. Demasiados Clientes\n");
                        send(new_sd, buffer, strlen(buffer), 0);
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

                    if (recibidos > 0){
                        buffer[strcspn(buffer, "\n")] = '\0';
                        printf("Socket %d envia: %s\n", i, buffer);

                        char comando[20], arg1[50], arg2[50];
                        int num_args = sscanf(buffer, "%s %s %s", comando, arg1, arg2);

                        // registro
                        if (strncmp(comando, "REGISTRO", 8) == 0){
                            if (num_args == 3){
                                int res = registrar_usuario(arg1, arg2);
                                if (res == 1){
                                    char *msg = "+Ok. Registro exitoso\n";
                                    send(i, msg, strlen(msg), 0);
                                } else if (res == 0) {
                                    char *msg = "-Err. Usuario existente\n";
                                    send(i, msg, strlen(msg), 0);
                                } else {
                                    char *msg = "-Err. Error en registro\n";
                                    send(i, msg, strlen(msg), 0);
                                }
                            } else {
                                char *msg = "-Err. Formato: REGISTRO <usuario> <password>\n";
                                send(i, msg, strlen(msg), 0);
                            }
                        } else if (strncmp(comando, "USUARIO", 7) == 0){
                            if (num_args == 2){
                                if (existe_usuario(arg1)){
                                    clientes[idx].estado = 1; // cambiamos estado a esperando contrasenia
                                    strcpy(clientes[idx].nombre, arg1); // guardamos el nombre de usuario

                                    char *msg = "+Ok. Usuario correcto\n";
                                    send(i, msg, strlen(msg), 0);
                                } else {
                                    char *msg = "-Err. Usuario incorrecto\n";
                                    send(i, msg, strlen(msg), 0);
                                }
                            } else {
                                char *msg = "-Err. Formato: USUARIO <usuario>\n";
                                send(i, msg, strlen(msg), 0);
                            }
                        } else {
                            char *msg = "-Err. Comando no reconocido\n";
                            send(i, msg, strlen(msg), 0);
                        }
                    } else {
                        // recibidos == 0 -> cliente cerro la conexion
                        // recibidos == -1 -> error en la lectura
                        printf("El cliente del socket %d se ha desconectado\n", i);
                        close(i);
                        FD_CLR(i, &readfds);

                        // borramos sobreescribiendo los sockets uno a uno, pasando todos una posicion a la izquierda desde el que se ha desconectado
                        for (; idx< numClientes - 1; idx++){
                            clientes[idx] = clientes[idx + 1];
                        }
                        numClientes--;

                    }
                }
            }
        }
    }

    close(fd);
    return 0;

}
    





















