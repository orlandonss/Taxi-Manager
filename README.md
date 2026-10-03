
# Taxi Manager: Concurrent Fleet Management System

Taxi Manager is a **POSIX-compliant, concurrent Client-Server system** designed to manage a fleet of vehicles and handle passenger ride scheduling.

Built entirely in **C**, the architecture relies heavily on **Inter-Process Communication (IPC)** mechanisms—specifically **Named Pipes (FIFOs), Unnamed Pipes, and Signals**—alongside **POSIX threads (`pthreads`)** to achieve non-blocking, asynchronous operations across multiple clients and vehicles.

---

## Table of Contents

* [System Overview](#system-overview)
* [System Architecture](#system-architecture)
* [IPC Communication Scheme](#ipc-communication-scheme)
* [Communication Flow](#communication-flow)
* [Message Protocol](#message-protocol)
* [Message Operation Codes](#message-operation-codes)
* [Server Concurrency Control](#server-concurrency-control)
* [System Components](#system-components)
* [Quick Start](#quick-start)

  * [Prerequisites](#prerequisites)
  * [Compilation](#compilation)
  * [Starting the System](#starting-the-system)
* [Usage Tutorial](#usage-tutorial)

  * [Client Commands](#client-commands)
  * [Administrator Commands](#administrator-commands)
* [Ride Lifecycle](#ride-lifecycle)
* [Vehicle Management](#vehicle-management)
* [Cancellation Mechanism](#cancellation-mechanism)
* [Graceful Shutdown](#graceful-shutdown)
* [IPC Summary](#ipc-summary)

---

# System Overview

The Taxi Manager system follows a **Client-Server architecture** composed of three main types of processes:

1. **Server (`controlador`)**

   * Central controller of the system.
   * Manages connected clients.
   * Maintains the fleet and scheduled rides.
   * Dispatches vehicle processes.
   * Handles administrative commands.
   * Coordinates communication between clients and vehicles.
2. **Clients (`cliente`)**

   * Represent passengers/users.
   * Connect to the server through a named FIFO.
   * Submit ride requests.
   * Query existing rides.
   * Cancel scheduled rides.
   * Receive asynchronous notifications from the server.
3. **Vehicles (`app_veiculo`)**

   * Simulated child processes created by the server.
   * Represent individual taxis.
   * Execute scheduled rides.
   * Report progress to the server.
   * Can be interrupted using signals.

The system is designed so that no single client, vehicle, or administrative operation unnecessarily blocks the rest of the system.

---

# System Architecture

The following diagram illustrates the main communication paths between clients, the server, and vehicle processes.

```bash
=======================================================================
                   PROCESS COMMUNICATION PIPELINE
=======================================================================

   +----------------+
   |   CLIENT (1)   |
   |   (PID 1001)   | ---- (write) ----> [ FIFO_SRV: "fifo_servidor" ]
   +----------------+                               |
           ^                                        | (read)
           |                                        v
  (read)   |                              +-------------------+
[ FIFO_CLI_1001 ] <------- (write) ------ |                   |
                                          |      SERVER       |
   +----------------+                     |  (Controlador)    |
   |   CLIENT (2)   | ---- (write) ---->  |                   |
   |   (PID 1002)   |                     +-------------------+
   +----------------+                           |       ^
           ^                                    |       |
           |                              fork()|       | (Unnamed Pipe)
  (read)   |                             SIGUSR1|       |
[ FIFO_CLI_1002 ] <------- (write) ------       v       |
                                          +-------------------+
                                          |      VEHICLE      |
                                          |  (app_veiculo)    |
                                          +-------------------+
```

---

# IPC Communication Scheme

The system uses three primary IPC mechanisms:

| IPC Mechanism          | Communication     | Purpose                               |
| ---------------------- | ----------------- | ------------------------------------- |
| **Named FIFO**   | Client → Server  | Client requests                       |
| **Named FIFO**   | Server → Client  | Responses and notifications           |
| **Unnamed Pipe** | Vehicle → Server | Vehicle telemetry                     |
| **Signals**      | Server → Vehicle | Vehicle cancellation                  |
| **Pthreads**     | Within Server     | Concurrent task execution             |
| **`select()`** | Within Client     | Simultaneous keyboard/FIFO monitoring |

These mechanisms allow the system to handle multiple users and vehicles concurrently.

---

# Communication Flow

## 1. Client → Server

All clients send their requests to a single, well-known public FIFO:

```text
fifo_servidor
```

The server has a dedicated client-handling thread, referred to as:

```text
tarefa_clientes
```

This thread continuously monitors the server FIFO and processes incoming `Pedido` messages.

Conceptually:

```bash
CLIENT
   |
   | Pedido
   v
fifo_servidor
   |
   v
tarefa_clientes
   |
   v
SERVER STATE
```

---

## 2. Server → Client

After connecting to the server, each client creates a private FIFO based on its process ID.

For example:

```text
fifo_cli_1001
fifo_cli_1002
fifo_cli_1003
```

The server uses these FIFOs to send responses and asynchronous notifications to individual clients.

```bash
                         +----------------+
                         |     SERVER     |
                         +----------------+
                           |      |      |
                           v      v      v
                     fifo_cli_1001
                     fifo_cli_1002
                     fifo_cli_1003
                           |      |      |
                           v      v      v
                       CLIENTS
```

The client uses the `select()` system call to monitor both:

* The client FIFO.
* Standard input (`stdin`).

This allows the client to receive server notifications while still accepting keyboard input.

---

## 3. Server → Vehicle

When a scheduled ride is ready to start, the server creates a new vehicle process.

The process creation sequence is conceptually:

```bash
Server
  |
  +-- pipe()
  |
  +-- fork()
        |
        +-- Child
              |
              +-- exec("app_veiculo")
```

An **unnamed pipe** is established between the server and the vehicle.

The vehicle uses this pipe to report telemetry such as:

* Ride initialization.
* Current progress.
* Ride completion.
* Ride cancellation.

The server creates a dedicated monitor thread:

```text
tarefa_monitor
```

for each active vehicle.

This prevents telemetry from one vehicle from blocking the processing of other system events.

---

## 4. Vehicle Cancellation via Signals

If a vehicle must be stopped while performing a ride, the server sends:

```c
SIGUSR1
```

to the vehicle process.

The vehicle registers a signal handler that detects the cancellation request.

The simplified sequence is:

```bash
SERVER
   |
   | kill(vehicle_pid, SIGUSR1)
   v
VEHICLE
   |
   | Signal handler
   v
Report cancellation
   |
   v
Terminate gracefully
```

The vehicle reports its cancellation status through the unnamed pipe before terminating.

---

# Message Protocol

All system components communicate using a standardized C structure called `Pedido`.

The structure provides a fixed-size message format suitable for communication through pipes.

```c
typedef struct {
    int tipo;                // Action code
    pid_t pid_origem;        // Originating process ID
    char str_dados[100];     // Text payload
    int val1;                // Generic integer value 1
    int val2;                // Generic integer value 2
} Pedido;
```

## Structure Fields

| Field          | Type          | Description                              |
| -------------- | ------------- | ---------------------------------------- |
| `tipo`       | `int`       | Identifies the type of operation/message |
| `pid_origem` | `pid_t`     | PID of the originating process           |
| `str_dados`  | `char[100]` | Textual data or message                  |
| `val1`       | `int`       | Generic integer value                    |
| `val2`       | `int`       | Generic integer value                    |

### Example Uses

Depending on the message type, the fields can contain different information.

For example, an `AGENDAR` request could use:

```text
tipo        -> AGENDAR
pid_origem  -> Client PID
str_dados   -> Destination
val1        -> Scheduled time / distance
val2        -> Additional service information
```

The exact interpretation of `val1`, `val2`, and `str_dados` depends on the operation being performed.

---

# Message Operation Codes

The system uses different message types for client requests, server responses, and vehicle telemetry.

## Client Requests

| Message Type     | Description                    |
| ---------------- | ------------------------------ |
| `LOGIN`        | Connect a client to the server |
| `AGENDAR`      | Schedule a new taxi ride       |
| `CONSULTAR`    | Query ride information         |
| `CANCELAR`     | Cancel a ride                  |
| `TERMINAR_CLI` | Disconnect the client          |

---

## Server Responses

| Message Type | Description                                 |
| ------------ | ------------------------------------------- |
| `OP_OK`    | Operation completed successfully            |
| `OP_ERRO`  | Operation failed or server is shutting down |
| `OP_INFO`  | Asynchronous informational notification     |

---

## Vehicle Telemetry

| Message Type           | Description                            |
| ---------------------- | -------------------------------------- |
| `V_STATUS_INICIO`    | Vehicle has started a ride             |
| `V_STATUS_ANDAMENTO` | Vehicle is currently performing a ride |
| `V_STATUS_CONCLUIDO` | Vehicle has completed a ride           |
| `V_STATUS_CANCELADO` | Vehicle has been canceled              |

---

# Server Concurrency Control

Because multiple threads operate on the server simultaneously, shared data must be protected from race conditions.

The server maintains a global state structure, referred to as:

```text
ctrl_estado
```

This state contains information about:

* Connected users.
* Scheduled rides.
* Active services.
* Vehicle processes.
* Fleet status.
* System time.
* Total kilometers traveled.

Access to this shared state is protected by a central mutex:

```c
pthread_mutex_t mutex_dados;
```

Threads must acquire the mutex before modifying shared state.

Conceptually:

```c
pthread_mutex_lock(&mutex_dados);

/* Access or modify shared state */

pthread_mutex_unlock(&mutex_dados);
```

This prevents two threads from modifying the same data simultaneously.

---

# Server Threads

The server uses multiple threads to perform different tasks concurrently.

## `tarefa_clientes`

Responsible for handling incoming client requests.

Typical responsibilities include:

* Receiving `Pedido` messages.
* Processing login requests.
* Scheduling rides.
* Processing ride queries.
* Handling client cancellations.
* Sending responses to clients.

---

## `tarefa_tempo`

Responsible for simulating the system clock.

Its general workflow is:

```text
Sleep 1 second
     |
     v
Increment simulated time
     |
     v
Check scheduled rides
     |
     v
Dispatch eligible rides
     |
     v
Repeat
```

This allows rides to be automatically dispatched when their scheduled time is reached.

---

## `tarefa_admin`

Provides the administrator with an interactive terminal.

The administrator can use commands to:

* Check system time.
* Inspect connected users.
* List rides.
* Inspect the fleet.
* View total kilometers.
* Cancel services.
* Shut down the server.

---

## `tarefa_monitor`

A monitor thread is created for each active vehicle.

Its primary responsibility is to read the vehicle's unnamed pipe and process telemetry messages.

```text
Vehicle
   |
   | Unnamed Pipe
   v
tarefa_monitor
   |
   v
Update ctrl_estado
   |
   v
Notify client
```

This architecture allows multiple vehicles to operate concurrently.

---

# System Components

A simplified project structure can look like this:

```text
TaxiManager/
│
├── controlador.c
├── cliente.c
├── veiculos.c
├── comum.c
│
├── controlador
├── cliente
├── app_veiculo
│
├── fifo_servidor
└── fifo_cli_<PID>
```

The exact project layout may differ depending on the implementation.

---

# Quick Start

## Prerequisites

The project requires a POSIX-compatible environment with:

* C compiler such as `gcc`.
* POSIX threads (`pthread`).
* POSIX IPC support.
* `fork()`.
* `exec()` / `exec*()`.
* Named pipes (`mkfifo()`).
* Unnamed pipes (`pipe()`).
* POSIX signals.
* `select()`.

Linux is recommended for development and testing.

---

# Compilation

Compile the server with pthread support:

```bash
gcc -Wall controlador.c comum.c -o controlador -lpthread
```

Compile the client:

```bash
gcc -Wall cliente.c comum.c -o cliente
```

Compile the vehicle application:

```bash
gcc -Wall veiculos.c -o app_veiculo
```

If the project contains additional source or header files, include them according to the project's Makefile/build configuration.

For example:

```bash
gcc -Wall -Wextra controlador.c comum.c -o controlador -lpthread
gcc -Wall -Wextra cliente.c comum.c -o cliente
gcc -Wall -Wextra veiculos.c comum.c -o app_veiculo
```

---

# Starting the System

The server must be started before clients attempt to connect.

## Terminal 1 — Server

```bash
./controlador
```

The server initializes the shared resources and waits for clients.

---

## Terminal 2 — Client

Start a client using a username:

```bash
./cliente "YourUsername"
```

For example:

```bash
./cliente "Alice"
```

Multiple clients can be started from different terminals:

```bash
./cliente "Alice"
./cliente "Bob"
./cliente "Charlie"
```

Each client receives its own process ID and communication FIFO.

---

# Usage Tutorial

Once the server and client are running, passengers can interact with the system through the client terminal.

---

# Client Commands

## `agendar`

Schedules a new taxi ride.

Syntax:

```text
agendar <hora> <local> <dist>
```

Where:

| Argument    | Description              |
| ----------- | ------------------------ |
| `<hora>`  | Simulated scheduled hour |
| `<local>` | Destination              |
| `<dist>`  | Distance in kilometers   |

### Example

```text
agendar 15 Aeroporto 20
```

This schedules a ride to:

```text
Aeroporto
```

for simulated hour:

```text
15
```

with a distance of:

```text
20 km
```

---

## `consultar`

Displays the user's scheduled and completed rides.

```text
consultar
```

Depending on the implementation, the output may include:

* Service ID.
* Destination.
* Scheduled time.
* Distance.
* Current status.
* Vehicle information.

---

## `cancelar`

Cancels a ride using its service ID.

Syntax:

```text
cancelar <id>
```

Example:

```text
cancelar 4
```

A scheduled ride can be canceled as long as it has not already started.

If the ride is already in progress, cancellation is handled through the vehicle cancellation mechanism.

---

## `sair`

Disconnects the client from the server.

```text
sair
```

The client should notify the server before terminating so that the server can remove the user from its active-user state.

---

# Administrator Commands

The administrator interacts directly with the server terminal.

---

## `hora`

Displays the current simulated system time.

```text
hora
```

Example:

```text
> hora
Hora atual: 15
```

---

## `utiliz`

Lists currently connected users.

```text
utiliz
```

Information may include:

* Username.
* PID.
* Connection status.

Example:

```text
> utiliz

Users:
PID      Username
1001     Alice
1002     Bob
1003     Charlie
```

---

## `listar`

Displays all services currently known by the server.

```text
listar
```

A service can have states such as:

```text
Scheduled
In Progress
Completed
Canceled
```

Example:

```text
> listar

ID     User       Destination    Status
1      Alice      Aeroporto      Scheduled
2      Bob        Centro         In Progress
3      Charlie    Estacao        Completed
```

---

## `frota`

Displays information about the active vehicle fleet.

```text
frota
```

Typical information includes:

* Vehicle PID.
* Associated service.
* Current progress.
* Vehicle state.

Example:

```text
> frota

Vehicle PID    Service ID    Progress
2001           2             65%
2002           5             20%
```

---

## `km`

Displays the total number of kilometers driven by the fleet.

```text
km
```

Example:

```text
> km

Total kilometers: 245 km
```

---

## `cancelar`

The administrator can cancel a specific ride.

Syntax:

```text
cancelar <id>
```

Example:

```text
cancelar 5
```

If the service has not started, it can be marked as canceled directly.

If the service is already in progress, the server sends:

```c
SIGUSR1
```

to the corresponding vehicle process.

The vehicle then:

1. Receives the signal.
2. Executes its signal handler.
3. Reports cancellation.
4. Terminates.
5. Allows the server to update the service state.

---

## Global Cancellation

The administrator can cancel all rides using:

```text
cancelar 0
```

This acts as a global cancellation operation.

Depending on the current state of each service, the server can:

* Mark scheduled services as canceled.
* Send cancellation signals to active vehicles.
* Update the corresponding client information.

---

## `terminar`

Gracefully shuts down the server.

```text
terminar
```

A graceful shutdown should perform the following operations:

1. Stop accepting new requests.
2. Notify connected clients.
3. Terminate active vehicle processes.
4. Wait for relevant threads/processes.
5. Close open file descriptors.
6. Remove named FIFOs.
7. Release synchronization resources.
8. Terminate the server process.

Conceptually:

```bash
                    TERMINAR
                       |
                       v
              Stop new operations
                       |
                       v
               Notify clients
                       |
                       v
              Stop active vehicles
                       |
                       v
              Wait for resources
                       |
                       v
               Close descriptors
                       |
                       v
                Remove FIFOs
                       |
                       v
              Destroy mutexes
                       |
                       v
                 Exit server
```

---

# Ride Lifecycle

A typical taxi ride follows this lifecycle:

```bash
                    +-----------+
                    | SCHEDULED |
                    +-----------+
                          |
                          | Scheduled time reached
                          v
                   +-------------+
                   | IN PROGRESS |
                   +-------------+
                    /           \
                   /             \
          completion             cancellation
                /                   \
               v                     v
       +-------------+       +-------------+
       |  COMPLETED  |       |  CANCELED   |
       +-------------+       +-------------+
```

## 1. Scheduled

The client submits:

```text
agendar <hora> <local> <dist>
```

The server creates a service entry and assigns it a unique ID.

The service remains scheduled until the simulated clock reaches its scheduled time.

---

## 2. In Progress

When the scheduled time is reached, `tarefa_tempo` detects the service and dispatches a vehicle.

The server:

```text
pipe()
fork()
exec()
```

creates the vehicle process.

The vehicle then reports:

```text
V_STATUS_INICIO
```

followed by periodic:

```text
V_STATUS_ANDAMENTO
```

messages.

---

## 3. Completed

When the vehicle reaches the end of the simulated trip, it reports:

```text
V_STATUS_CONCLUIDO
```

The server then updates the corresponding service and fleet state.

The client can receive an asynchronous notification.

---

## 4. Canceled

A service can be canceled before or during execution.

If the vehicle is already running, the server sends:

```text
SIGUSR1
```

The vehicle reports:

```text
V_STATUS_CANCELADO
```

and terminates.

---

# Vehicle Management

Vehicles are not permanently running processes.

Instead, the server creates vehicle processes when a ride needs to be executed.

The process lifecycle is approximately:

```bash
                  SERVER
                    |
                    | fork()
                    v
              VEHICLE PROCESS
                    |
                    | exec()
                    v
              app_veiculo
                    |
                    | simulation
                    v
              Send telemetry
                    |
                    v
              Ride completed
                    |
                    v
                  exit()
```

Each vehicle communicates its status to the server through an unnamed pipe.

---

# Vehicle Telemetry

During a ride, the vehicle can send several status messages.

## Start

```text
V_STATUS_INICIO
```

Indicates that the vehicle has started the ride.

---

## Progress

```text
V_STATUS_ANDAMENTO
```

Indicates that the ride is currently in progress.

The message can contain the current progress percentage.

For example:

```text
0%
25%
50%
75%
100%
```

---

## Completion

```text
V_STATUS_CONCLUIDO
```

Indicates that the ride has finished successfully.

---

## Cancellation

```text
V_STATUS_CANCELADO
```

Indicates that the ride was interrupted before normal completion.

---

# Cancellation Mechanism

Cancellation is handled differently depending on whether the ride has started.

## Before the ride starts

The service exists only in the server's state.

The server can simply mark it as:

```text
CANCELED
```

No vehicle process needs to be terminated.

---

## While the ride is running

A vehicle process already exists.

The server identifies the vehicle PID and sends:

```c
kill(vehicle_pid, SIGUSR1);
```

The vehicle's signal handler receives the signal.

A simplified implementation could follow this model:

```c
void handle_cancel(int signal)
{
    /* Mark cancellation request */
}
```

The vehicle then reports the cancellation through its communication pipe.

---

# Asynchronous Client Notifications

One of the important characteristics of the system is that clients do not need to continuously poll the server to discover changes.

The server can send asynchronous notifications through the client's private FIFO.

For example:

```bash
SERVER
   |
   | OP_INFO
   v
fifo_cli_1001
   |
   v
CLIENT
```

The client monitors its FIFO and keyboard input simultaneously using `select()`.

Conceptually:

```bash
                 +----------------+
                 |     CLIENT     |
                 +----------------+
                    ^          ^
                    |          |
              FIFO |          | stdin
                    |          |
             +------+----------+------+
             |       select()        |
             +-----------------------+
```

This allows the client to:

* Type commands.
* Receive ride updates.
* Receive cancellation notifications.
* Receive completion notifications.

without having to block on either source indefinitely.

---

# Concurrency Model

The system combines **process-based concurrency**, **thread-based concurrency**, and **IPC**.

The main concurrency model can be summarized as:

```bash
                         SERVER
                           |
          +----------------+----------------+
          |                |                |
          v                v                v
 tarefa_clientes     tarefa_tempo     tarefa_admin
          |
          |
          +----------------------+
                                 |
                                 v
                         Vehicle processes
                                 |
                  +--------------+--------------+
                  |              |              |
                  v              v              v
              Vehicle 1      Vehicle 2      Vehicle 3
                  |              |              |
                  v              v              v
            monitor thread  monitor thread  monitor thread
```

This allows different activities to proceed independently:

* Clients can submit requests concurrently.
* The administrator can issue commands.
* The simulated clock continues running.
* Multiple vehicles can operate simultaneously.
* Vehicle telemetry can be processed asynchronously.

---

# Synchronization

The server's shared state must be protected whenever multiple threads access it.

The central synchronization mechanism is:

```c
pthread_mutex_t mutex_dados;
```

A typical critical section follows:

```c
pthread_mutex_lock(&mutex_dados);

/* Read or modify shared server state */

pthread_mutex_unlock(&mutex_dados);
```

The mutex should be held only for the minimum amount of time necessary.

This reduces contention between server threads.

---

# Important POSIX APIs

The project makes use of several POSIX mechanisms.

| API                            | Purpose                                        |
| ------------------------------ | ---------------------------------------------- |
| `pthread_create()`           | Create server threads                          |
| `pthread_join()`             | Wait for threads                               |
| `pthread_mutex_lock()`       | Enter critical section                         |
| `pthread_mutex_unlock()`     | Leave critical section                         |
| `pipe()`                     | Create an unnamed pipe                         |
| `fork()`                     | Create vehicle processes                       |
| `exec()`                     | Replace child process with vehicle application |
| `mkfifo()`                   | Create named FIFO                              |
| `open()`                     | Open FIFO/file descriptors                     |
| `read()`                     | Read IPC data                                  |
| `write()`                    | Write IPC data                                 |
| `close()`                    | Close file descriptors                         |
| `select()`                   | Monitor multiple file descriptors              |
| `signal()` / `sigaction()` | Register signal handlers                       |
| `kill()`                     | Send signals to processes                      |
| `unlink()`                   | Remove FIFOs                                   |

---

# IPC Summary

The complete communication model can be summarized as follows:

```bash
+-------------+                    +----------------+
|             |   Named FIFO       |                |
|   CLIENT    | -----------------> |     SERVER     |
|             |  fifo_servidor     |                |
+-------------+                    +----------------+
       ^                                    |
       |                                    |
       | Named FIFO                         | fork()
       | fifo_cli_<PID>                     | exec()
       |                                    |
       |                                    v
       |                            +----------------+
       |                            |    VEHICLE     |
       +----------------------------| app_veiculo    |
                                    +----------------+
                                             |
                                             |
                                             | Unnamed Pipe
                                             |
                                             v
                                    +----------------+
                                    |    SERVER      |
                                    | Monitor Thread |
                                    +----------------+

             SERVER
                |
                | SIGUSR1
                v
             VEHICLE
```

---

# IPC Mechanism Summary Table

| Direction         | Mechanism    | Purpose                    |
| ----------------- | ------------ | -------------------------- |
| Client → Server  | Named FIFO   | Requests                   |
| Server → Client  | Named FIFO   | Responses                  |
| Server → Client  | Named FIFO   | Asynchronous notifications |
| Vehicle → Server | Unnamed Pipe | Telemetry                  |
| Server → Vehicle | `SIGUSR1`  | Cancellation               |
| Server internal   | `pthread`  | Concurrent tasks           |
| Client internal   | `select()` | Monitor FIFO + keyboard    |

---

# Example Session

A simplified interaction could look like this.

## Start the Server

```bash
$ ./controlador
```

The server starts its internal tasks and waits for clients.

---

## Connect a Client

```bash
$ ./cliente "Alice"
```

The client connects and creates a private FIFO.

---

## Schedule a Ride

```text
> agendar 15 Aeroporto 20
```

The server registers the ride.

The client receives a response such as:

```text
Operation successful.
Service ID: 1
```

---

## Check Services

```text
> consultar
```

Example:

```text
ID     Destination    Time    Distance    Status
1      Aeroporto      15      20 km       Scheduled
```

---

## Ride Starts

When the simulated clock reaches hour 15, the server dispatches a vehicle.

The client may receive:

```text
INFO: Service 1 has started.
```

The vehicle reports progress:

```text
INFO: Service 1 - 25%
INFO: Service 1 - 50%
INFO: Service 1 - 75%
```

---

## Ride Completes

The vehicle reports:

```text
V_STATUS_CONCLUIDO
```

The client receives:

```text
INFO: Service 1 completed.
```

---

# Graceful Shutdown

When the administrator executes:

```text
terminar
```

the server should cleanly shut down all system resources.

The intended shutdown sequence is:

```bash
                    +----------------+
                    |    terminar    |
                    +----------------+
                            |
                            v
                 Stop accepting requests
                            |
                            v
                    Notify clients
                            |
                            v
                   Cancel/stop vehicles
                            |
                            v
                    Join server threads
                            |
                            v
                   Close file descriptors
                            |
                            v
                     Remove FIFOs
                            |
                            v
                   Destroy synchronization
                            |
                            v
                         exit()
```

This prevents resources such as named FIFOs, file descriptors, and child processes from being left behind.

---

# Error Handling Considerations

A robust implementation should check the return value of system calls and library functions.

For example:

```c
int fd = open("fifo_servidor", O_WRONLY);

if (fd == -1) {
    perror("open");
    return 1;
}
```

Similarly, process creation should be checked:

```c
pid_t pid = fork();

if (pid < 0) {
    perror("fork");
    return 1;
}
```

Thread creation should also be checked:

```c
int result = pthread_create(&thread, NULL, tarefa, NULL);

if (result != 0) {
    fprintf(stderr, "Failed to create thread\n");
}
```

Error handling is particularly important in an IPC-heavy application because failures can otherwise result in:

* Broken communication.
* Deadlocks.
* Orphaned processes.
* Stale FIFOs.
* Inconsistent service state.

---

# Design Goals

The architecture is designed around several key goals.

## Concurrency

Multiple clients and vehicles should be able to operate simultaneously.

## Non-Blocking Communication

Operations such as client input and vehicle monitoring should not unnecessarily block unrelated activities.

## Process Isolation

Each vehicle runs as an independent process.

## Centralized State

The server remains the authoritative source of information about:

* Users.
* Services.
* Vehicles.
* System time.
* Fleet statistics.

## Asynchronous Notifications

Clients can receive important system events without continuously polling.

## Graceful Resource Management

The system should clean up:

* FIFOs.
* File descriptors.
* Threads.
* Child processes.
* Mutexes.

during shutdown.

---

# Summary

Taxi Manager is a concurrent POSIX Client-Server application implemented in C.

Its architecture combines several operating-system concepts:

* **Named FIFOs** for client-server communication.
* **Unnamed pipes** for server-vehicle telemetry.
* **Signals** for vehicle cancellation.
* **`fork()` and `exec()`** for vehicle process creation.
* **Pthreads** for concurrent server tasks.
* **Mutexes** for protecting shared state.
* **`select()`** for responsive client-side I/O.
* **Asynchronous notifications** for communicating service updates.

The overall architecture can be summarized as:

```bash
                         TAXI MANAGER
                              |
             +----------------+----------------+
             |                |                |
             v                v                v
          CLIENTS           SERVER          VEHICLES
             |                |                |
             |                |                |
             +------ FIFO ----+                |
                              |                |
                              +--- fork/exec --+
                              |                |
                              |   SIGUSR1      |
                              +--------------->|
                                               |
                              <--- Pipe --------+
```

The result is a system capable of managing multiple passengers, scheduled rides, and simulated vehicles concurrently while demonstrating practical use of fundamental POSIX operating-system mechanisms.
