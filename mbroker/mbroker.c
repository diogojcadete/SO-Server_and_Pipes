#include "../utils/logging.h"
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
#include "../producer-consumer/producer-consumer.h"



#define MAX_SESSIONS 128
#define MAX_QUEUE_SIZE 1024
#define MAX_SUBS 1024

uint32_t number_max_sessions;
int current_boxes = 0;

mail_box boxes[MAX_BOXES];
pc_queue_t *task_queue;


pthread_cond_t thread_cond;
pthread_mutex_t mutex;
pthread_t tid[MAX_SESSIONS];
task *builder;
mail_box boxes[MAX_BOXES];


int init_server() {
    if (tfs_init(NULL) == -1) {
		return -1;
	}
	task_queue = malloc(sizeof(pc_queue_t));
    if (task_queue == NULL) {
        return -1;
    }
	return 0;
}

static void end_mbroker(int sig) {
    if (sig == SIGINT) {
        fprintf(stdout, "\nEnded the server");
        return; // Resume execution at point of interruption
    }
}

char *task_error_box_to_str(task builder_t){
    return ("%d|%d|%s", builder_t.opcode, builder_t.return_value, builder_t.error);
}

char *task_to_str(task builder_t){
    return ("%d|%s", builder_t.opcode, builder_t.message);
}

task string_to_task(char* building) {
    task task_op;
    memset(task_op.pipe_path, 0, sizeof(task_op.pipe_path));
    memset(task_op.box_name, 0, sizeof(task_op.box_name));
    sscanf(building, "%s|%s|%s", &task_op.opcode, task_op.pipe_path, task_op.box_name);
    return task_op;
}


void pub_connect_request(task* builder_t) {

    int pipe;
    pipe = open(builder_t->pipe_path, O_RDONLY);
    if (pipe == -1){
        exit(EXIT_FAILURE);
    }
    // Check if there is already a publisher connected to the same mailbox
    int server_res = -1;
    for (int i = 0; i < current_boxes; i++) {
        if (strcmp(builder_t->box_name, boxes[i].box_name) == 0) {
            if(boxes[i].num_publishers == 1){
                if(write(pipe, &server_res, sizeof(int)) < 0){
                    exit(EXIT_FAILURE);
                }
            }
            else{
                char buffer[MESSAGE_MAX_SIZE];
                for(;;){
                    task task_op;
                    char pub_wr_request[sizeof(uint8_t) + 1 + (MESSAGE_MAX_SIZE * sizeof(char))];
                    ssize_t bytes_written = read(pipe, pub_wr_request, sizeof(pub_wr_request));
                    if (bytes_written == -1){
                        close(pipe);
                        exit(EXIT_FAILURE);
                    }
                    task_op = string_to_task(pub_wr_request);

                    int box_fhandle = tfs_open(builder_t->box_name, TFS_O_APPEND);
                    if(box_fhandle == -1){
                       exit(EXIT_FAILURE);
                    }
                    ssize_t bytes_read = tfs_read(box_fhandle,buffer, sizeof(buffer));
                    if(bytes_read<0){
                        exit(EXIT_FAILURE);
                    }
                    if((bytes_read + (ssize_t)strlen(task_op.message)) > 1024) {
                        break;
                    }
                    else{
                        ssize_t wr = tfs_write(box_fhandle, task_op.message, strlen(task_op.message));
                        if (wr == -1){
                            exit(EXIT_FAILURE);
                        }
                         tfs_close(box_fhandle);
                    }

                }
            }    
        }
    }    

    if(write(pipe, &server_res, sizeof(int)) < 0){
       exit(EXIT_FAILURE);
    }
    close(pipe);
}  


void sub_connect_request(task* builder_t) {
    int sub_pipe;
    if((sub_pipe = open(builder_t->pipe_path, O_WRONLY))==-1){
        exit(EXIT_FAILURE);
    }
   for (int i = 0; i < current_boxes; i++) {
        if (strcmp(builder_t->box_name, boxes[i].box_name) == 0) {
           strcpy(&boxes[i].subs_array_pipe_path[boxes[i].num_subscribers], builder_t->pipe_path);
            boxes[i].num_subscribers ++;    
            char buffer_box[MESSAGE_MAX_SIZE];
            int tfs_fd;
            tfs_fd = tfs_open(builder_t->box_name, TFS_O_APPEND);
            if(tfs_fd == -1){
                exit(EXIT_FAILURE);
            }
            for(;;){
                char sub_rd_request[sizeof(uint8_t) + 1 + (MESSAGE_MAX_SIZE * sizeof(char))];
                ssize_t bytes_read_box;
                bytes_read_box = tfs_read(tfs_fd, buffer_box, sizeof(buffer_box));
                if(bytes_read_box<0){
                    exit(EXIT_FAILURE);
                }
                task task_op;
                task_op.opcode = OP_CODE_READ;
                strcpy(task_op.message, buffer_box);
                strcpy(sub_rd_request, task_to_str(task_op));
                if(write(sub_pipe, &sub_rd_request, sizeof(sub_rd_request)) == -1){
                    close(sub_pipe);
                    exit(EXIT_FAILURE);
                }
            } 
        }
        else{
            int server_return = 1;
            if(write(sub_pipe,&server_return, sizeof(server_return)) == -1){
                close(sub_pipe);
                exit(EXIT_FAILURE);
            }
            
        }
   } 
    close(sub_pipe);

}

void box_create_request(task* builder_t) {
    // Check if the box already exists

    int pipe_fd = open(builder_t->pipe_path, O_WRONLY);
    char res_create[sizeof(uint8_t) + 2 + sizeof(int32_t) + sizeof(char)*MAX_ERROR_SIZE];
    for (int i = 0; i < current_boxes; i++) {
        if (strcmp(builder_t->box_name, boxes[i].box_name) == 0) {
            task task_op;
            task_op.opcode = OP_CODE_CREATE_BOX_RESPONSE;
            task_op.return_value = -1;
            strcpy(task_op.error,"THE BOX ALREADY EXISTS");
            strcpy(res_create, task_error_box_to_str(task_op));
            ssize_t bytes_written = write(pipe_fd, &res_create, sizeof(res_create));
            if(bytes_written == -1){
                exit(EXIT_FAILURE);
            }
        }
        
    }
    int fhandle;
    fhandle = tfs_open(builder_t->box_name,TFS_O_CREAT);
    if(fhandle == -1){
        exit(-1);
    }

    mail_box new_box = {
        .box_name = builder_t->box_name,
        .box_size = 1024,
        .last = 1,
        .num_publishers = 0,
        .num_subscribers = 0,
        .subs_array_pipe_path = {"0"}
    };

    boxes[current_boxes-1].last = 0;
    boxes[current_boxes] = new_box;
    current_boxes ++;

    // Close the pipe
    if(close(pipe_fd) == -1) {
        perror("Error closing pipe");
    }
}

void box_remove_request(task *builder_t){

    int pipe_fd = open(builder_t->pipe_path, O_WRONLY);
    char res_remove[sizeof(uint8_t) + 2 + sizeof(int32_t) + sizeof(char) * MAX_ERROR_SIZE];

    if(current_boxes == 0){
        task task_op;
        task_op.opcode = OP_CODE_CREATE_BOX_RESPONSE;
        task_op.return_value = -1;
        strcpy(task_op.error,"THERE AREN'T ANY BOXES");
        strcpy(res_remove, task_error_box_to_str(task_op));
        ssize_t bytes_written = write(pipe_fd, &res_remove, sizeof(res_remove));
        if(bytes_written == -1){
            exit(EXIT_FAILURE);
        }
    }
    else{
        int index;
        for(int i = 0; i< current_boxes; i++){
            if(strcmp(boxes[i].box_name, builder_t->box_name) == 0){
                index = i;
                break;
            }
        }
        for(int j = index; j < current_boxes -1 ; j++){
            boxes[j] = boxes[j+1];
        }
    }

    if(tfs_unlink(builder_t->box_name)==-1){
        exit(EXIT_FAILURE);
    }

    current_boxes--;
    
}
/*
void case_list_box(task* builder_t){
    int ret;
    int pipe;
    char client_name[MAX_CLIENT_NAME];
    uint8_t op_code = OP_CODE_LIST_BOX;
    memcpy(client_name, builder_t->buffer + 1, MAX_CLIENT_NAME);
    pipe = open(client_name, O_WRONLY);

    if(current_boxes == 0){
        char response[sizeof(uint8_t) + sizeof(uint8_t) + MAX_BOXES * sizeof(char)];
        memcpy(response, &op_code, sizeof(uint8_t));
        memcpy(response + 1, 1, 1 * sizeof(uint8_t));
        memset(response + 2, '\0', MAX_BOXES * sizeof(char));

        if (write(pipe, &response, sizeof(response)) == -1) {
		    return;
	    }
    }
    else {
        qsort(boxes, MAX_BOXES, sizeof(mail_box), myCompare); //sort the boxes
        char response[sizeof(uint8_t) + sizeof(uint8_t) + MAX_BOXES * sizeof(char) 
                + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t)];
        
        for(int i=0; i < current_boxes; i++){
            memcpy(response, &op_code, sizeof(uint8_t));
            memcpy(response + 1, boxes[i].last, 1 * sizeof(uint8_t));
            memset(response + 2, '\0', MAX_BOXES * sizeof(char));
            memcpy(response + 2, boxes[i].box_name, strlen(boxes[i].box_name) * sizeof(char));
            memcpy(response + 2 + MAX_BOXES, boxes[i].box_size, sizeof(uint64_t));
            memcpy(response + 2 + MAX_BOXES + sizeof(uint64_t), boxes[i].num_publishers, sizeof(uint64_t));
            memcpy(response + 2 + MAX_BOXES + sizeof(uint64_t) + sizeof(uint64_t), boxes[i].num_subscribers, sizeof(uint64_t));

            if (write(pipe, &response, sizeof(response)) == -1) {
		        return;
	        }

            //memset(response, 0, strlen(response));
        }
    }
}
*/
/* Auxiliary functions for sorting the boxes
static int myCompare(const void* a, const void* b){
  return strcmp(((mail_box *)a)->box_name, ((mail_box *)b)->box_name);
}
*/


void *task_handler(){
    while(true){
        task *task_t;
        if((task_t = (task*) pcq_dequeue(task_queue))==NULL){
            exit(EXIT_FAILURE);
        }
        switch (task_t->opcode) {
            case OP_CODE_LOGIN_PUB:
                pub_connect_request(task_t);
                break;              
            case OP_CODE_LOGIN_SUB:
                sub_connect_request(task_t);
                break;
            case OP_CODE_CREATE_BOX:
                box_create_request(task_t);
            case OP_CODE_REMOVE_BOX:
                box_remove_request(task_t);
        }
    }
}

int initialize_threads(task *builder_t) {

    for (uint32_t i = 0; i < number_max_sessions; i++) {
        int return_value;
        //initialize mutex for task
        
        //create thread for task
        if ((return_value = pthread_create(&builder_t[i].thread, NULL, task_handler, NULL)) != 0) {
            fprintf(stderr, "[ERR]: couldn't create thread: %s\n", strerror(return_value));
    
        }
    }
    return 0;
}



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


    printf("entrei\n");
    /* Get server pipe name from command line */
    char *pipe_name = argv[1];
	printf("Starting TecnicoFS server with pipe called %s\n", pipe_name);

	/*Get the maximum sessions argument*/
	number_max_sessions = (uint32_t)atoi(argv[2]);
	printf("the number of max_sessions has been defined to %d\n", number_max_sessions);
    printf("1\n");
    builder = (task*)malloc(number_max_sessions * sizeof(task));
    task_queue = (pc_queue_t*)malloc(1024 * sizeof(pc_queue_t));
    printf("2\n");
    if (pcq_create(task_queue, MAX_QUEUE_SIZE) == -1){
        fprintf(stderr, "Error inicializing task request services.\n");
        return -1;
    } 


	if (unlink(pipe_name) == -1 && errno != ENOENT) {
		exit(EXIT_FAILURE);
	}

	if (mkfifo(pipe_name, 0777) == -1) {
		exit(EXIT_FAILURE);
	}

    /* Open server pipe */
    int server_pipe; 
    ssize_t server_read;
	if ((server_pipe = open(pipe_name, O_RDONLY)) == -1) {
		exit(EXIT_FAILURE);
	}   

    initialize_threads(builder);

    for(;;){
        task task_op;
        char request[sizeof(uint8_t) + 2 + (PIPE_PATH_MAX_SIZE * sizeof(char)) + (sizeof(char) * MAX_BOX_NAME)];
        if((server_read = read(server_pipe, &request, sizeof(task)))<0){
            return -1;
        } 
        if(server_read<0){
            break;
        }
        task_op = string_to_task(request);
        switch (task_op.opcode)
        {
        case OP_CODE_LOGIN_PUB:
        case OP_CODE_LOGIN_SUB:
        case OP_CODE_CREATE_BOX:
        case OP_CODE_LIST:
        case OP_CODE_REMOVE_BOX:

            pcq_enqueue(task_queue, (void *) &task_op);
            break;

        default:
            break;    
        }
        if (signal(SIGINT, end_mbroker) == SIG_ERR){
            pcq_destroy(task_queue);
            free(task_queue);
            
            close(server_pipe);
            unlink(pipe_name);
            exit(EXIT_SUCCESS);
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
