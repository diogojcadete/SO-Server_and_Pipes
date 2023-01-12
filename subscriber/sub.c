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


void sig_handler(int signo) {
    if (signo == SIGINT) {
        session_active = false;
        printf("\nSession closed. Number of messages received: %d\n", num_messages);
    }
}

int start_server_connection(char const *server_pipe_path, char const *pipe_path, char const *box_name) {

	/* Create client pipe */
	if (unlink(pipe_path) == -1 && errno != ENOENT) {
		return -1;
	}
	if (mkfifo(pipe_path, 0640) == -1) {
		return -1;
	}
	//strcpy(client_pipe_file, pipe_path);

	/* Open server pipe */
    int server_fd;
	if ((server_fd = open(server_pipe_path, O_WRONLY)) == -1) {
		return -1;
	}

	/* Send request to server */
	task task_op;
    task_op.user_type = OP_CODE_SUBBSCRIBER;
	task_op.opcode = OP_CODE_LOGIN_SUB;
	strcpy(task_op.pipe_path, pipe_path);
    strcpy(task_op.box_name, box_name);
	if (write(server_fd, &task_op, sizeof(task)) == -1) {
        close(server_fd);
		return -1;
	}
    return 0;
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

    if(start_server_connection(server_pipe, sub_pipe, box_name) == -1){
        return -1;
    }

    int sub_fd = open(sub_pipe, O_RDONLY);

    if (sub_fd < 0) {
        exit(EXIT_FAILURE);
    }

    char buffer[MESSAGE_MAX_SIZE + sizeof(uint8_t)];

    ssize_t bytes_read,bytes_written;
    while(session_active == true){
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            printf("\n can't catch SIGINT\n");
        }
        bytes_read = read(sub_fd, buffer, sizeof(buffer));
        bytes_written = write(STDOUT_FILENO, buffer, (size_t)bytes_read);
        if (bytes_written < 0){
            close(sub_fd);
            close(server_fd);
            unlink(sub_pipe);
            return -1;
        }
        num_messages++;
    }
    close(sub_fd);
    close(server_fd);
    unlink(sub_pipe);
    return 0;
}
