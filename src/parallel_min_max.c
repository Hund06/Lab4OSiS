#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <signal.h>

#include "find_min_max.h"
#include "utils.h"

// Глобальные переменные для доступа из обработчика сигналов
pid_t *child_pids;
int pnum_global = 0;

// Обработчик сигнала SIGALRM, вызываемый при истечении таймаута
void handle_alarm(int sig) {
    printf("Таймаут исчерпан! Посылаем SIGKILL дочерним процессам.\n");
    for (int i = 0; i < pnum_global; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGKILL);
        }
    }
}

int main(int argc, char **argv) {
    int seed = -1;
    int array_size = -1;
    int pnum = -1;
    int timeout = -1;
    bool with_files = false;

    while (true) {
        int current_optind = optind ? optind : 1;

        static struct option options[] = {
            {"seed", required_argument, 0, 0},
            {"array_size", required_argument, 0, 0},
            {"pnum", required_argument, 0, 0},
            {"by_files", no_argument, 0, 'f'},
            {"timeout", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "f", options, &option_index);

        if (c == -1) break;

        switch (c) {
            case 0:
                switch (option_index) {
                    case 0: seed = atoi(optarg); break;
                    case 1: array_size = atoi(optarg); break;
                    case 2: pnum = atoi(optarg); pnum_global = pnum; break;
                    case 3: with_files = true; break;
                    case 4: timeout = atoi(optarg); break;
                }
                break;
            case 'f':
                with_files = true;
                break;
            case '?':
                break;
        }
    }

    if (seed == -1 || array_size == -1 || pnum == -1) {
        printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--timeout \"num\"]\n", argv[0]);
        return 1;
    }

    int *array = malloc(sizeof(int) * array_size);
    GenerateArray(array, array_size, seed);
    int active_child_processes = 0;
    
    child_pids = malloc(sizeof(pid_t) * pnum);

    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    int pipefd[2];
    if (!with_files) {
        if (pipe(pipefd) == -1) {
            perror("Pipe failed");
            return 1;
        }
    }

    // Регистрируем обработчик и заводим таймер
    if (timeout > 0) {
        signal(SIGALRM, handle_alarm);
        alarm(timeout);
    }

    for (int i = 0; i < pnum; i++) {
        pid_t child_pid = fork();
        if (child_pid >= 0) {
            child_pids[i] = child_pid;
            active_child_processes += 1;
            
            if (child_pid == 0) {
                unsigned int step = array_size / pnum;
                unsigned int begin = i * step;
                unsigned int end = (i == pnum - 1) ? array_size : (i + 1) * step;

                // Искусственная задержка для демонстрации работы таймаута
                usleep(5000000); 

                struct MinMax current_min_max = GetMinMax(array, begin, end);

                if (with_files) {
                    char filename[256];
                    sprintf(filename, "temp_result_%d.bin", i);
                    FILE *f = fopen(filename, "wb");
                    fwrite(&current_min_max, sizeof(struct MinMax), 1, f);
                    fclose(f);
                } else {
                    write(pipefd[1], &current_min_max, sizeof(struct MinMax));
                }
                return 0;
            }
        } else {
            printf("Fork failed!\n");
            return 1;
        }
    }

    // Неблокирующее ожидание завершения процессов
    while (active_child_processes > 0) {
        int status;
        pid_t p = waitpid(-1, &status, WNOHANG);
        
        if (p > 0) {
            active_child_processes -= 1;
        } else if (p == 0) {
            usleep(10000); 
        } else {
            continue; 
        }
    }

    // Отключаем таймер при успешном завершении дочерних процессов
    if (timeout > 0) {
        alarm(0);
    }

    // Закрываем дескриптор на запись, чтобы предотвратить блокировку функции read
    if (!with_files) {
        close(pipefd[1]);
    }

    struct MinMax min_max;
    min_max.min = INT_MAX;
    min_max.max = INT_MIN;

    for (int i = 0; i < pnum; i++) {
        struct MinMax current_min_max;

        if (with_files) {
            char filename[256];
            sprintf(filename, "temp_result_%d.bin", i);
            FILE *f = fopen(filename, "rb");
            if (f != NULL) {
                fread(&current_min_max, sizeof(struct MinMax), 1, f);
                fclose(f);
                remove(filename);
            }
        } else {
            if (read(pipefd[0], &current_min_max, sizeof(struct MinMax)) <= 0) {
                continue; 
            }
        }

        if (current_min_max.min < min_max.min) min_max.min = current_min_max.min;
        if (current_min_max.max > min_max.max) min_max.max = current_min_max.max;
    }

    if (!with_files) {
        close(pipefd[0]);
    }

    struct timeval finish_time;
    gettimeofday(&finish_time, NULL);
    double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
    elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

    free(array);
    free(child_pids);

    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
    printf("Elapsed time: %fms\n", elapsed_time);
    return 0;
}