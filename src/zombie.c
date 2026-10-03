#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    pid_t child_pid = fork();

    if (child_pid > 0) {
        // Мы в родительском процессе.
        printf("Родительский процесс (PID: %d). Дочерний процесс (PID: %d) сейчас станет зомби.\n", getpid(), child_pid);
        printf("Откройте другой терминал и введите: ps -aux | grep Z\n");
        printf("Родитель засыпает на 20 секунд, чтобы зомби был жив...\n");
        
        // Родитель спит, не вызывая wait(). В это время ребенок - зомби.
        sleep(20); 
        
        printf("Родитель проснулся и завершается. Зомби будет убит системой (init).\n");
    } else if (child_pid == 0) {
        // Мы в дочернем процессе. Сразу же завершаем его работу.
        printf("Дочерний процесс завершен.\n");
        exit(0);
    } else {
        perror("Ошибка при создании процесса");
        return 1;
    }

    return 0;
}