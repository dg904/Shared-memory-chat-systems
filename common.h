#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MAX_CLIENTS     10
#define MAX_MESSAGES    50
#define MAX_MSG_LEN     256
#define MAX_NAME_LEN    32
#define SHM_KEY         0x1234
#define SEM_MUTEX_NAME  "/chat_mutex"
#define SEM_MSG_NAME    "/chat_msg"

/* Single message in the chat buffer */
typedef struct {
    int     active;
    pid_t   sender_pid;
    char    sender_name[MAX_NAME_LEN];
    char    content[MAX_MSG_LEN];
    time_t  timestamp;
} Message;

/* Information about a connected client */
typedef struct {
    pid_t   pid;
    char    name[MAX_NAME_LEN];
    int     active;
} ClientInfo;

/* Entire shared memory layout */
typedef struct {
    int         server_running;
    int         client_count;
    int         message_count;
    int         msg_write_idx;
    ClientInfo  clients[MAX_CLIENTS];
    Message     messages[MAX_MESSAGES];
} SharedChat;

#endif