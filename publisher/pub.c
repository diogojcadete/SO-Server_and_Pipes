
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "../utils/lib.h"

int session_id;
int pub_pipe;
char pipe_file[PIPE_PATH_MAX_SIZE];
bool session_active;

void sig_handler(int signo) {
    if (signo == SIGINT) {
        session_active = false;
    }
}

void start_server_connection(int server_pipe, char const * pipe_path, char const *box_name) {

	/* Create client pipe */
	//strcpy(client_pipe_file, pipe_path);

	/* Send request to server */
	task task_op;
	task_op.opcode = OP_CODE_LOGIN_PUB;
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


	if (argc != 4){
		printf("The number of arguments is invalid.\n");
		printf("The correct usage is: pub <register_pipe_name> <pipe_name> <box_name>\n");
		exit(EXIT_FAILURE);
	} 
    
    char *register_pipe_name = argv[1];
    char *pipe_name = argv[2];
    char *box_name = argv[3];

    if (unlink(pipe_name) == -1 && errno != ENOENT) {
		return -1;
	}

    if (strlen(box_name) > PATH_MAX_SIZE || strlen(pipe_name) > PIPE_PATH_MAX_SIZE){
        fprintf(stderr, "The arguments surpassed the maximum size allowed\n");
        exit(EXIT_FAILURE);
    }
    
    if (mkfifo(pipe_name, 0640) == -1) {
        fprintf(stderr, "Pipe already exists /n");
		exit(EXIT_FAILURE);
	}
    fprintf(stderr, "usage: pub %s %s %s\n", register_pipe_name, pipe_name, box_name);


    int server_pipe;
    server_pipe = open(register_pipe_name, O_WRONLY);
    if (server_pipe == -1) {
        fprintf(stderr, "Failed to open server pipe/n");
        exit(EXIT_FAILURE);
    }

    printf("1\n");
    start_server_connection(server_pipe, pipe_name, box_name);
    printf("2\n");
    int pipe_fhandle = open(pipe_name, O_WRONLY);
    if (pipe_fhandle == -1) {
            fprintf(stderr, "Failed to open the named pipe");
            exit(EXIT_FAILURE);
        }
    printf("3\n");
    char buffer[1], buffer2[1024];

    memset(buffer2,0,sizeof(buffer));
    int i = 0;
    session_active = true;
    while(session_active == true){
       while(read(STDIN_FILENO, &buffer, 1) > 0 || strcmp(buffer,"\n") == 0){
            printf("ainda estou a ler\n");
            if (signal(SIGINT, sig_handler) == SIG_ERR) {
                printf("\n can't catch SIGINT\n");
                break;
            }
            memset(buffer2 + i,strlen(buffer), 1);
            i++;
       } 
       printf("estou aqui\n");
        write(pipe_fhandle, buffer2, sizeof(buffer2)); 
    }

    fprintf(stderr, "[INFO]: closing named pipe\n");
    close(pipe_fhandle); 
    unlink(pipe_name);

    return -1;
}
