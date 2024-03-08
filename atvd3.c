#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> 
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <mpi.h>     

#define THREAD_NUM 3
#define CLOCK_QUEUE_SIZE 10

typedef struct {
    int p[3];
    int idProcess;
} Clock;

pthread_mutex_t saidaMutex;
pthread_cond_t saidaCondEmpty;
pthread_cond_t saidaCondFull;

int saidaClockCount = 0;
Clock saidaClockQueue[CLOCK_QUEUE_SIZE];

pthread_mutex_t entradaMutex;
pthread_cond_t entradaCondEmpty;
pthread_cond_t entradaCondFull;
int entradaClockCount = 0;
Clock entradaClockQueue[CLOCK_QUEUE_SIZE];

void Event(int pid, Clock *clock) {
    clock->p[pid]++;
    printf("Processo: %d, Relógio: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
}

Clock GetClock(pthread_mutex_t *mutex, pthread_cond_t *condEmpty, pthread_cond_t *condFull, int *clockCount, Clock *clockQueue) {
    Clock clock;
    pthread_mutex_lock(mutex);
    
    while (*clockCount == 0) {
        pthread_cond_wait(condEmpty, mutex);
    }

    clock = clockQueue[0];

    for (int i = 0; i < *clockCount - 1; i++) {
        clockQueue[i] = clockQueue[i + 1];
    }

    (*clockCount)--;
    
    pthread_mutex_unlock(mutex);

    pthread_cond_signal(condFull);
    
    return clock;
}

void PutClock(pthread_mutex_t *mutex, pthread_cond_t *condEmpty, pthread_cond_t *condFull, int *clockCount, Clock clock, Clock *clockQueue) {
    pthread_mutex_lock(mutex);

    while (*clockCount == CLOCK_QUEUE_SIZE) {
        pthread_cond_wait(condFull, mutex);
    }
    
    Clock temp = clock;

    clockQueue[*clockCount] = temp;
    (*clockCount)++;
    

    pthread_mutex_unlock(mutex);
    pthread_cond_signal(condEmpty);
}

void SendControl(int id, Clock *clock) {
    Event(id, clock);
    PutClock(&saidaMutex, &saidaCondEmpty, &saidaCondFull, &saidaClockCount, *clock, saidaClockQueue);
}

Clock* ReceiveControl(int id, Clock *clock) {
    Clock* temp = clock;
    Clock clock2 = GetClock(&entradaMutex, &entradaCondEmpty, &entradaCondFull, &entradaClockCount, entradaClockQueue);
    for (int i = 0; i < 3; i++) {
        if (temp->p[i] < clock2.p[i]) {
            temp->p[i] = clock2.p[i];
        }
    }
    temp->p[id]++;
    printf("Processo: %d, Relógio: (%d, %d, %d)\n", id, clock->p[0], clock->p[1], clock->p[2]);
    return temp;
}

void Send(int pid, Clock *clock){
    int mensagem[3];
    mensagem[0] = clock->p[0];
    mensagem[1] = clock->p[1];
    mensagem[2] = clock->p[2];
    //MPI SEND
    MPI_Send(&mensagem, 3, MPI_INT, clock->idProcess, 0, MPI_COMM_WORLD);
}

void Receive(int pid, Clock *clock){
    int mensagem[3];
    //MPI RECV
    MPI_Recv(&mensagem, 3, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    clock->p[0] = mensagem[0];
    clock->p[1] = mensagem[1];
    clock->p[2] = mensagem[2];
}

void *MainThread(void *args) {
    long id = (long) args;
    int pid = (int) id;
    Clock* clock = malloc(sizeof(Clock));
    
    // Inicializando os campos da estrutura Clock
    clock->p[0] = 0;
    clock->p[1] = 0;
    clock->p[2] = 0;
    clock->idProcess = 0;
        
    if (pid == 0) {
        
        Event(pid, clock);
        
      
        clock->idProcess = 1;
        SendControl(pid, clock);
       
        clock = ReceiveControl(pid, clock);
        
      
        clock->idProcess = 2;
        SendControl(pid, clock);
       
        clock = ReceiveControl(pid, clock);
        
        
        clock->idProcess = 1;
        SendControl(pid, clock);
    
       
        Event(pid, clock);
    } else if (pid == 1) {
        
        clock->idProcess = 0;
        SendControl(pid, clock);
        
     
        clock = ReceiveControl(pid, clock);
        

        clock = ReceiveControl(pid, clock);
    } else if (pid == 2) {
    
        Event(pid, clock);
        
    
        clock->idProcess = 0;
        SendControl(pid, clock);
     
        clock = ReceiveControl(pid, clock);
    }

    return NULL;
}

void *SendThread(void *args) {
    long pid = (long) args;
    Clock clock;
    
    while(1){
      clock = GetClock(&saidaMutex, &saidaCondEmpty, &saidaCondFull, &saidaClockCount, saidaClockQueue);
      Send(pid, &clock);
    }

    return NULL;
}

void *ReceiveThread(void *args) {
    long pid = (long) args;
    Clock clock;

    while(1){
      Receive(pid, &clock);
      PutClock(&entradaMutex, &entradaCondEmpty, &entradaCondFull, &entradaClockCount, clock, entradaClockQueue);
    }
 
    return NULL;
}

// Representa o processo de rank 0
void process0(){
   pthread_t thread[THREAD_NUM];
   pthread_create(&thread[0], NULL, &MainThread, (void*) 0);
   pthread_create(&thread[1], NULL, &SendThread, (void*) 0);
   pthread_create(&thread[2], NULL, &ReceiveThread, (void*) 0);

   for (int i = 0; i < THREAD_NUM; i++){  
      if (pthread_join(thread[i], NULL) != 0) {
         perror("Falha ao juntar a thread");
      }
   }
}

// Representa o processo de rank 1
void process1(){
   pthread_t thread[THREAD_NUM];
   pthread_create(&thread[0], NULL, &MainThread, (void*) 1);
   pthread_create(&thread[1], NULL, &SendThread, (void*) 1);
   pthread_create(&thread[2], NULL, &ReceiveThread, (void*) 1);
   
   for (int i = 0; i < THREAD_NUM; i++){  
      if (pthread_join(thread[i], NULL) != 0) {
         perror("Falha ao juntar a thread");
      }
   }
}

// Representa o processo de rank 2
void process2(){
   pthread_t thread[THREAD_NUM];
   pthread_create(&thread[0], NULL, &MainThread, (void*) 2);
   pthread_create(&thread[1], NULL, &SendThread, (void*) 2);
   pthread_create(&thread[2], NULL, &ReceiveThread, (void*) 2);
   
   for (int i = 0; i < THREAD_NUM; i++){  
      if (pthread_join(thread[i], NULL) != 0) {
         perror("Falha ao juntar a thread");
      }
   }
}

int main(int argc, char* argv[]) {
   int my_rank;
   
   pthread_mutex_init(&entradaMutex, NULL);
   pthread_mutex_init(&saidaMutex, NULL);
   pthread_cond_init(&entradaCondEmpty, NULL);
   pthread_cond_init(&saidaCondEmpty, NULL);
   pthread_cond_init(&entradaCondFull, NULL);
   pthread_cond_init(&saidaCondFull, NULL);
  
   
   MPI_Init(NULL, NULL); 
   MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); 

   if (my_rank == 0) { 
      process0();
   } else if (my_rank == 1) {  
      process1();
   } else if (my_rank == 2) {  
      process2();
   }

   /* Finaliza MPI */
   
   
   pthread_mutex_destroy(&entradaMutex);
   pthread_mutex_destroy(&saidaMutex);
   pthread_cond_destroy(&entradaCondEmpty);
   pthread_cond_destroy(&saidaCondEmpty);
   pthread_cond_destroy(&entradaCondFull);
   pthread_cond_destroy(&saidaCondFull);
   
   MPI_Finalize();

   return 0;
} 
