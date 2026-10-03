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
#include <errno.h>

#include "find_min_max.h"
#include "utils.h"

// Глобальные переменные для доступа из обработчика системного сигнала
pid_t *child_pids;
int pnum_global = 0;

// Атомарный флаг для безопасного изменения внутри обработчика прерываний
volatile sig_atomic_t timeout_expired = 0;

// Обработчик сигнала SIGALRM, вызываемый при истечении заданного таймаута
void handle_alarm(int sig) {
    timeout_expired = 1;
    printf("Таймаут исчерпан! Посылаем SIGKILL дочерним процессам.\n");

    for (int i = 0; i < pnum_global; i++) {
        // Завершаем только те процессы, которые были успешно созданы
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

    // Парсинг аргументов командной строки
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

        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                switch (option_index) {
                    case 0: seed = atoi(optarg); break;
                    case 1: array_size = atoi(optarg); break;
                    case 2: 
                        pnum = atoi(optarg);
                        pnum_global = pnum; // Сохраняем глобально для обработчика
                        break;
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
        printf(
            "Usage: %s --seed \"num\" --array_size \"num\" "
            "--pnum \"num\" [--timeout \"num\"]\n",
            argv[0]
        );
        return 1;
    }

    int *array = malloc(sizeof(int) * array_size);
    GenerateArray(array, array_size, seed);

    int active_child_processes = 0;
    child_pids = malloc(sizeof(pid_t) * pnum);

    for (int i = 0; i < pnum; i++) {
        child_pids[i] = -1;
    }

    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    // Инициализация канала связи, если не используются файлы
    int pipefd[2];
    if (!with_files) {
        if (pipe(pipefd) == -1) {
            perror("Pipe failed");
            free(array);
            free(child_pids);
            return 1;
        }
    }

    // Порождение рабочих процессов
    for (int i = 0; i < pnum; i++) {
        pid_t child_pid = fork();

        if (child_pid >= 0) {
            child_pids[i] = child_pid;
            active_child_processes += 1;

            // Блок кода, выполняемый исключительно в дочернем процессе
            if (child_pid == 0) {
                unsigned int step = array_size / pnum;
                unsigned int begin = i * step;
                unsigned int end = (i == pnum - 1) ? array_size : (i + 1) * step;

                struct MinMax current_min_max = GetMinMax(array, begin, end);

                // Запись локальных результатов работы процесса
                if (with_files) {
                    char filename[256];
                    sprintf(filename, "temp_result_%d.bin", i);
                    FILE *f = fopen(filename, "wb");

                    if (f != NULL) {
                        fwrite(&current_min_max, sizeof(struct MinMax), 1, f);
                        fclose(f);
                    }
                } else {
                    write(pipefd[1], &current_min_max, sizeof(struct MinMax));
                }

                return 0;
            }
        } else {
            printf("Fork failed!\n");

            if (!with_files) {
                close(pipefd[0]);
                close(pipefd[1]);
            }

            free(array);
            free(child_pids);
            return 1;
        }
    }

    // Установка системного таймера, если задан аргумент timeout
    if (timeout > 0) {
        signal(SIGALRM, handle_alarm);
        alarm(timeout);
    }

    // Неблокирующий цикл ожидания завершения дочерних процессов
    while (active_child_processes > 0) {
        int status;
        pid_t p = waitpid(-1, &status, WNOHANG);

        if (p > 0) {
            active_child_processes -= 1;
        } else if (p == 0) {
            usleep(10000);
        } else {
            // Игнорируем прерывания от системных сигналов (например, SIGALRM)
            if (errno == EINTR) {
                continue;
            }
            break;
        }
    }

    // Деактивация таймера при естественном завершении расчетов
    if (timeout > 0) {
        alarm(0);
    }

    // Закрываем дескриптор записи, чтобы предотвратить бесконечное ожидание функции read
    if (!with_files) {
        close(pipefd[1]);
    }

    struct MinMax min_max;
    min_max.min = INT_MAX;
    min_max.max = INT_MIN;

    // Сбор результатов от отработавших процессов
    for (int i = 0; i < pnum; i++) {
        struct MinMax current_min_max;
        bool result_received = false;

        if (with_files) {
            char filename[256];
            sprintf(filename, "temp_result_%d.bin", i);
            FILE *f = fopen(filename, "rb");

            if (f != NULL) {
                if (fread(&current_min_max, sizeof(struct MinMax), 1, f) == 1) {
                    result_received = true;
                }
                fclose(f);
                remove(filename);
            }
        } else {
            // Проверка корректности считывания полной структуры из трубы
            if (read(pipefd[0], &current_min_max, sizeof(struct MinMax)) == sizeof(struct MinMax)) {
                result_received = true;
            }
        }

        // Обновление глобального экстремума только при успешном получении данных
        if (result_received) {
            if (current_min_max.min < min_max.min) {
                min_max.min = current_min_max.min;
            }
            if (current_min_max.max > min_max.max) {
                min_max.max = current_min_max.max;
            }
        }
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

    // Вывод информации в зависимости от того, сработал ли таймаут
    if (timeout_expired) {
        printf("Вычисление было прервано по таймауту.\n");
    } else {
        printf("Min: %d\n", min_max.min);
        printf("Max: %d\n", min_max.max);
    }

    printf("Elapsed time: %fms\n", elapsed_time);

    return 0;
}