CC=gcc
CFLAGS=-I.

all: parallel_min_max zombie process_memory parallel_sum

# Задание 1 (Из 3 лабы с изменениями)
parallel_min_max: utils.o find_min_max.o
	$(CC) -o parallel_min_max utils.o find_min_max.o parallel_min_max.c $(CFLAGS)

find_min_max.o:
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)

utils.o:
	$(CC) -o utils.o -c utils.c $(CFLAGS)

# Задание 2
zombie:
	$(CC) -o zombie zombie.c $(CFLAGS)

# Задание 3
process_memory:
	$(CC) -o process_memory process_memory.c $(CFLAGS)

# Задания 5 и 6 (Библиотека и программа с потоками)
sum.o:
	$(CC) -o sum.o -c sum.c $(CFLAGS)

libsum.a: sum.o
	ar rcs libsum.a sum.o

parallel_sum: utils.o libsum.a
	$(CC) -o psum parallel_sum.c utils.o -L. -lsum -lpthread $(CFLAGS)

clean:
	rm -f *.o *.a parallel_min_max zombie process_memory psum
