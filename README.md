# Práctica 2: Redes - Dominio Territorial

**Asignatura:** Redes (3º Curso)  
**Titulación:** Grado en Ingeniería Informática (Mención en Computación)  
**Centro:** Escuela Politécnica Superior de Córdoba (EPSC) - Universidad de Córdoba, España  
**Autores:** Samuel Ibarra García y Marcos Cáceres García  

---

## Descripción del Proyecto

Este repositorio contiene la implementación completa de la **Práctica 2** de la asignatura de Redes. El proyecto consiste en el desarrollo de una arquitectura Cliente/Servidor en C bajo el protocolo TCP para el juego multijugador por turnos **"Dominio Territorial"**[cite: 13].

El servidor es concurrente de un solo hilo (Single-Threaded Concurrent Server), utilizando la llamada al sistema `select()` para gestionar el registro, autenticación y multiplexación de hasta 10 clientes conectados de forma simultánea, permitiendo un máximo de 4 partidas concurrentes[cite: 23].

## Estructura del Código

El proyecto sigue una arquitectura modular, separando la lógica de red de las reglas de negocio del juego mediante el uso de cabeceras y múltiples unidades de traducción:

*   **`servidor.h`**: Archivo de cabecera que define estructuras de datos (`Cliente`, `Partida`), variables globales (`extern`) y  funciones compartidas.
*   **`servidor.c`**: Contiene la lógica del juego (validación de adyacencias, turnos, ataques, refuerzos y recuento de victorias).
*   **`main_servidor.c`**: Despliega el servidor TCP en el puerto `2026` e implementa el bucle infinito de escucha de eventos de red con `select()`.
*   **`cliente.c`**: Aplicación cliente TCP que lee de la entrada estándar y recibe datos del servidor de manera asíncrona.
*   **`usuarios.txt`**: Archivo de texto plano utilizado como base de datos persistente para el registro de usuarios.

## Requisitos y Compilación

El proyecto está diseñado para sistemas UNIX/Linux. No requiere herramientas de automatización; la compilación se realiza directamente a través del compilador GNU (`gcc`).

Para compilar el servidor enlazando sus módulos, ejecuta en la raíz del directorio:

`gcc main_servidor.c servidor.c -o servidor -Wall -Wextra`

Para compilar el cliente:

`gcc cliente.c -o cliente -Wall -Wextra`

## Ejecución

1. Levantar el servidor en la máquina anfitriona:
   `./servidor`

2. Conectar un cliente desde la misma máquina o red local (reemplazando la IP por la del equipo servidor si se juega en red):
   `./cliente 127.0.0.1`

## Protocolo de Comandos Disponibles

El protocolo de la capa de aplicación admite los siguientes comandos integrados respetando el tamaño máximo de búfer de 200 caracteres:

**Gestión de Sesión:**
*   `REGISTRO [usuario] [password]`: Registra una nueva cuenta en la base de datos local y evita colisiones.
*   `USUARIO [usuario]`: Inicia la fase de autenticación.
*   `PASSWORD [password]`: Completa la autenticación y valida al usuario.
*   `SALIR`: Cierra el socket y libera los recursos de memoria del servidor de forma segura.

**Mecánicas de Partida:**
*   `INICIAR-PARTIDA`: Busca un oponente en la cola de emparejamiento para comenzar el juego.
*   `ESTADO`: Muestra la matriz del tablero 5x5 con las tropas y territorios actualizados.
*   `CONQUISTAR [origen] [destino] [unidades]`: Desplaza tropas a una casilla neutral adyacente.
*   `ATACAR [origen] [destino] [unidades]`: Inicia una contienda contra un territorio enemigo adyacente.
*   `REFORZAR [origen] [destino] [unidades]`: Transfiere tropas entre dos territorios propios.
*   `PASAR`: Finaliza el turno actual y cede el control al rival
*   `AYUDA`: Despliega el manual de uso interactivo por consola.
