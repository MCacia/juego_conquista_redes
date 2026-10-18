#ifndef SERVIDOR_H
#define SERVIDOR_H

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
    int fd; 
    char nombre[50]; 
    int estado; 
    int partida;    
    int jugador;    
} Cliente;

typedef struct {
    int activa;                 
    int fd[2];                  
    int dueno[FILAS][COLS];     
    int unidades[FILAS][COLS]; 
    int turno;
    int numTurnos;
} Partida;

// variables globales
extern Cliente clientes[MAX_CLIENTES];
extern int numClientes;
extern Partida partidas[MAX_PARTIDAS];
extern fd_set readfds;

// Prototipos de funciones
void enviar(int fd, const char *msg);
int existe_usuario(const char *user);
int passwordCorrecta(const char *user, const char *pass);
int usuarioYaConectado(const Cliente *c, const char *nombre);
void login(Cliente *c, char *cmd, char *arg);
int registrar_usuario(char *usuario, char *password);
void registro(Cliente *c, char *arg);
Cliente *buscarCliente(int fd);
void abandonarPartida(Cliente *c);
void cmdAyudaInicio(int fd);
void cmdAyuda(Cliente *c);
void crearPartida(Cliente *a, Cliente *b);
void iniciarPartida(Cliente *c);
void cmdEstado(Cliente *c);
void eliminarCliente(int fd_cliente);
int parsearCasilla(const char *casilla, int *f, int *c);
int sonAdyacentes(int f1, int c1, int f2, int c2);
void finalizarPartida(Partida *p);
void avanzarTurno (Partida *p);
int validarAccion(Cliente *c, char *arg, int *f1, int *c1, int *f2, int *c2, int *uds);
void cmdConquistar(Cliente *c, char *arg);
void cmdReforzar(Cliente *c, char *arg);
void cmdAtacar(Cliente *c, char *arg);
void procesarMensaje(Cliente *c, char *msg);

#endif