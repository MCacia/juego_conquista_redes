#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PUERTO 2026
#define MSG_SIZE 250 
#define MAX_CLIENTES 10

#define MAX_USER 32
#define FICHERO_USUARIOS "usuarios.txt"

#define MAX_PARTIDAS 4
#define FILAS 5
#define COLS 5
#define UNIDADES_INICIO 30

typedef struct {
    int fd; // descriptor del socket del cliente
    char nombre[50]; // Nombre de usuario en proceso de login o validado
    int estado; // 0: no logueado(ST_CONECTADO), 1: Esperando password(ST_USUARIO_OK), 2: Logueado(ST_VALIDADO), 3: Esperando oponente(ST_ESPERA), 4: Jugando(ST_JUGANDO)
    int partida;    // numero de partida en la que esta (-1 = ninguna)
int jugador;    // 0 = J1, 1 = J2
} Cliente;

typedef struct {
    int activa;                 // 1 = en uso, 0 = libre
    int fd[2];                  // sockets de J1 y J2
    int dueno[FILAS][COLS];     // 0 = neutral, 1 = J1, 2 = J2
    int unidades[FILAS][COLS]; // numero de tropas que tiene cada jugador en X casilla
    int turno;
    int numTurnos;
} Partida;

Cliente clientes[MAX_CLIENTES];
int numClientes = 0;
Partida partidas[MAX_PARTIDAS];
fd_set readfds; // array de sockets de cliente

void enviar(int fd, const char *msg) {
    char buf[MSG_SIZE];
    snprintf(buf, sizeof(buf), "%s\n", msg);
    send(fd, buf, strlen(buf), 0);
}


int existe_usuario(const char *user) {
    FILE *f = fopen(FICHERO_USUARIOS, "r"); // abrimos el fichero de usuarios
    char u[MAX_USER], p[MAX_USER];          // creamos las variables de usuario y contraseña
    if (!f)
        return 0; // comprobamos que se ha abierto el fichero correctamente
    while (fscanf(f, "%31s %31s", u, p) == 2)
        if (strcmp(u, user) == 0)
        { // comprobamos que el usuario existe comparando u(el archivo) con user(entrada de teclado)
            fclose(f);
            return 1; // si no es correcto devuelve 1
        }
    fclose(f); // cerramos el archivo
    return 0;  // si es correcto devuelve 0
}

int passwordCorrecta(const char *user, const char *pass) {
    FILE *f = fopen(FICHERO_USUARIOS, "r");
    char u[MAX_USER], p[MAX_USER];
    if (!f) {
        return 0;
    }

    while (fscanf(f, "%31s %31s", u, p) == 2)
        if (strcmp(u, user) == 0) { // verificamos que el usuario coincide
            fclose(f); // cerramos el archivo
            return strcmp(p, pass) == 0; // comprobamos que la contrasenia del archivo coincida con la que le ha escrito el usuario
        }
    fclose(f);
    return 0;
}

// Devuelve 1 si ya hay OTRO cliente validado (estado >= 2) con ese nombre de usuario
int usuarioYaConectado(const Cliente *c, const char *nombre) {
    for (int i = 0; i < numClientes; i++)
        if (clientes[i].fd != c->fd && clientes[i].estado >= 2 && strcmp(clientes[i].nombre, nombre) == 0)
            return 1;
    return 0;
}

void login(Cliente *c, char *cmd, char *arg) {

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
            if (usuarioYaConectado(c, arg)){
                enviar(c->fd, "-Err. Usuario ya conectado desde otro cliente");
                return;
            }
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
            // comprobacion final: otro cliente pudo validarse con este usuario mientras tanto
            if (usuarioYaConectado(c, c->nombre)){
                c->estado = 0;
                c->nombre[0] = '\0';
                enviar(c->fd, "-Err. Usuario ya conectado desde otro cliente");
                return;
            }
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
        return 0; // Si el usuario existe salimos
    }

    // Comprobamos que usuario y contrasenna sean validos
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



void registro(Cliente *c, char *arg) {
    char user[MSG_SIZE], pass[MSG_SIZE], extra[MSG_SIZE];

    if (c->estado != 0){
        enviar(c->fd, "-Err. No puedes registrarte en este momento");
        return;
    }
    // debe haber exactamente 2 palabras: usuario y password
    if (sscanf(arg, "%s %s %s", user, pass, extra) != 2){
        enviar(c->fd, "-Err. Formato: REGISTRO usuario password");
        return;
    }

    int res = registrar_usuario(user, pass);
    if (res == 1)
        enviar(c->fd, "+Ok. Registro exitoso");
    else if (res == 0)
        enviar(c->fd, "-Err. Usuario existente");
    else
        enviar(c->fd, "-Err. Error en registro");
}

// Devuelve un puntero al cliente que tiene ese socket
Cliente *buscarCliente(int fd) {
   for (int i = 0; i < numClientes; i++)
        if (clientes[i].fd == fd) return &clientes[i];
    return NULL;
}

// Si el cliente estaba jugando, avisa al oponente y libera la partida
void abandonarPartida(Cliente *c) {
    if (c->estado != 4) return;

    Partida *p = &partidas[c->partida];
    Cliente *otro = buscarCliente(p->fd[1 - c->jugador]);

    if (otro != NULL){
        enviar(otro->fd, "+Ok. Tu oponente ha terminado la partida");
        otro->estado = 2;
        otro->partida = -1;
    }
    p->activa = 0;
}

// Envia las instrucciones de uso del programa
void cmdAyudaInicio(int fd) {
    enviar(fd, "---- MANUAL DE INICIO -----");
    enviar(fd, "REGISTRO : Registrar usuario nuevo en la BB.DD. Uso: REGISTRO <name> <password>");
    enviar(fd, "USUARIO : Iniciar sesion como un usuario existente. Uso: USUARIO <name>");
    enviar(fd, "PASSWORD : Autenticar la identidad del usuario usando la contrasena. Uso: PASSWORD <password>");
    enviar(fd, "INICIAR-PARTIDA : Iniciar una nueva partida. Uso: INICIAR-PARTIDA");
    enviar(fd, "SALIR : Abandonar el servidor");
    enviar(fd, "---------------------------");
}

// Envia las instrucciones del juego respetando el límite de búfer
void cmdAyuda(Cliente *c) {
    enviar(c->fd, "+Ok. --- MANUAL DE JUEGO ---");
    enviar(c->fd, "ESTADO : Muestra el tablero actual");
    enviar(c->fd, "CONQUISTAR [origen] [destino] [uds] : Conquista neutral adyacente");
    enviar(c->fd, "ATACAR [origen] [destino] [uds] : Ataca enemigo adyacente");
    enviar(c->fd, "REFORZAR [origen] [destino] [uds] : Mueve tropas propias");
    enviar(c->fd, "PASAR : Cede el turno al rival");
    enviar(c->fd, "SALIR : Abandona el servidor");
    enviar(c->fd, "----------------------------");
}

// Crea la partida entre a (J1) y b (J2)
void crearPartida(Cliente *a, Cliente *b){
    int p; // inicializamos un contador de partidas
    for (p = 0; p < MAX_PARTIDAS; p++)
        if (!partidas[p].activa) break; // si las partidas superan el maximo de partidas paramos.

    if (p == MAX_PARTIDAS){
        enviar(b->fd, "-Err. No hay hueco para mas partidas");
        return;
    }

    memset(&partidas[p], 0, sizeof(Partida)); // limpiamos la estructura de partidas
    partidas[p].activa = 1; // como vamos a crear una partida, ponemos a uno el selector de partidas activas
    partidas[p].fd[0] = a->fd; // socket cliente a
    partidas[p].fd[1] = b->fd; // socket cliente b

    // casilla inicial aleatoria y distinta para cada jugador
    int f1 = rand() % FILAS, c1 = rand() % COLS;
    int f2, c2;
    do {
        f2 = rand() % FILAS;
        c2 = rand() % COLS;
    } while (f2 == f1 && c2 == c1);

    partidas[p].dueno[f1][c1] = 1; // ponemos como dueño de la casilla aleatoria a J1
    partidas[p].unidades[f1][c1] = UNIDADES_INICIO; // asignamos sus tropas
    partidas[p].dueno[f2][c2] = 2;
    partidas[p].unidades[f2][c2] = UNIDADES_INICIO;

    a->estado = 4;  a->partida = p;  a->jugador = 0; // cambiamos el estado a "ST_JUGANDO"
    b->estado = 4;  b->partida = p;  b->jugador = 1;

    enviar(a->fd, "+Ok. Empieza la partida"); // notificamos a los usuarios 
    cmdAyuda(a); // Imprime el manual a J1
    enviar(b->fd, "+Ok. Empieza la partida");
    cmdAyuda(b); // Imprime el manual a J2
}

void iniciarPartida(Cliente *c){
    // esto es simplemente validaciones antes de empezar la partida
    if (c->estado < 2){
        enviar(c->fd, "-Err. Debes validarte antes de iniciar una partida");
        return;
    }
    if (c->estado == 3){
        enviar(c->fd, "-Err. Ya estas esperando otro jugador");
        return;
    }
    if (c->estado == 4){
        enviar(c->fd, "-Err. Ya estas en una partida");
        return;
    }

    // buscamos a alguien que ya este esperando
    Cliente *rival = NULL;
    for (int i = 0; i < numClientes; i++){
        if (clientes[i].estado == 3){
            rival = &clientes[i];
            break;
        }
    }

    if (rival == NULL){
        c->estado = 3;
        enviar(c->fd, "+Ok. Esperando otro jugador");
    } else {
        crearPartida(rival, c);
    }
}



// muestra el tablero tus casillas con unidades, las del rival sin unidades
void cmdEstado(Cliente *c){
    // comprobaciones...
    if (c->estado != 4){
        enviar(c->fd, "-Err. No estas dentro de una partida");
        return;
    }
    Partida *p = &partidas[c->partida]; // creamos un puntero a la partida de dicho cliente
    int yo = c->jugador + 1; // 1 o 2
    char mapa[512] = "+Ok. Tablero\n   1       2       3       4       5\n";
    char linea[100], texto[16], celda[16];

    // esto basicamente es el codigo para "dibujar el mapa"
    for (int f = 0; f < FILAS; f++){
        sprintf(linea, "%c  ", 'A' + f); // letra de la fila

        for (int col = 0; col < COLS; col++){
            int d = p->dueno[f][col];

            if (d == 0)
                strcpy(texto, ".");
            else if (d == yo)
                sprintf(texto, "J%d(%d)", d, p->unidades[f][col]);
            else
                sprintf(texto, "J%d", d);

            sprintf(celda, "%-8s", texto);         // ancho fijo de 8
            strcat(linea, celda);
        }
        strcat(linea, "\n");
        strcat(mapa, linea);
    }
    send(c->fd, mapa, strlen(mapa), 0);
}

void eliminarCliente(int fd_cliente){

    int idx = -1;
    for(int i = 0; i < numClientes; i++){
        if (clientes[i].fd == fd_cliente){
            idx = i;
            break;
        }
    }

    if (idx != -1){
        if (clientes[idx].estado == 4) { // si es 4 su estado, el cliente se encuentra en partida
            abandonarPartida(&clientes[idx]);
        }

        close(fd_cliente);
        FD_CLR(fd_cliente, &readfds);

        for(; idx < numClientes - 1; idx++){
            clientes[idx] = clientes[idx + 1];
        }
        numClientes--;

        printf("El cliente del socket %d ha sido eliminado correctamente.\n", fd_cliente);
    }
}

// logica de casillas

int parsearCasilla(const char *casilla, int *f, int *c){
    
    if (strlen(casilla) != 2){
        return 0;
    }

    *f = casilla[0] - 'A';
    *c = casilla[1] - '1';
    // jugamos con los valores numericos en ascii de las letras y numeros, si 'A' es 65 y 'B' es 66, cuando el usuario inserte 'B3' como casilla, *f contendra 66 - 65 = 1

    if (*f < 0 || *f >= FILAS || *c < 0 || *c >= COLS){
        return 0;
    }
    return 1;
}



int sonAdyacentes(int f1, int c1, int f2, int c2){
    // hace el valor absoluto de las casillas y columnas para ver si estan adyacentes o no
    int df = abs(f1 - f2);
    int dc = abs(c1 - c2);

    return (df == 1 && dc == 0) || (df == 0 && dc == 1);
}

// Lógica de recuento de ganador y perdedor y liberación de recursos de la partida
void finalizarPartida(Partida *p) {
    int terr1 = 0, terr2 = 0;
    int uds1 = 0, uds2 = 0;

    // Escaneo del tablero (recuento de unidades y territorios)
    for (int i = 0; i < FILAS; i++) {
        for (int j = 0; j < COLS; j++) {
            if (p->dueno[i][j] == 1) {
                terr1++;
                uds1 += p->unidades[i][j];
            } else if (p->dueno[i][j] == 2) {
                terr2++;
                uds2 += p->unidades[i][j];
            }
        }
    }

    char msg[MSG_SIZE];
    if (terr1 > terr2) {
        // Caso 1: Jugador 1 con mas territorios conquistados que Jugador 2
        snprintf(msg, sizeof(msg), "+Ok. Fin de partida. Ganador J1");
    } else if (terr2 > terr1) {
        // Caso 2: Jugador 2 con mas territorios conquistados que Jugador 1
        snprintf(msg, sizeof(msg), "+Ok. Fin de partida. Ganador J2");
    } else {
        // Caso 3: Empate, miramos las unidades de cada uno para dictar el ganador
        if (uds1 > uds2){
            snprintf(msg, sizeof(msg), "+Ok. Fin de partida. Empate en territorios. Ganador J1 por unidades");
        } else if (uds2 > uds1){
            snprintf(msg, sizeof(msg), "+Ok. Fin de partida. Empate en territorios. Ganador J2 por unidades");
        } else {
            snprintf(msg, sizeof(msg), "+Ok. Fin de partida. Empate total");
        }
    }

    Cliente *j1 = buscarCliente(p->fd[0]);
    Cliente *j2 = buscarCliente(p->fd[1]);

    // Devolvemos a los clientes al estado "Validado" para que puedan buscar otra partida
    if (j1) { 
        enviar(j1->fd, msg);
        j1->estado = 2; 
        j1->partida = -1; 
        j1->jugador = -1; 
    }
    if (j2) {  
        enviar(j2->fd, msg); 
        j2->estado = 2; 
        j2->partida = -1; 
        j2->jugador = -1; 
    }

    // Liberamos el "slot" de la partida
    p->activa = 0; 
}


void avanzarTurno (Partida *p){
    p->turno = 1-p->turno;
    p->numTurnos++;

    // establecemos el limite de partidas
    if (p->numTurnos >= 10) { 
        finalizarPartida(p);
        return;
    }

    // comprobamos si alguien ha sido eliminado, es decir que no este en ninguna casilla
    int vivos[2]={0, 0};
    for(int i=0; i<FILAS; i++){
        for(int j=0; j<COLS; j++){
            if (p->dueno[i][j] == 1){
                vivos[0] = 1;
            } else if (p->dueno[i][j] == 2){
                vivos[1] = 1;
            }
        }
    }

    if (!vivos[0] || !vivos[1]) {
        finalizarPartida(p);
        return;
    }
    
    Cliente *siguiente = buscarCliente(p->fd[p->turno]); // buscamos al cliente nuevo
    if(siguiente){
        enviar(siguiente->fd, "+Ok. Es tu turno");
    }
}
    
// FUNCION COMPARTIDA POR CONQUISTAR, ATACAR Y REFORZAR
int validarAccion(Cliente *c, char *arg, int *f1, int *c1, int *f2, int *c2, int *uds) {

    // comprobaciones de errores comunes en las tres funciones
    if (c->estado != 4){
        enviar(c->fd, "-Err. No estas dentro de una partida");
        return 0;
    }

    Partida *p = &partidas[c->partida];
    if (p->turno != c->jugador){
        enviar(c->fd, "-Err. No es tu turno");
        return 0;
    }

    char cas1[10], cas2[10];
    if (sscanf(arg, "%s %s %d", cas1, cas2, uds) != 3){
        enviar(c->fd, "-Err. Formato incorrecto");
        return 0;
    }

    if (!parsearCasilla(cas1, f1, c1) || !parsearCasilla(cas2, f2, c2)) {
        enviar(c->fd, "-Err. Coordenadas invalidas");
        return 0;
    }

    if (!sonAdyacentes(*f1, *c1, *f2, *c2)) {
        enviar(c->fd, "-Err. Los territorios no son adyacentes");
        return 0;
    }

    if (p->dueno[*f1][*c1] != c->jugador + 1){
        enviar(c->fd, "-Err. El territorio de origen no te pertenece");
        return 0;
    }

    if (*uds <= 0 || *uds > p->unidades[*f1][*c1]){
        enviar(c->fd, "-Err. No dispones de suficientes unidades en el origen");
        return 0;
    }

    return 1;
}

void cmdConquistar(Cliente *c, char *arg){
    int f1, f2, c1, c2, uds; // definimos variables

    if(!validarAccion(c, arg, &f1, &c1, &f2, &c2, &uds)){
        return;
    }
    
    Partida *p = &partidas[c->partida];

    if(p->dueno[f2][c2] != 0){ // para la conquista el territorio debe ser neutral
        enviar(c->fd, "-Err. El territorio destino no es neutral");
        return;
    }

    // modificamos la memoria del servidor y actualizamos la matriz
    p->unidades[f1][c1] -= uds;
    p->dueno[f2][c2] = c->jugador + 1;
    p->unidades[f2][c2] = uds;

    // confirmar al cliente y avanzar en el turno
    char resp[100];
    snprintf(resp, sizeof(resp), "+Ok. Territorio %c%c conquistado.", f2 + 'A', c2 + '1');
    enviar(c->fd, resp);
    
    avanzarTurno(p);
}

void cmdReforzar(Cliente *c, char *arg){

    int f1, c1, f2, c2, uds;
    // validamos que la accion es posible
    if (!validarAccion(c, arg, &f1, &c1, &f2, &c2, &uds)){
        return;
    }

    Partida *p = &partidas[c->partida];
    
    // No podemos reforzar un territorio que no nos pertenece
    if (p->dueno[f2][c2] != c->jugador + 1){
        enviar(c->fd, "-Err. El territorio destino no te pertenece");
        return;
    }

    p->unidades[f1][c1] -= uds;
    p->unidades[f2][c2] += uds;

    enviar(c->fd, "+Ok. Territorio reforzado");
    avanzarTurno(p);
}

void cmdAtacar(Cliente *c, char *arg){
    int f1, c1, f2, c2, uds;
    
    if(!validarAccion(c, arg, &f1, &c1, &f2, &c2, &uds)){
        return;
    }
    
    Partida *p = &partidas[c->partida];
    
    if (p->dueno[f2][c2] == 0 || p->dueno[f2][c2] == c->jugador + 1){
        enviar(c->fd, "-Err. El territorio destino no pertenece al enemigo");
        return;
    }

    int def_uds = p->unidades[f2][c2];
    p->unidades[f1][c1] -= uds;

    char resp_atk[150];
    char resp_def[150];
    Cliente *enemigo = buscarCliente(p->fd[1 - c->jugador]);
    
    if (uds > def_uds) {
        p->dueno[f2][c2] = c->jugador + 1;
        p->unidades[f2][c2] = uds;
        snprintf(resp_atk, sizeof(resp_atk), "+Ok. Has conquistado el territorio %c%c", f2 + 'A', c2 + '1');
        snprintf(resp_def, sizeof(resp_def), "+Ok. Te han atacado en %c%c y has perdido el territorio", f2 + 'A', c2 + '1');
    } else {
        snprintf(resp_atk, sizeof(resp_atk), "+Ok. Has perdido el ataque contra %c%c", f2 + 'A', c2 + '1');
        snprintf(resp_def, sizeof(resp_def), "+Ok. Te han atacado en %c%c y has defendido con exito", f2 + 'A', c2 + '1');
    }
    
    enviar(c->fd, resp_atk);
    if (enemigo != NULL) {
        enviar(enemigo->fd, resp_def);
    }
    
    avanzarTurno(p);
    
}

void procesarMensaje(Cliente *c, char *msg)
{
    char cmd[MSG_SIZE] = "", arg[MSG_SIZE] = "";
    char *sp = strchr(msg, ' ');

    if (sp) {
        *sp = '\0';
        strcpy(cmd, msg);
        strcpy(arg, sp + 1);
    } else {
        strcpy(cmd, msg);
    }

    if (strcmp(cmd, "USUARIO") == 0 || strcmp(cmd, "PASSWORD") == 0) {
        login(c, cmd, arg);
    }
    // registro
    else if (strcmp(cmd, "REGISTRO") == 0) {
        registro(c, arg);
    } else if (strcmp(cmd, "INICIAR-PARTIDA") == 0) {
        iniciarPartida(c);
    } else if (strcmp(cmd, "ESTADO") == 0) {
        cmdEstado(c);
    } else if (strcmp(cmd, "AYUDA") == 0){
        cmdAyuda(c);
    } else if (strcmp(cmd, "SALIR") == 0) {
        eliminarCliente(c->fd);   // el cliente cierra su socket y el main lo elimina al detectarlo
    } else if (strcmp(cmd, "PASAR") == 0){
        if (c->estado != 4){
            enviar(c->fd, "-Err. No estas dentro de una partida");
        } else {
            Partida *p = &partidas[c->partida];
            if (p->turno != c->jugador) {
                enviar(c->fd, "-Err. No es tu turno");
            } else {
                enviar(c->fd, "+Ok. Turno pasado");
                avanzarTurno(p);
            }
        }
    } else if (strcmp(cmd, "CONQUISTAR") == 0){
        cmdConquistar(c, arg);
    } else if (strcmp(cmd, "REFORZAR") == 0){
        cmdReforzar(c, arg);
    } else if (strcmp(cmd, "ATACAR") == 0){
        cmdAtacar(c, arg);
    } else {
        enviar(c->fd, "-Err. Comando no reconocido");
    }
}



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