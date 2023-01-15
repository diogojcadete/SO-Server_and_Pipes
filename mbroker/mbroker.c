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

typedef struct{
    bool is_free;
    char *box_name;
    uint8_t last;
    uint64_t box_size;
    uint64_t num_publishers;
    uint64_t num_subscribers;
    char subs_array_pipe_path[1024];
}mail_box;

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

int pub_connect_request(task* builder_t) {

    // Check if there is already a publisher connected to the same mailbox
    for (int i = 0; i < current_boxes; i++) {
        if (strcmp(builder_t->box_name, boxes[i].box_name) == 0) {
            if(boxes[i].num_publishers == 1){
                return -1;
            }
            else{
                return 0;
            }    
        }
    }    

     return -1;  
}

void sub_connect_request(task* builder_t) {
   for (int i = 0; i < current_boxes; i++) {
        if (strcmp(builder_t->box_name, boxes[i].box_name) == 0) {
            strcpy(boxes[i].subs_array_pipe_path[boxes[i].num_subscribers], (char *)builder_t->pipe_path);
            boxes[i].num_subscribers ++;
        }
   } 
   message message_sub;
   message_sub.opcode = OP_CODE_READ;
   char buffer_box[MESSAGE_MAX_SIZE];
    int tfs_fd;
    tfs_fd = tfs_open(&builder_t->box_name, TFS_O_APPEND);
    if(tfs_fd == -1){
        exit(EXIT_FAILURE);
    }
    ssize_t bytes_read_box;
    bytes_read_box = tfs_read(tfs_fd, buffer_box, sizeof(buffer_box));
    strcpy(message_sub.message, buffer_box);
    int sub_pipe;
    if(sub_pipe = open(builder_t->pipe_path, O_WRONLY)==-1){
        exit(EXIT_FAILURE);
    }
    if(write(sub_pipe, &message_sub, sizeof(message)) == -1){
        exit(EXIT_FAILURE);
    }
    close(sub_pipe);

}

void box_create_request(task* builder_t) {
    int return_value;
    int pipe;
    char client_name[MAX_CLIENT_NAME];
    char box_name[MAX_BOXES];

    // Copy client_name and box_name from buffer
    memcpy(client_name, builder_t->buffer + 1, MAX_CLIENT_NAME);
    memcpy(box_name, builder_t->buffer + 1 + MAX_CLIENT_NAME, MAX_BOXES);

    // Open the pipe for writing
    pipe = open(client_name, O_WRONLY);
    if (pipe == -1) {
        perror("Error opening pipe for writing");
        return;
    }

    // Check if the box already exists
    for (int i = 0; i < MAX_BOXES; i++) {
        if (strcmp(box_name, boxes[i].box_name) == 0) {
            return_value = -1;
            if (write(pipe, &return_value, sizeof(int)) == 0) {
                close(pipe);
                return;
            }
        }
    }

    // Create the new box
    for (int i = 0; i < MAX_BOXES; i++) {
        if (boxes[i].is_free) {
            boxes[i].box_name = box_name;
            boxes[i].box_size = 1024;
            boxes[i].is_free = false;
            boxes[i].last = 1;
            boxes[i].num_publishers = 0;
            boxes[i].num_subscribers = 0;
            if (i != 0) {
                boxes[i - 1].last = 0;
            }
            current_boxes++;
            return_value = 0;
            if (write(pipe, &return_value, sizeof(int)) == 0) {
                close(pipe);
                return;
            }
        }
    }

    // If the function reaches here, it means that there are no free boxes available
    return_value = -1;
    if (write(pipe, &return_value, sizeof(int)) == 0) {
        close(pipe);
        return;
    }

    // Close the pipe
    if(close(pipe) == -1) {
        perror("Error closing pipe");
    }
}

void box_remove_request(task* builder_t){
    int return_value;
    int pipe;
    char client_name[MAX_CLIENT_NAME];
    char box_name[MAX_BOXES];
    // Copy the client name from the buffer into the client_name variable
    memcpy(client_name, builder_t->buffer + 1, MAX_CLIENT_NAME);
    // Copy the box name from the buffer into the box_name variable
    memcpy(box_name, builder_t->buffer + 1 + MAX_CLIENT_NAME, MAX_BOXES);
    // Open the pipe for writing
    pipe = open(client_name, O_WRONLY);
    // Check if the box name provided is a valid box
    for (int i=0; i < MAX_BOXES; i++){
        if (strcmp(box_name, boxes[i].box_name) != 0){
            // If the box name is not valid, return -1 and close the pipe
            return_value = -1;
            if (write(pipe, &return_value, sizeof(int)) == 0){
                close(pipe);
                return;
            }   
        }
    }
    // Delete the box from the file system
    tfs_unlink(box_name);
    // Remove the box from the boxes array
    for (int i=0; i < MAX_BOXES; i++){
        if(strcmp(boxes[i].box_name, box_name)){
            // Clear the box's information
            strcpy(boxes[i].box_name, box_name);
            boxes[i].box_size = 0;
            boxes[i].is_free = true;
            boxes[i].last = 0;
            boxes[i].num_publishers = 0;
            boxes[i].num_subscribers = 0;
            boxes[i-1].last = 1;
            current_boxes--;
            // Return 0 on success
            return_value = 0;
            if (write(pipe, &return_value, sizeof(int)) == 0){
                close(pipe);
                return;
            }
        }
    // If the box was not removed, return -1
    return_value = -1;
    if (write(pipe, &return_value, sizeof(int)) == 0){
        close(pipe);
        return;
        }
    }
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
                if(pub_connect_request(task_t) == -1){
                    int server_res = -1;
                    if(write(pipe, &server_res, sizeof(int)) < 0){
                        exit(EXIT_FAILURE);
                    }
                }
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
        builder_t[i].not_building = true;
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

    printf("3\n");

    initialize_threads(builder);

    printf("4\n");
    for(;;){
        task task_op;
        if((server_read = read(server_pipe, &task_op, sizeof(task)))<0){
            return -1;
        } 
        if(server_read<0){
            break;
        }
        switch (task_op.opcode)
        {
        case OP_CODE_LOGIN_PUB:
        case OP_CODE_LOGIN_SUB:
        case OP_CODE_CREATE_BOX:
        case OP_CODE_LIST:
        case OP_CODE_REMOVE_BOX:
            printf("5\n");
            pcq_enqueue(task_queue, (void *) &task_op);
            break;
        

        case OP_CODE_WRITE:
            int pipe_fd,tfs_fd;
            char buffer[FILE_MAX_SIZE], buffer_box[FILE_MAX_SIZE];
            if ((pipe_fd = open(task_op.pipe_path, O_RDONLY)) == -1) {
		        exit(EXIT_FAILURE);
	        }
            tfs_fd = tfs_open(task_op.pipe_path, TFS_O_APPEND);
            if(tfs_fd == -1){
                return NULL;
            }
            ssize_t bytes_read_box,bytes_read;
            bytes_read_box = tfs_read(tfs_fd, buffer_box, sizeof(buffer_box));
            bytes_read = read(pipe_fd, buffer, sizeof(buffer));
            if(bytes_read + bytes_read_box>1024){
                break;
            }
            else{
                ssize_t bytes_written = tfs_write(tfs_fd, buffer, (size_t)bytes_read);
                if(bytes_written < 0){
                    close(pipe_fd);
                    tfs_close(tfs_fd);
                    return NULL;
                }
            }
            close(pipe_fd);
            for(int i = 0; i< current_boxes; i++){
                if(strcmp(task_op.box_name, boxes[i].box_name) == 0){
                    for(int j = 0; j< boxes[i].num_subscribers; j++){
                        message new_message;
                        char buffer_sub[1024];
                        new_message.opcode = OP_CODE_READ;
                        strcpy(buffer_sub,buffer);
                        strcpy(new_message.message, buffer_sub);
                        int sub_pipe;
                        if(sub_pipe = open(boxes[i].subs_array_pipe_path[j], O_WRONLY)==-1){
                            exit(EXIT_FAILURE);
                        }
                        if(write(sub_pipe, &new_message, sizeof(message)) == -1){
                            exit(EXIT_FAILURE);
                        }
                        close(sub_pipe);
                    }
                }
            }
            tfs_close(tfs_fd);


        }
        if (signal(SIGINT, mbroker_exit) == SIG_ERR){
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
