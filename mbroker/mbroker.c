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


uint32_t number_max_sessions;
int current_boxes = 0;

mail_box boxes[MAX_BOXES];
pc_queue_t *task_queue;


pthread_cond_t thread_cond;
pthread_mutex_t mutex;
pthread_t tid[MAX_SESSIONS];
task *builder;


int init_server() {
    if (tfs_init(NULL) == -1) {
		return -1;
	}
	task_queue = malloc(sizeof(pc_queue_t));
    if (task_queue == NULL) {
        perror("malloc failed");
        return -1;
    }
	return 0;
}

void pub_connect_request(task* builder_t) {
    int return_value;
    char client_name[MAX_CLIENT_NAME];
    char box[MAX_BOXES];
    int pipe;

    // Copy client_name and box from buffer
    memcpy(client_name, builder_t->buffer + 1, MAX_CLIENT_NAME);
    memcpy(box, builder_t->buffer + 1 + MAX_CLIENT_NAME, MAX_BOXES);

    // Open the pipe for writing
    pipe = open(client_name, O_WRONLY);
    if (pipe == -1) {
        perror("Error opening pipe for writing");
        return;
    }

    // Check if there is already a publisher connected to the same mailbox
    for (int i = 0; i < number_max_sessions; i++) {
        if (builder[i].user_type == OP_CODE_PUB) {
            return_value = -1;
            if (write(pipe, &return_value, sizeof(int)) == 0) {
                close(pipe);
                return;
            }
        }
    }

    // Check if the mailbox exists
    for (int i = 0; i < MAX_BOXES; i++) {
        if (strcmp(box, boxes[i].box_name) == 0) {
            builder->user_type = OP_CODE_PUB;
            builder->session_id = pipe;
            strcpy(builder->pipe_path, client_name);
            return_value = 0;
            if (write(pipe, &return_value, sizeof(int)) == 0) {
                close(pipe);
                return;
            }
        }
    }
    return_value = -1;
    if (write(pipe, &return_value, sizeof(int)) == 0) {
        close(pipe);
        return;
    }

    // Close the pipe
    if (close(pipe) == -1) {
        perror("Error closing pipe");
    }
}

void sub_connect_request(task* builder_t) {
    int return_value;
    char client_name[MAX_CLIENT_NAME];
    char box[MAX_BOXES];
    int pipe;

    // Copy client_name and box from buffer
    memcpy(client_name, builder_t->buffer + 1, MAX_CLIENT_NAME);
    memcpy(box, builder_t->buffer + 1 + MAX_CLIENT_NAME, MAX_BOXES);

    // Open the pipe for writing
    pipe = open(client_name, O_WRONLY);
    if (pipe == -1) {
        perror("Error opening pipe for writing");
        return;
    }

    // Check if the user is already logged in as a subscriber
    if (builder_t->user_type == OP_CODE_LOGIN_SUB) {
        for (int i = 0; i < MAX_BOXES; i++) {
            if (strcmp(box, boxes[i].box_name) == 0) {
                builder_t->user_type = OP_CODE_LOGIN_SUB;
                builder_t->session_id = pipe;
                strcpy(builder->pipe_path, client_name);
                return_value = 0;
                if (write(pipe, &return_value, sizeof(int)) == 0) {
                    close(pipe);
                    return;
                }
            }
        }
        return_value = -1;
        if (write(pipe, &return_value, sizeof(int)) == 0) {
            close(pipe);
            return;
        }
        return;
    }
    return_value = -1;
    if (write(pipe, &return_value, sizeof(int)) == 0) {
        close(pipe);
        return;
    }

    // Close the pipe
    if (close(pipe) == -1) {
        perror("Error closing pipe");
    }
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

static int list_box_aux(const void* a, const void* b){
    return (int)strcmp(((mail_box *)a)->box_name, ((mail_box *)b)->box_name);
}

void list_box_request(task* builder_t){
    int pipe;
    char client_name[MAX_CLIENT_NAME];
    uint8_t op_code = OP_CODE_LIST_BOX;
    memcpy(client_name, builder_t->buffer + 1, MAX_CLIENT_NAME);
    pipe = open(client_name, O_WRONLY);

    // Check if there are any mailboxes
    if(current_boxes == 0){
        char response[sizeof(uint8_t) + sizeof(uint8_t) + MAX_BOXES * sizeof(char)];
        memcpy(response, &op_code, sizeof(uint8_t));
        memcpy(response + 1, &(uint8_t){1}, sizeof(uint8_t));
        memset(response + 2, '\0', MAX_BOXES * sizeof(char));

        // Write to the pipe
        if (write(pipe, response, sizeof(response)) == -1) {
            perror("Failed to write to the pipe");
            return;
        }
    }
    else {
        // Sort the boxes
        qsort(boxes, (size_t)current_boxes, sizeof(mail_box), list_box_aux);

        // Iterate through the boxes and write the information to the pipe
        for(int i = 0; i < current_boxes; i++) {
            char response[sizeof(uint8_t) + sizeof(uint8_t) + MAX_BOXES * sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t)];
            memcpy(response, &op_code, sizeof(uint8_t));
            memcpy(response + 1, &(boxes[i].last), sizeof(uint8_t));
            memset(response + 2, '\0', MAX_BOXES * sizeof(char));
            memcpy(response + 2, boxes[i].box_name, strlen(boxes[i].box_name) * sizeof(char));
            memcpy(response + 2 + MAX_BOXES, &boxes[i].box_size, sizeof(uint64_t));
            memcpy(response + 2 + MAX_BOXES + sizeof(uint64_t), &boxes[i].num_publishers, sizeof(uint64_t));
            memcpy(response + 2 + MAX_BOXES + sizeof(uint64_t) + sizeof(uint64_t), &boxes[i].num_subscribers, sizeof(uint64_t));

            // Write to the pipe
            if (write(pipe, response, sizeof(response)) == -1) {
                perror("Failed to write to the pipe");
                return;
            }
        }
    }
    // Close the pipe
    close(pipe);
}

void *task_handler(void *builder_v){
    task *builder_t = (task*) builder_v;

    while(true){
        pthread_mutex_lock(&builder_t->lock);
        while (builder_t->not_building) {
            pthread_cond_wait(&builder_t->flag, &builder_t->lock);
        }
        builder_t->not_building = false;
        if (memcpy(&builder_t->opcode, &builder_t->buffer, sizeof(char)) == NULL) {
            perror("memcpy failed");
            exit(EXIT_FAILURE);
        }
        switch (builder_t->opcode) {
        case OP_CODE_LOGIN_PUB:
            pub_connect_request(builder_t);
        case OP_CODE_LOGIN_SUB:
            sub_connect_request(builder_t);
        case OP_CODE_CREATE_BOX:
            box_create_request(builder_t);
        case OP_CODE_REMOVE_BOX:
            box_remove_request(builder_t);
        case OP_CODE_LIST_BOX:
            list_box_request(builder_t);
        }
        builder_t->not_building = true;
        pthread_mutex_unlock(&builder_t->lock);
    }
}

int initialize_threads(task *builder_t) {

    for (uint32_t i = 0; i < number_max_sessions; i++) {
        builder_t[i].not_building = true;
        int return_value;
        //initialize mutex for task
        if ((return_value = pthread_mutex_init(&builder_t[i].lock, NULL)) != 0) {
            fprintf(stderr, "[ERR]: couldn't initialize mutex: %s\n", strerror(return_value));
            goto cleanup;
        }
        
        //initialize condition variable for task
        if ((return_value = pthread_cond_init(&builder_t[i].flag, NULL)) != 0) {
            fprintf(stderr, "[ERR]: couldn't initialize condition variable: %s\n", strerror(return_value));
            goto cleanup;
        }
        
        //create thread for task
        if ((return_value = pthread_create(&builder_t[i].thread, NULL, task_handler, (void *) builder_t + i)) != 0) {
            fprintf(stderr, "[ERR]: couldn't create thread: %s\n", strerror(return_value));
            goto cleanup;
        }
    }
    return 0;

cleanup:
    //cleanup resources
    for (uint32_t j = 0; j < number_max_sessions; j++) {
        pthread_cancel(builder_t[j].thread);
        pthread_join(builder_t[j].thread, NULL);
        pthread_cond_destroy(&builder_t[j].flag);
        pthread_mutex_destroy(&builder_t[j].lock);
    }
    return -1;
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

    /* Get server pipe name from command line */
    char *pipe_name = argv[1];
	printf("Starting TecnicoFS server with pipe called %s\n", pipe_name);

	/*Get the maximum sessions argument*/
	number_max_sessions = (uint32_t)atoi(argv[2]);
	printf("the number of max_sessions has been defined to %d\n", number_max_sessions);
    builder = (task*)malloc(number_max_sessions * sizeof(task));

    if (pcq_create(task_queue, number_max_sessions) == -1){
        fprintf(stderr, "Error inicializing task request services.\n");
        return -1;
    }

    /* Unlink and create server pipe */
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
