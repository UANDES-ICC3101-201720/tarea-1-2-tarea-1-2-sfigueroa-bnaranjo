#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <assert.h>
#include "types.h"
#include "const.h"
#include "util.h"

#define SWAP(x,y) do {\
    __typeof__(x) tmp = x;\
    x = y;\
    y = tmp;\
} while(0)

// TODO: implement
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int partition(UINT *array, int left, int right, int pivot)
{
    int pivotValue = array[pivot];
    SWAP(array[pivot], array[right]);
    int storeIndex = left;
    for (int i=left ; i<right ; i++)
    {
        if (array[i] <= pivotValue)
        {
            SWAP(array[i], array[storeIndex]);
            storeIndex++;
        }
    }
    SWAP(array[storeIndex], array[right]);
    return storeIndex;
}
void quicksort(UINT *array, int left, int right)
{
     if (right > left)
     {
        int pivotIndex = left + (right - left)/2;
        pivotIndex = partition(array, left, right, pivotIndex);
        quicksort(array, left, pivotIndex-1);
        quicksort(array, pivotIndex+1, right);
     }
}

// TODO: implement
struct args {
    UINT *A;
    int lower, higher, number_threads;
};
int parallel_quicksort(UINT* A, int lower, int higher, int number_threads);
void * parallel_sort(void * arg){
    struct args * info = arg;
    parallel_quicksort(info->A, info -> lower, info -> higher, info -> number_threads);
    return NULL;
}

int parallel_quicksort(UINT* A, int lower, int higher, int number_threads) {
		/*Se crea un thread por cada vez que se llama la funcion hasta alcanzar la cantidad de nucleos disponibles*/
		if (higher > 1){
            int pivot = (higher -1)/2 + 1;
            pivot = partition(A, lower, higher, pivot);
            
            if (number_threads > 1){
                struct args arg = {A, 1, pivot-1, number_threads};
                pthread_t t;
                int t_create = pthread_create(&t, NULL, parallel_sort, &arg);
                assert((t_create == 0) && "Thread creation failed");
                number_threads--;
                parallel_quicksort(A, lower, higher, number_threads);
                pthread_join(t, NULL);
            }
            else{
                quicksort(A, 1, pivot-1);
                quicksort(A, pivot, higher);
            }
        }

    return 0;

}

int main(int argc, char** argv) {
    printf("[quicksort] Starting up...\n");

    /* Get the number of CPU cores available */
    printf("[quicksort] Number of cores available: '%ld'\n",
           sysconf(_SC_NPROCESSORS_ONLN));

    /* TODO: parse arguments with getopt */
	int num_exp, num_pot, opt;
	while ((opt = getopt (argc, argv, "E:T:")) != -1)
	{
		switch (opt)
		{
			case 'E':
				num_exp = atoi(optarg);
				if(num_exp < 1) {
					printf("-E value out of range, exiting program\n");
					exit(-1);
				}
				break;
			case 'T':
				num_pot = atoi(optarg);
				if(num_pot < 3 || num_pot > 9){
					printf("-T value out of range, exiting program\n");
					exit(-1);
				}
				break;
			case '?':
				printf("please use -e <number of experiments> -t <exponent of size of array> -p <position to find in array>");
				break;
		}
	}

    /* TODO: start datagen here as a child process. */
	pid_t pid = fork();
	if(pid == -1){
		perror("error al crear fork\n");
		exit(1);
	}
	else if(pid == 0){
		execvp("./datagen", argv);
	}


    /* Create the domain socket to talk to datagen. */
    struct sockaddr_un addr;
    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("[quicksort] Socket error.\n");
        exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DSOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("[quicksort] connect error.\n");
        close(fd);
        exit(-1);
    }


    /* DEMO: request two sets of unsorted random numbers to datagen */
	//Cambiar las veces que lo lanza  a "T" veces (num_exp)
    for (int i = 0; i < num_exp; i++) {
		/* T value 3 hardcoded just for testing. */
		//Entregar un BEGIN U T
        char *begin = "BEGIN U ";
	size_t len = strlen(begin);
	char *begin_n = (char *) malloc(len+1 +1);
	strcpy(begin_n, begin);
	begin_n[len] = num_pot +'0';
	begin_n[len+1] = '\0';
	fprintf(stderr,"sending %s\n",begin_n);
      int rc = strlen(begin_n);

      /* Request the random number stream to datagen */
      if (write(fd, begin_n, strlen(begin_n)) != rc) {
          if (rc > 0) fprintf(stderr, "[quicksort] partial write.\n");
          else {
              perror("[quicksort] write error.\n");
              exit(-1);
          }
      }

      /* validate the response */
      char respbuf[10];
      read(fd, respbuf, strlen(DATAGEN_OK_RESPONSE));
      respbuf[strlen(DATAGEN_OK_RESPONSE)] = '\0';

      if (strcmp(respbuf, DATAGEN_OK_RESPONSE)) {
          perror("[quicksort] Response from datagen failed.\n");
          close(fd);
          exit(-1);
      }

      UINT readvalues = 0;
      size_t numvalues = pow(10, num_pot);
      size_t readbytes = 0;

      UINT *readbuf = malloc(sizeof(UINT) * numvalues);

      while (readvalues < numvalues) {
          /* read the bytestream */
          readbytes = read(fd, readbuf + readvalues, sizeof(UINT) * 1000);
          readvalues += readbytes / 4;
      }

      /* Print out the values obtained from datagen */
			printf("\nE%d: ", i+1);
      for (UINT *pv = readbuf; pv < readbuf + numvalues; pv++) {
            printf("%u,\t ", *pv); // Aquí tendriamos que llamar a los quicksort

			}

			//quicksort(readbuf, 0, numvalues);
            parallel_quicksort(readbuf, 0, numvalues, sysconf(_SC_NPROCESSORS_ONLN));

			printf("\nS%d: ", i+1);
      for (UINT *pv = readbuf; pv < readbuf + numvalues; pv++) {
          printf("%u,\t ", *pv); // Aquí tendriamos que llamar a los quicksort

			}
      free(readbuf);
			printf("\n");
    }


    /* Issue the END command to datagen */
    int rc = strlen(DATAGEN_END_CMD);
    if (write(fd, DATAGEN_END_CMD, strlen(DATAGEN_END_CMD)) != rc) {
        if (rc > 0) fprintf(stderr, "[quicksort] partial write.\n");
        else {
            perror("[quicksort] write error.\n");
            close(fd);
            exit(-1);
        }
    }

    close(fd);
    exit(0);
}
