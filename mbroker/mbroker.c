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

uint32_t number_max_sessions;
int current_boxes = 0;

mail_box boxes[MAX_BOXES];
pc_queue_t *task_queue;


pthread_cond_t thread_cond;
pthread_mutex_t mutex;
pthread_t tid[MAX_SESSIONS];
task *builder;

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

void *task_handler(void *builder_v){
    task *builder_t = (task*) builder_v;

    char op_code = '\0';
    while(true){
        pthread_mutex_lock(&builder_t->lock);
        while (builder_t->not_building) {
            pthread_cond_wait(&builder_t->flag, &builder_t->lock);
        }
        builder_t->not_building = false;
        if (memcpy(&op_code, &builder_t->buffer, sizeof(char)) == NULL) {
            perror("memcpy failed");
            exit(EXIT_FAILURE);
        }


        // mudar isto para tbm ter os ecrever e ler mensagem
        // + fazer as respostas
            /*
        switch (op_code) {
        case OP_CODE_LOGIN_PUB:
            case_pub_request(builder_t);
        case SUB_REQUEST:
            case_sub_request(actual_session);
        case CREATE_BOX_REQUEST:
            case_create_box(actual_session);
        case REMOVE_BOX_REQUEST:
            case_remove_box(actual_session);
        case LIST_BOXES_REQUEST:
            case_list_box(actual_session);
        }
            */
        builder_t->not_building = true;
        pthread_mutex_unlock(&builder_t->lock);
    }
}


int initialize_threads(task *builder_t) {
    for (uint32_t i=0; i < number_max_sessions; i++){
        builder_t[i].not_building = true;
       	if (pthread_mutex_init(&builder_t[i].lock, NULL) == -1) {
			return -1;
		}

        if (pthread_cond_init(&builder_t[i].flag, NULL) == -1){
            return -1;
        }

        if (pthread_create(&builder_t[i].thread, NULL, task_handler, (void *) builder_t+i) != 0) {
            fprintf(stderr, "[ERR]: couldn't create threads: %s\n", strerror(errno));
			return -1;
		}
    }
    return 0;
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

	/*Get the maximum sessions argument*/
	number_max_sessions = (uint32_t)atoi(argv[2]);
	printf("the number of max_sessions has been defined to %d\n", number_max_sessions);
    builder = (task*)malloc(number_max_sessions * sizeof(task));

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

    }

    /* Close and unlink server pipe */
	if (close(server_pipe) != 0) {
		exit(EXIT_FAILURE);
	}
	if (unlink(pipe_name) != 0) {
		exit(EXIT_FAILURE);
	}


	return 0;
}
