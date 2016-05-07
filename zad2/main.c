#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

#define RECORD_SIZE 1024

int read_args(int argc, char *argv[], int *threads_num, char **filename,
              int *records_num, char **query, int *mode, int *signal_num);
void *search_in_file_task(void *arg);
void handle_signal(int signal_num);

struct thread {
    pthread_t thread_id;
    int is_joined;
    int is_terminated;
};

int records_num;
char *query;
int input_file;
int threads_num;
struct thread *threads;
int pause_threads = 1;
pthread_mutex_t read_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t buffer_key;
#ifndef VER3
pthread_mutex_t join_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int active_threads;
int threads_to_join_exists = 0;
int found = 0;
#endif
int divide_by_zero = 0;


int main(int argc, char *argv[]) {
    char *args_help = "Enter number of threads, path to input file, number of records to read "
            "in one cycle, a word the file should be searched for, mode and signal "
            "(sigusr1, sigterm, sigkill or sigstop).\n";
    char *filename;
    int mode;
    int signal_num;
    if (read_args(argc, argv, &threads_num, &filename, &records_num, &query, &mode, &signal_num) != 0) {
        printf(args_help);
        return 1;
    }

    input_file = open(filename, O_RDONLY);
    if (input_file == -1) {
        printf("Error while opening file occurred\n");
        return 1;
    }

    threads = malloc(threads_num * sizeof(struct thread));
    if (threads == NULL) {
        printf("Error with memory occurred.\n");
        return 1;
    }

    sigset_t signal_mask;
    sigset_t old_signal_mask;
    if (mode == 4) {
        sigfillset(&signal_mask);
        pthread_sigmask(SIG_SETMASK, &signal_mask, &old_signal_mask);
    }
    else if (mode == 6) {
        divide_by_zero = 1;
    }

    pthread_key_create(&buffer_key, NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);

#ifdef VER3
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#else
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
#endif

    for (int i = 0; i < threads_num; i++) {
        threads[i].is_terminated = 0;
        threads[i].is_joined = 0;
        if (pthread_create(&(threads[i].thread_id), &attr, search_in_file_task, NULL) != 0) {
            printf("Error while creating new thread occurred.\n");
            break;
        }
    }
    pthread_attr_destroy(&attr);

    if (mode == 4) {
        pthread_sigmask(SIG_SETMASK, &old_signal_mask, NULL);
    }

    struct sigaction act;
    act.sa_handler = handle_signal;
    sigemptyset (&signal_mask);
    switch (mode) {
        case 1:
            kill(getpid(), signal_num);
            break;
        case 2:
            if (signal_num != SIGKILL && signal_num != SIGSTOP) {
                sigaddset(&signal_mask, signal_num);
                pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
            }
            kill(getpid(), signal_num);
            break;
        case 3:
            if (signal_num != SIGKILL && signal_num != SIGSTOP)
                sigaction(signal_num, &act, NULL);
            kill(getpid(), signal_num);
            break;
        case 4:
            pthread_kill(threads[0].thread_id, signal_num);
            break;
        case 5:
            if (signal_num != SIGKILL && signal_num != SIGSTOP)
                sigaction(signal_num, &act, NULL);
            pthread_kill(threads[0].thread_id, signal_num);
            break;
    }

    pause_threads = 0; // let all threads start

#ifndef VER3
    active_threads = threads_num;
    while (active_threads > 0) {
        pthread_mutex_lock(&join_mutex);
        while (!threads_to_join_exists) {
            pthread_cond_wait(&cond, &join_mutex);
        }
        for (int i = 0; i < threads_num; i++) {
            if (threads[i].is_terminated && !threads[i].is_joined) {
                pthread_join(threads[i].thread_id, NULL);
                threads[i].is_joined = 1;
                active_threads--;
            }
        }
        threads_to_join_exists = 0;
        pthread_mutex_unlock(&join_mutex);
    }

    close(input_file);
    pthread_key_delete(buffer_key);
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&join_mutex);
    pthread_mutex_destroy(&read_mutex);
#endif
    free(threads);
#ifdef VER3
    pthread_exit(0); // wait for all detached threads
#else
    return 0;
#endif
}

void handle_signal(int signal_num) {
    printf("%d signal received. PID: %d, TID: %ld.\n", signal_num, getpid(), pthread_self());
}

void to_lowercase(char * str) {
    for(int i = 0; str[i]; i++){
        str[i] = (char)tolower(str[i]);
    }
}

int read_args(int argc, char *argv[], int *threads_num, char **filename,
              int *records_num, char **query, int *mode, int *signal_num) {
    if (argc != 7 && argc != 6) {
        printf("Incorrect number of arguments.\n");
        return 1;
    }
    int arg_num = 1;
    *threads_num = atoi(argv[arg_num++]);
    if (*threads_num <= 0) {
        printf("Incorrect number of threads. It should be > 0.\n");
        return 1;
    }
    *filename = argv[arg_num++];
    *records_num = atoi(argv[arg_num++]);
    if (*records_num <= 0) {
        printf("Incorrect number of records. It should be > 0.\n");
        return 1;
    }
    *query = argv[arg_num++];
    *mode = atoi(argv[arg_num++]);
    if (*mode < 1 || *mode > 6) {
        printf("Incorrect mode. It should in range [1,6].\n");
        return 1;
    }
    if (*mode != 6) {
        if (argc != 7) {
            printf("For modes [1,5] signal must be provided.\n");
            return 1;
        }
        to_lowercase(argv[arg_num]);
        if (strcmp(argv[arg_num], "sigusr1") == 0)
            *signal_num = SIGUSR1;
        else if (strcmp(argv[arg_num], "sigterm") == 0)
            *signal_num = SIGTERM;
        else if (strcmp(argv[arg_num], "sigkill") == 0)
            *signal_num = SIGKILL;
        else if (strcmp(argv[arg_num], "sigstop") == 0)
            *signal_num = SIGSTOP;
        else {
            printf("Illegal signal. Correct signals are: sigusr1, sigterm, sigkill and sigstop.\n");
        }
    }

    return 0;
}

void thread_cleanup(void *args) {
    pthread_mutex_unlock(&read_mutex);
    free(pthread_getspecific(buffer_key));

#ifndef VER3
    pthread_mutex_lock(&join_mutex);
    for (int j = 0; j < threads_num; j++) {
        if (pthread_equal(threads[j].thread_id, pthread_self())) {
            threads[j].is_terminated = 1;
        }
    }
    threads_to_join_exists = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&join_mutex);
#endif
}

ssize_t mutex_read(int fd, void *buf, size_t count) {
    ssize_t read_bytes;
    pthread_mutex_lock(&read_mutex);
    read_bytes = read(fd, buf, count);
    pthread_mutex_unlock(&read_mutex);
    return read_bytes;
}

void *search_in_file_task(void *arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    if (divide_by_zero && pthread_equal(threads[0].thread_id, pthread_self())) {
        printf("%d", 100 / 0);
    }
    pthread_cleanup_push(thread_cleanup, NULL) ;
#ifdef VER1
            pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif
#ifdef VER2
            pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
#endif
            size_t buffer_size = (unsigned) records_num * RECORD_SIZE;
            char *buffer = malloc(buffer_size * sizeof(char));
            pthread_setspecific(buffer_key, (void *) buffer);
            if (buffer == NULL) {
                printf("Error with memory occurred.\n");
                return (void *) 1;
            }
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            while (pause_threads);
            ssize_t bytes_read = mutex_read(input_file, (void *) buffer, buffer_size);
            int data_index;
            char *row;
            while (bytes_read > 0) {
                if (bytes_read % RECORD_SIZE != 0) {
                    printf("Error while reading from file occurred.\n");
                    pthread_exit(NULL);
                }
                for (int i = 0; i < bytes_read / RECORD_SIZE; i++) {
                    data_index = 1;
                    row = buffer + i * RECORD_SIZE;
                    row[RECORD_SIZE - 1] = '\0'; //change \n to \0
                    while (row[data_index] != ' ')
                        data_index++;
                    data_index++;
                    if (strstr(row + data_index, query) != NULL) {
#ifndef VER3
                        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
                        pthread_mutex_lock(&join_mutex);
                        if (!found) {
                            found = 1;
#endif
                            int id;
                            sscanf(row, "%d", &id);
                            printf("Thread %ld: found in row id %d\n", pthread_self(), id);
#ifndef VER3
                            for (int j = 0; j < threads_num; j++) {
                                if (!pthread_equal(threads[j].thread_id, pthread_self()) && !threads[j].is_terminated) {
                                    pthread_cancel(threads[j].thread_id);
                                }
                            }
                        }
                        pthread_mutex_unlock(&join_mutex);
                        pthread_exit(NULL);
#endif
                    }
#ifdef VER2
                    pthread_testcancel();
#endif
                }
                bytes_read = mutex_read(input_file, (void *) buffer, buffer_size);
            }
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_cleanup_pop(1);
    return (void *) 0;
}
