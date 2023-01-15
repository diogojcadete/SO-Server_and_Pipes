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
int global_pub_pipe;
char global_pipe_name[PIPE_PATH_MAX_SIZE];

char wr_task_to_string(task builder_t, char *message){
    return ("%s|%s", builder_t.opcode, builder_t.pipe_path, message);
}

char task_to_string(task builder_t){
    return ("%s|%s|%s", builder_t.opcode, builder_t.pipe_path, builder_t.box_name);
}

void sig_handler(int signo) {
    if (signo == SIGQUIT) {
        session_active = false;
        fprintf(stderr, "[INFO]: closing named pipe\n");
        close(global_pub_pipe); 
        unlink(global_pipe_name);
    }
}

void send_msg_server(int pub_pipe, char const * pipe_path, char const *message){
    task task_op;
	task_op.opcode = OP_CODE_WRITE;
	strcpy(task_op.pipe_path, pipe_path);

    char pub_wr_request[sizeof(uint8_t) + 1 + (MESSAGE_MAX_SIZE * sizeof(char))];
    strcpy(pub_wr_request, wr_task_to_string(task_op, message));
    
	if (write(pub_pipe, &pub_wr_request, sizeof(pub_wr_request)) == -1) {
        fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
        close(pub_pipe);
        unlink(pipe_path);
		exit(EXIT_FAILURE);
	}
}

int start_server_connection(int server_pipe, char const * pipe_path, char const *box_name) {

	/* Create client pipe */
	//strcpy(client_pipe_file, pipe_path);

	/* Send request to server */


	task task_op;
	task_op.opcode = OP_CODE_LOGIN_PUB;
	strcpy(task_op.pipe_path, pipe_path);
    strcpy(task_op.box_name, box_name);
    char pub_request[sizeof(uint8_t) + 2 + (PIPE_PATH_MAX_SIZE * sizeof(char)) + (sizeof(char) * MAX_BOX_NAME)];
    strcpy(pub_request,task_to_string(task_op));

	if (write(server_pipe, &pub_request, sizeof(pub_request)) == -1) {
        fprintf(stderr, "failed to read from the server\n");
        close(pub_pipe);
        unlink(pipe_path);
		exit(EXIT_FAILURE);
	}

    int pub_pipe = open(pipe_path, O_RDONLY);
    if (pub_pipe == -1){
        return -1;
    }
    int server_return;
    if (read(pub_pipe, &server_return, sizeof(server_return)) == -1){
        fprintf(stderr, "failed to read from the server\n");
        return -1;
    }
    close(pub_pipe);
    return server_return;
    
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

    if(start_server_connection(server_pipe, pipe_name, box_name) == -1){
        exit(EXIT_FAILURE);
    }
    int pipe_fhandle = open(pipe_name, O_WRONLY);
    if (pipe_fhandle == -1) {
            fprintf(stderr, "Failed to open the named pipe");
            exit(EXIT_FAILURE);
        }

    global_pub_pipe = pipe_fhandle;
    strcpy(global_pipe_name,pipe_name);

    char buffer1[MESSAGE_MAX_SIZE];
    ssize_t bytes_read;

    memset(buffer1,0,sizeof(buffer1));

    session_active = true;
    while(session_active == true){
       while((bytes_read = read(STDIN_FILENO, buffer1, sizeof(buffer1))) > 0 || strcmp(buffer1,"\n") != 0){
            if (signal(SIGQUIT, sig_handler) == SIG_ERR) {
                //nesta função ainda vou para o close pipe ou tenho de fzr isso dentro do sig_handler?
                printf("\n can't catch SIGINT\n");
                break;
            }
            send_msg_server(pipe_fhandle, pipe_name, buffer1);
       } 
       
    }

    fprintf(stderr, "[INFO]: closing named pipe\n");
    close(pipe_fhandle); 
    unlink(pipe_name);

    return -1;
}
