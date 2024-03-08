#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <pthread.h>
#include <unistd.h>

#define QUEUE_SIZE 10

// mpicc -o atvd3 atvd3.c -lpthread
// mpiexec -n 3 ./at3

typedef struct {
    int p[3];
    int idProcesso;
} Clock;

typedef struct _Fila {
    Clock queue[QUEUE_SIZE];
    int start, end;
    pthread_mutex_t mutex;
    pthread_cond_t is_full, is_empty;
} Fila;

Fila receiveFila, sendFila;
Clock global_clock = {{0, 0, 0}, 0};

Fila create_fila() {
    Fila q;
    q.start = 0;
    q.end = 0;
    pthread_mutex_init(&q.mutex, NULL);
    pthread_cond_init(&q.is_full, NULL);
    pthread_cond_init(&q.is_empty, NULL);
    return q;
}

int add_to_fila(Fila *q, Clock c) {
    pthread_mutex_lock(&q->mutex);
    while ((q->end + 1) % QUEUE_SIZE == q->start % QUEUE_SIZE) {
        pthread_cond_wait(&q->is_full, &q->mutex);
    }
    q->queue[q->end % QUEUE_SIZE] = c;
    q->end++;
    pthread_cond_signal(&q->is_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

Clock remove_from_fila(Fila *q) {
    pthread_mutex_lock(&q->mutex);
    while (q->start == q->end) {
        pthread_cond_wait(&q->is_empty, &q->mutex);
    }
    Clock c = q->queue[q->start % QUEUE_SIZE];
    q->start++;
    pthread_cond_signal(&q->is_full);
    pthread_mutex_unlock(&q->mutex);
    return c;
}

void* receive_thread(void* arg) {
    Clock c;
    while (1) {
        MPI_Recv(&c, sizeof(Clock) / sizeof(int), MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        add_to_fila(&receiveFila, c);
    }
    return NULL;
}

void* send_thread(void* arg) {
    Clock c;
    while (1) {
        c = remove_from_fila(&sendFila);
        int target = c.idProcesso;
        global_clock.idProcesso = c.idProcesso;
        MPI_Send(&c, sizeof(Clock) / sizeof(int), MPI_INT, target, 0, MPI_COMM_WORLD);
        printf("Processo: %d, Clock: (%d, %d, %d)\n", global_clock.idProcesso, global_clock.p[0], global_clock.p[1], global_clock.p[2]);
    }
    return NULL;
}

void Event(int rank) {
    global_clock.p[rank]++;
    printf("Processo: %d, Clock: (%d, %d, %d)\n", global_clock.idProcesso, global_clock.p[0], global_clock.p[1], global_clock.p[2]);
}

void Send(int rank, int target) {
    global_clock.p[rank]++;
    Clock c = global_clock;
    c.idProcesso = target;
    add_to_fila(&sendFila, c);
}

void Receive(int rank) {
    Clock c = remove_from_fila(&receiveFila);
    global_clock.p[rank]++;
    for (int i = 0; i < 3; i++) {
        if (global_clock.p[i] < c.p[i])  global_clock.p[i] = c.p[i];
    }
    printf("Processo: %d, Clock: (%d, %d, %d)\n", global_clock.idProcesso, global_clock.p[0], global_clock.p[1], global_clock.p[2]);
}

void processo0() {
    Event(0);
    Send(0, 1);
    Receive(0);
    Send(0, 2);
    Receive(0);
    Send(0, 1);
    Event(0);
}

void processo1() {
    Send(1, 0);
    Receive(1);
    Receive(1);
}

void processo2() {
    Event(2);
    Send(2, 0);
    Receive(2);
}

int main(int argc, char **argv) {
    int my_rank;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    global_clock.idProcesso = my_rank;

    receiveFila = create_fila();
    sendFila = create_fila();

    pthread_t threadRecv, threadSend;
    pthread_create(&threadRecv, NULL, receive_thread, NULL);
    pthread_create(&threadSend, NULL, send_thread, NULL);

    switch (my_rank) {
        case 0:
            processo0();
            break;
        case 1:
            processo1();
            break;
        case 2:
            processo2();
            break;
        default:
            printf("Rank inválido %d", my_rank);
            exit(1);
    }

    pthread_join(threadRecv, NULL);
    pthread_join(threadSend, NULL);

    MPI_Finalize();
    return 0;
}
