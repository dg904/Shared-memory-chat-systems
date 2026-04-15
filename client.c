#include "common.h"
#include <sys/wait.h>

SharedChat  *shared    = NULL;
sem_t       *mutex_sem = NULL;
sem_t       *msg_sem   = NULL;
int          my_slot   = -1;
volatile int running   = 1;
pid_t        my_pid    = -1;

/* Called when server sends SIGUSR1 (shutdown notification) */
void handle_server_shutdown(int sig) {
    (void)sig;
    running = 0;
}

/* Clean disconnect: remove from client list, detach, close semaphores */
void client_cleanup(int sig) {
    (void)sig;

    static volatile int cleaning = 0;
    if (cleaning) return;
    cleaning = 1;

    if (shared && shared != (void *)-1 && my_slot >= 0) {
        if (sem_trywait(mutex_sem) == 0) {
            shared->clients[my_slot].active = 0;
            shared->client_count--;
            sem_post(mutex_sem);
        }
    }

    if (shared && shared != (void *)-1) {
        shmdt(shared);
        shared = NULL;
    }

    if (mutex_sem && mutex_sem != SEM_FAILED) {
        sem_close(mutex_sem);
        mutex_sem = NULL;
    }
    if (msg_sem && msg_sem != SEM_FAILED) {
        sem_close(msg_sem);
        msg_sem = NULL;
    }

    printf("[Client] Disconnected.\n");
    exit(0);
}

/* Write a message into the circular buffer in shared memory */
void send_message(const char *name, const char *content) {
    sem_wait(mutex_sem);  /* enter critical section */

    int idx = shared->msg_write_idx;
    shared->messages[idx].active     = 1;
    shared->messages[idx].sender_pid = my_pid;
    strncpy(shared->messages[idx].sender_name, name,    MAX_NAME_LEN - 1);
    strncpy(shared->messages[idx].content,     content, MAX_MSG_LEN  - 1);
    shared->messages[idx].timestamp  = time(NULL);
    shared->msg_write_idx = (idx + 1) % MAX_MESSAGES;
    shared->message_count++;

    /* Post once per active client so ALL readers wake up */
    int notify_count = shared->client_count;

    sem_post(mutex_sem);  /* leave critical section */

    for (int i = 0; i < notify_count; i++)
        sem_post(msg_sem);
}

/* Child process: continuously watches for new messages */
void reader_process(const char *my_name) {
    int last_read_idx = shared->msg_write_idx;
    (void)my_name;

    while (running && shared->server_running) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        /* Wait for hint or timeout --- either way, check for messages */
        sem_timedwait(msg_sem, &ts);

        /* Read ALL pending messages */
        sem_wait(mutex_sem);
        while (last_read_idx != shared->msg_write_idx) {
            if (shared->messages[last_read_idx].active) {
                Message *msg = &shared->messages[last_read_idx];

                if (msg->sender_pid != my_pid) {
                    char tbuf[20];
                    strftime(tbuf, sizeof(tbuf), "%H:%M:%S",
                             localtime(&msg->timestamp));
                    printf("\r\033[K[%s] %s: %s\n",
                           tbuf, msg->sender_name, msg->content);
                    printf("you> ");
                    fflush(stdout);
                }
            }
            last_read_idx = (last_read_idx + 1) % MAX_MESSAGES;
        }
        sem_post(mutex_sem);
    }
}

int main(void) {
    char username[MAX_NAME_LEN];

    printf("Enter your username: ");
    fflush(stdout);
    if (fgets(username, sizeof(username), stdin) == NULL) exit(1);
    username[strcspn(username, "\n")] = '\0';
    if (strlen(username) == 0) {
        printf("Username cannot be empty.\n");
        exit(1);
    }

    my_pid = getpid();

    signal(SIGINT,  client_cleanup);
    signal(SIGUSR1, handle_server_shutdown);

    int shm_id = shmget(SHM_KEY, sizeof(SharedChat), 0666);
    if (shm_id == -1) {
        perror("shmget: is the server running?");
        exit(1);
    }

    shared = (SharedChat *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        shared = NULL;
        perror("shmat");
        exit(1);
    }

    if (!shared->server_running) {
        printf("Server is not running.\n");
        shmdt(shared);
        shared = NULL;
        exit(1);
    }

    mutex_sem = sem_open(SEM_MUTEX_NAME, 0);
    if (mutex_sem == SEM_FAILED) {
        perror("sem_open mutex");
        shmdt(shared);
        shared = NULL;
        exit(1);
    }

    msg_sem = sem_open(SEM_MSG_NAME, 0);
    if (msg_sem == SEM_FAILED) {
        perror("sem_open msg");
        sem_close(mutex_sem);
        shmdt(shared);
        shared = NULL;
        exit(1);
    }

    sem_wait(mutex_sem);
    if (shared->client_count >= MAX_CLIENTS) {
        sem_post(mutex_sem);
        printf("Server full.\n");
        shmdt(shared);
        shared = NULL;
        exit(1);
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!shared->clients[i].active) {
            shared->clients[i].pid = my_pid;
            strncpy(shared->clients[i].name, username, MAX_NAME_LEN - 1);
            shared->clients[i].active = 1;
            my_slot = i;
            break;
        }
    }
    shared->client_count++;
    sem_post(mutex_sem);

    printf("[Client] Joined as '%s'. Commands: /list /quit /help\n\n", username);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        client_cleanup(0);
    }

    if (pid == 0) {
        reader_process(username);
        exit(0);
    } else {
        char input[MAX_MSG_LEN];

        while (running && shared->server_running) {
            printf("you> ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin) == NULL) break;
            input[strcspn(input, "\n")] = '\0';
            if (strlen(input) == 0) continue;

            if (strcmp(input, "/quit") == 0) break;

            if (strcmp(input, "/list") == 0) {
                sem_wait(mutex_sem);
                printf("\n-- Online --\n");
                for (int i = 0; i < MAX_CLIENTS; i++)
                    if (shared->clients[i].active)
                        printf("  %s (PID %d)%s\n",
                               shared->clients[i].name,
                               shared->clients[i].pid,
                               shared->clients[i].pid == my_pid ? " (you)" : "");
                printf("--\n\n");
                sem_post(mutex_sem);
                continue;
            }

            if (strcmp(input, "/help") == 0) {
                printf("/list - online users\n/quit - disconnect\n/help - this\n");
                continue;
            }

            send_message(username, input);
        }

        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        client_cleanup(0);
    }

    return 0;
}