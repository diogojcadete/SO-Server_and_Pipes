#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "../utils/lib.h"



int server_fd;
int man_fd;

char *task_to_string(task builder_t){
    char* response = malloc(sizeof(char) * (MESSAGE_MAX_SIZE + 1));
    snprintf(response, MESSAGE_MAX_SIZE, "%hhu|%s|%s", builder_t.opcode, builder_t.pipe_path, builder_t.box_name);
    return response;

}

void start_server_connection(char const *server_pipe_path, char const *pipe_path, char const *request, char const *box_name) {

	/* Create client pipe */
	if (unlink(pipe_path) == -1 && errno != ENOENT) {
		exit(EXIT_FAILURE);
	}
	if (mkfifo(pipe_path, 0640) == -1) {
		exit(EXIT_FAILURE);
	}
	//strcpy(client_pipe_file, pipe_path);

	/* Open server pipe */
	if ((server_fd = open(server_pipe_path, O_WRONLY)) == -1) {
		exit(EXIT_FAILURE);
	}

	/* Send request to server */
	task task_op;
    strcpy(task_op.pipe_path, pipe_path); 

    char req_create[sizeof(uint8_t) + 2 + (PIPE_PATH_MAX_SIZE * sizeof(char)) + (sizeof(char) * MAX_BOX_NAME)];
    char req_remove[sizeof(uint8_t) + 2 + (PIPE_PATH_MAX_SIZE * sizeof(char)) + (sizeof(char) * MAX_BOX_NAME)];
    char req_list[sizeof(uint8_t) + 1 + sizeof(char) * PIPE_PATH_MAX_SIZE];
    char res_create[sizeof(uint8_t) + 2 + sizeof(int32_t) + sizeof(char) * MAX_ERROR_SIZE];
    char res_remove[sizeof(uint8_t) + 2 + sizeof(int32_t) + sizeof(char) * MAX_ERROR_SIZE];
    char res_list[sizeof(uint8_t) + 5 + sizeof(uint8_t) + sizeof(char)*MAX_BOX_NAME + 3 * sizeof(uint64_t)];

    if (strcmp(request, "create") == 0){
        task_op.opcode = OP_CODE_CREATE_BOX;
        strncpy(task_op.box_name, box_name, MAX_BOX_NAME);
        strcpy(req_create,task_to_string(task_op));
        if (write(server_fd, &req_create, sizeof(req_create)) == -1) {
		exit(EXIT_FAILURE);
        }
        if (read(man_fd, &res_create, sizeof(res_create)) == -1){
            fprintf(stderr, "failed to read from the server\n");
            exit(EXIT_FAILURE);
        }
        task new_task;
        new_task = string_to_task(res_create);
        if(new_task.return_value == 0){
            fprintf(stdout, "OK\n");
            close(man_fd);
        }
        else{
            fprintf(stdout, "ERROR %s\n", new_task.error);
            close(man_fd);
        }
	}

    else if(strcmp(request,"remove") == 0){
        task_op.opcode = OP_CODE_REMOVE_BOX;
        strncpy(task_op.box_name, box_name, MAX_BOX_NAME);
        strcpy(req_create,task_to_string(task_op));
        if (write(server_fd, &req_remove, sizeof(req_remove)) == -1) {
            close(server_fd);
            exit(EXIT_SUCCESS);
	    }
        if (read(man_fd, &res_remove, sizeof(res_remove)) == -1){
            fprintf(stderr, "failed to read from the server\n");
            exit(EXIT_SUCCESS);
        }
        task new_task;
        new_task = string_to_task(res_remove);
        if(new_task.return_value == 0){
            fprintf(stdout, "OK\n");
            close(man_fd);
            exit(EXIT_SUCCESS);
        }
        else{
            fprintf(stdout, "ERROR %s\n", new_task.error);
            close(man_fd);
        }
    }
    else if(strcmp(request,"list") == 0){
        task_op.opcode = OP_CODE_LIST;
        if (write(server_fd, &req_list, sizeof(req_list)) == -1) {
            close(server_fd);
		    exit(EXIT_FAILURE);
	    }   
        if (read(man_fd, &res_list, sizeof(res_list)) == -1){
            fprintf(stderr, "failed to read from the server\n");
            exit(EXIT_FAILURE);
        }
        task new_task;
        new_task = string_to_task(res_list);
        if(new_task.return_value == 0){
        ;
        }
        else{
            fprintf(stdout, "NO BOXES FOUND\n");
            close(man_fd);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 5) {
        printf("Usage: %s server_pipe_path manager_pipe_path create/remove/list [box_name]\n", argv[0]);
        return -1;
    }


    const char *server_pipe = argv[1];
    const char *manager_pipe = argv[2];
    const char *type_request = argv[3];
    const char *box_path;

    /*Criar o PIPE*/
    if (mkfifo(manager_pipe, 0777) == -1) {
        unlink(manager_pipe);
		return -1;
	}

    man_fd = open(manager_pipe, O_RDONLY);

    if (man_fd < 0) {
        close(man_fd);
        close(server_fd);
        unlink(manager_pipe);
        perror("Couldn't open Manager_Pipe");
        return -1;
    }

    if (strcmp(type_request, "create") == 0 || strcmp(type_request,"remove") == 0){
        box_path = argv[4];
    }

    else if(strcmp(type_request,"list") == 0){
        box_path = NULL;
    }
    
    else{
        perror("Unknown request. Please try again.");
        unlink(manager_pipe);
        return -1;
    }
    /*Fazer conexão com o Servidor*/
    start_server_connection(server_pipe, manager_pipe, type_request, box_path);
        unlink(manager_pipe);


    // Dar List das mensagens
    if (strcmp(type_request,"list") == 0) {
        char box_names[MAX_NUM_BOXES][PATH_MAX_SIZE];
        int num_boxes;
        if (read(man_fd, &num_boxes, sizeof(int)) == -1) {
            perror("Error reading number of boxes from server");
            close(man_fd);
            close(server_fd);
            unlink(manager_pipe);
            return -1;
        }
        if (read(man_fd, box_names,(size_t) num_boxes * PIPE_PATH_MAX_SIZE) == -1) {
            perror("Error reading box names from server");
            close(man_fd);
            close(server_fd);
            unlink(manager_pipe);
            return -1;
        }

        // Print the list of box names
        for (int i = 0; i < num_boxes; i++) {
            printf("%s\n", box_names[i]);
        }
    }



    close(man_fd);
    close(server_fd);
    unlink(manager_pipe);
    return 0;
}

