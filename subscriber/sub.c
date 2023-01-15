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

void sig_handler(int signo) {
    if (signo == SIGINT) {
        session_active = false;
        printf("\nSession closed. Number of messages received: %d\n", num_messages);
        close(global_pub_pipe); 
        unlink(global_pipe_name);
    }
}

void start_server_connection(int server_pipe, char const * pipe_path, char const *box_name) {

	/* Create client pipe */
	//strcpy(client_pipe_file, pipe_path);

	/* Send request to server */
	task task_op;
	task_op.opcode = OP_CODE_LOGIN_SUB;
	strcpy(task_op.pipe_path, pipe_path);
    strcpy(task_op.box_name, box_name);
    printf("SSC : 1\n");
    
	if (write(server_pipe, &task_op, sizeof(task)) == -1) {
        fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
    printf("SSC : 2\n");
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

    start_server_connection(server_fd, sub_pipe, box_name);

    int sub_fd = open(sub_pipe, O_RDONLY);

    if (sub_fd < 0) {
        exit(EXIT_FAILURE);
    }

    message message_received;
    ssize_t bytes_read,bytes_written;
    session_active = true;
    printf("session is now open\n");
    while(session_active == true){
        printf("awaiting data...\n");
       while((bytes_read = read(sub_fd, &message_received, sizeof(message)))>0){
            bytes_written = write(STDOUT_FILENO, &message_received.message, (size_t)bytes_read);
            if(bytes_written <0){
                close(sub_fd);
                close(server_fd);
                unlink(sub_pipe);
                exit(EXIT_FAILURE);
            }
            message_received.opcode = OP_CODE_READ;
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
