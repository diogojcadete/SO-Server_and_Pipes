#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "../utils/lib.h"

int num_messages;
bool session_active;
int global_pub_pipe;
char global_pipe_name[PIPE_PATH_MAX_SIZE];

char *task_to_string(task builder_t){
    char* response = malloc(sizeof(char) * (MESSAGE_MAX_SIZE + 1));
    snprintf(response, MESSAGE_MAX_SIZE,"%hhu|%s|%s", builder_t.opcode, builder_t.pipe_path, builder_t.box_name);
    return response;
}

void sig_handler(int signo) {
    if (signo == SIGINT) {
        session_active = false;
        printf("\nSession closed. Number of messages received: %d\n", num_messages);
        close(global_pub_pipe); 
        unlink(global_pipe_name);
    }
}

int start_server_connection(int server_pipe, char const * pipe_path, char const *box_name) {

	/* Create client pipe */
	//strcpy(client_pipe_file, pipe_path);

	/* Send request to server */
	task task_op;
	task_op.opcode = OP_CODE_LOGIN_SUB;
	strcpy(task_op.pipe_path, pipe_path);
    strcpy(task_op.box_name, box_name);
    printf("SSC : 1\n");
    
    char sub_request[sizeof(uint8_t) + 2 + (PIPE_PATH_MAX_SIZE * sizeof(char)) + (sizeof(char) * MAX_BOX_NAME)];
    strcpu(sub_request,task_to_string(task_op));

	if (write(server_pipe, &sub_request, sizeof(task)) == -1) {
        fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
    
    int sub_pipe = open(pipe_path, O_RDONLY);
    if (sub_pipe == -1){
        return -1;
    }
    int server_return;
    if (read(sub_pipe, &server_return, sizeof(server_return)) == -1){
        fprintf(stderr, "failed to read from the server\n");
        return -1;
    }
    close(sub_pipe);
    return server_return;
}



int main(int argc, char **argv) {
    /* check if we have enough arguments */
    if (argc < 4) {
        printf("Usage: %s <server_path_pipe> <sub_pipe_path> <box_message>\n", argv[0]);
        return -1;
    }

    const char *server_pipe = argv[1];
    const char *sub_pipe = argv[2];
    const char *box_name = argv[3];
    session_active = true;

    if (mkfifo(sub_pipe, 0777) == -1) {
		exit(EXIT_FAILURE);
	}
    int server_fd = open(server_pipe, O_WRONLY);
    if(server_fd<0){
        exit(EXIT_FAILURE);
    }
    /* check if the open function call was successful */
    /* open server pipe with O_WRONLY flag for writing */

    if(start_server_connection(server_fd, sub_pipe, box_name) == -1){
        exit(EXIT_FAILURE);
    }

    int sub_fd = open(sub_pipe, O_RDONLY);

    if (sub_fd < 0) {
        exit(EXIT_FAILURE);
    }

    task task_op;
    ssize_t bytes_read,bytes_written;
    session_active = true;
    char sub_res[sizeof(uint8_t) + 1 + (MESSAGE_MAX_SIZE * sizeof(char))];
    printf("session is now open\n");
    while(session_active == true){
        printf("awaiting data...\n");
       while((bytes_read = read(sub_fd, &sub_res, sizeof(message)))>0){
            task new_task = string_to_task(sub_res);
            bytes_written = write(STDOUT_FILENO, new_task.message, sizeof(new_task.message));
            if(bytes_written <0){
                close(sub_fd);
                close(server_fd);
                unlink(sub_pipe);
                exit(EXIT_FAILURE);
            }
            if (signal(SIGINT, sig_handler) == SIG_ERR) {
                printf("\n can't catch SIGINT\n");
                close(sub_fd);
                close(server_fd);
                unlink(sub_pipe);
                exit(EXIT_FAILURE);
            }
            num_messages++;
        }   
    }

    if(bytes_read <0){
            session_active = false;
            close(sub_fd);
            close(server_fd);
            unlink(sub_pipe);
            exit(EXIT_FAILURE);
    }

    close(sub_fd);
    close(server_fd);
    unlink(sub_pipe);
    exit(EXIT_SUCCESS);
}
