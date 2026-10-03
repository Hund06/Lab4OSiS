#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/time.h>

#include "utils.h"
#include "sum.h"

// Обертка для запуска функции Sum внутри потока
void *ThreadSum(void *args) {
    struct SumArgs *sum_args = (struct SumArgs *)args;
    // Подсчитываем сумму и возвращаем ее через приведение к void*
    return (void *)(size_t)Sum(sum_args);
}

int main(int argc, char **argv) {
    uint32_t threads_num = 0;
    uint32_t array_size = 0;
    uint32_t seed = 0;

    // Чтение аргументов командной строки
    while (true) {
        int current_optind = optind ? optind : 1;
        static struct option options[] = {
            {"threads_num", required_argument, 0, 0},
            {"seed", required_argument, 0, 0},
            {"array_size", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "", options, &option_index);

        if (c == -1) break;

        switch (c) {
            case 0:
                switch (option_index) {
                    case 0: threads_num = atoi(optarg); break;
                    case 1: seed = atoi(optarg); break;
                    case 2: array_size = atoi(optarg); break;
                }
                break;
            case '?':
                break;
        }
    }

    if (threads_num == 0 || array_size == 0 || seed == 0) {
        printf("Usage: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n", argv[0]);
        return 1;
    }

    pthread_t threads[threads_num];
    struct SumArgs args[threads_num];

    // Выделение памяти и генерация массива
    int *array = malloc(sizeof(int) * array_size);
    GenerateArray(array, array_size, seed);

    // Запускаем таймер ДО начала работы потоков (согласно условию)
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    // Распределяем отрезки массива между потоками
    unsigned int step = array_size / threads_num;
    for (uint32_t i = 0; i < threads_num; i++) {
        args[i].array = array;
        args[i].begin = i * step;
        args[i].end = (i == threads_num - 1) ? array_size : (i + 1) * step;

        // Создаем поток, передавая ему аргументы через адрес структуры
        if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
            printf("Error: pthread_create failed!\n");
            return 1;
        }
    }

    int total_sum = 0;
    // Ожидаем завершения всех потоков и собираем сумму
    for (uint32_t i = 0; i < threads_num; i++) {
        int sum = 0;
        pthread_join(threads[i], (void **)&sum);
        total_sum += sum;
    }

    // Останавливаем таймер
    struct timeval finish_time;
    gettimeofday(&finish_time, NULL);
    double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
    elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

    free(array);

    printf("Total: %d\n", total_sum);
    printf("Elapsed time: %fms\n", elapsed_time);
    
    return 0;
}