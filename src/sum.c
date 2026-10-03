#include "sum.h"

// Реализация функции подсчета суммы участка массива
int Sum(const struct SumArgs *args) {
    int sum = 0;
    // Проходим по заданному отрезку и суммируем элементы
    for (int i = args->begin; i < args->end; i++) {
        sum += args->array[i];
    }
    return sum;
}