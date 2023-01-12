#include "logging.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../fs/operations.h"
#include "../utils/lib.h"
#include "producer-consumer.h"



#define MAX_SESSIONS 128
#define MAX_BOXES 1024

int number_max_sessions;
int current_boxes = 0;

mail_box boxes[MAX_BOXES];
pc_queue_t *task_queue;


pthread_cond_t thread_cond;
pthread_mutex_t mutex;
pthread_t tid[MAX_SESSIONS];
session sessions[MAX_SESSIONS];

typedef struct {
	pthread_t thread;
    bool available;
} session;


int init_server() {
    if (tfs_init(NULL) == -1) {
		return -1;
	}
	task_queue = malloc(sizeof(pc_queue_t));

	return 0;
}

int inicialize_threads(){
	pthread_t threads[number_max_sessions];
    int i;

    for (i = 0; i < number_max_sessions; i++) {
        pthread_create(&threads[i], NULL, task_processor, NULL);
    }
}

void *task_processor(void *arg) {
    while (1) {
        task *task_op = (task *)pcq_dequeue(&task_queue);
        switch (task_op->opcode) {
            case OP_CODE_LOGIN_PUB:
                // handle create request
                break;
            case OP_CODE_LOGIN_SUB:
                // handle read request
                break;
            case 'U':
                // handle update request
                break;
            case 'D':
                // handle delete request
                break;
            default:
                // handle invalid opcode
                break;
        }
    }
	return NULL;
}



//perguntaro ao professor a questão das sessões
/*
  Não podemos mandar tid como parametro porque vão ser as threads dos clientes a fzr register session*/

int main(int argc, char **argv) {


	/* Ignore sigpipe */
	signal(SIGPIPE, SIG_IGN);

	/* Initialize server */
	if (init_server() == -1) {
		exit(EXIT_FAILURE);
	}



    
    if (argc < 3) {
		printf("You need to specify the pathname of the server's pipe.\n");
		exit(EXIT_FAILURE);
	}

    /* Get server pipe name from command line */
    char *pipe_name = argv[1];
	printf("Starting TecnicoFS server with pipe called %s\n", pipe_name);

    fprintf(stderr, "usage: mbroker %s\n", pipe_name);
	/*Get the maximum sessions argument*/
	number_max_sessions = atoi(argv[2]);
	printf("the number of max_sessions has been defined to %d\n", number_max_sessions);



    /* Unlink and create server pipe */
	if (unlink(pipe_name) == -1 && errno != ENOENT) {
		exit(EXIT_FAILURE);
	}

	if (mkfifo(pipe_name, 0777) == -1) {
		exit(EXIT_FAILURE);
	}

    /*
    int id[number_max__sessions];

    for(int i = 0; i< number_max__sessions; i++){
        pthread_create(&sessions[i].thread, NULL,task_session, &id[i]);
    }
    */

    /* Open server pipe */
    int server_pipe; 
    ssize_t server_read;
	if ((server_pipe = open(pipe_name, O_RDONLY)) == -1) {
		exit(EXIT_FAILURE);
	}

    /*READ REQUESTS -> MISSING*/
    //char buffer[FILE_MAX_SIZE];
    


    while(1){
        task task_op;
        if((server_read = read(server_pipe, &task_op, sizeof(task)))<0){
            return -1;
        }  

        if(server_read<0){
            break;
        }
		pcq_enqueue(&task_queue, &task_op);

    }

    /* Close and unlink server pipe */
	if (close(server_pipe) != 0) {
		exit(EXIT_FAILURE);
	}
	if (unlink(pipe_name) != 0) {
		exit(EXIT_FAILURE);
	}

	pcq_destroy(&task_queue);
	return 0;
}
