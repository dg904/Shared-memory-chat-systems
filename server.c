#include "common.h"

int          shm_id   = -1;
SharedChat  *shared   = NULL;
sem_t       *mutex_sem = NULL;
sem_t       *msg_sem   = NULL;

/* Graceful shutdown: free all IPC resources */
void cleanup(int sig) {
    (void)sig;  /* suppress unused parameter warning */

    static volatile int cleaning = 0;  /* prevent recursive cleanup */
    if (cleaning) return;
    cleaning = 1;

    printf("\n[Server] Shutting down...\n");

    if (shared && shared != (void *)-1) {
        shared->server_running = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (shared->clients[i].active)
                kill(shared->clients[i].pid, SIGUSR1);  /* notify clients */
        }
        shmdt(shared);  /* detach shared memory */
        shared = NULL;
    }

    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);  /* remove shared memory */
        shm_id = -1;
    }

    if (mutex_sem && mutex_sem != SEM_FAILED) {
        sem_close(mutex_sem);
        sem_unlink(SEM_MUTEX_NAME);
        mutex_sem = NULL;
    }
    if (msg_sem && msg_sem != SEM_FAILED) {
        sem_close(msg_sem);
        sem_unlink(SEM_MSG_NAME);
        msg_sem = NULL;
    }

    printf("[Server] Cleanup complete.\n");
    exit(0);
}

/* Print all connected clients */
void list_clients(void) {
    sem_wait(mutex_sem);
    printf("\n--- Connected Clients ---\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shared->clients[i].active)
            printf("  %s (PID: %d)\n", shared->clients[i].name,
                   shared->clients[i].pid);
    }
    printf("  Total: %d\n---\n", shared->client_count);
    sem_post(mutex_sem);
}

/* Print all active messages */
void list_messages(void) {
    sem_wait(mutex_sem);
    printf("\n--- Messages ---\n");
    for (int i = 0; i < MAX_MESSAGES; i++) {
        if (shared->messages[i].active) {
            char tbuf[20];
            strftime(tbuf, sizeof(tbuf), "%H:%M:%S",
                     localtime(&shared->messages[i].timestamp));
            printf("  [%s] %s: %s\n", tbuf,
                   shared->messages[i].sender_name,
                   shared->messages[i].content);
        }
    }
    printf("---\n");
    sem_post(mutex_sem);
}

/* Remove clients whose processes no longer exist */
void check_clients(void) {
    sem_wait(mutex_sem);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shared->clients[i].active) {
            if (kill(shared->clients[i].pid, 0) == -1) {  /* process dead? */
                printf("[Server] Client '%s' (PID %d) disconnected.\n",
                       shared->clients[i].name, shared->clients[i].pid);
                shared->clients[i].active = 0;
                shared->client_count--;
            }
        }
    }
    sem_post(mutex_sem);
}

int main(void) {
    printf("[Server] Starting...\n");

    signal(SIGINT,  cleanup);  /* Ctrl+C handler */ 
    signal(SIGTERM, cleanup);

    /* Create shared memory */
    shm_id = shmget(SHM_KEY, sizeof(SharedChat), IPC_CREAT | 0666);
    if (shm_id == -1) { perror("shmget"); exit(1); }

    /* Attach shared memory */
    shared = (SharedChat *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        shared = NULL;  /* reset so cleanup doesn't dereference it */
        perror("shmat");
        cleanup(0);
    }

    /* Initialize shared memory to zeros */
    memset(shared, 0, sizeof(SharedChat));
    shared->server_running = 1;

    /* Create named semaphores */
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_MSG_NAME);

    mutex_sem = sem_open(SEM_MUTEX_NAME, O_CREAT, 0666, 1);
    if (mutex_sem == SEM_FAILED) { perror("sem_open mutex"); cleanup(0); }

    msg_sem = sem_open(SEM_MSG_NAME, O_CREAT, 0666, 0);
    if (msg_sem == SEM_FAILED) { perror("sem_open msg"); cleanup(0); }

    printf("[Server] Ready. Commands: list, msgs, check, quit\n");

    /* Command loop */
    char cmd[64];
    while (1) {
        printf("server> ");
        fflush(stdout);

        if (fgets(cmd, sizeof(cmd), stdin) == NULL) break;
        cmd[strcspn(cmd, "\n")] = '\0';

        if      (strcmp(cmd, "list")  == 0) list_clients();
        else if (strcmp(cmd, "msgs")  == 0) list_messages();
        else if (strcmp(cmd, "check") == 0) { check_clients(); printf("Done.\n"); }
        else if (strcmp(cmd, "quit")  == 0) cleanup(0);
        else if (strlen(cmd) > 0)
            printf("Unknown command. Use: list, msgs, check, quit\n");
    }

    cleanup(0);
    return 0;
}