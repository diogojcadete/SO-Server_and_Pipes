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

int start_server_connection(char const *server_pipe_path, char const *pipe_path, char const *request, char const *box_name) {

	/* Create client pipe */
	if (unlink(pipe_path) == -1 && errno != ENOENT) {
		return -1;
	}
	if (mkfifo(pipe_path, 0640) == -1) {
		return -1;
	}
	//strcpy(client_pipe_file, pipe_path);

	/* Open server pipe */
	if ((server_fd = open(server_pipe_path, O_WRONLY)) == -1) {
		return -1;
	}

	/* Send request to server */
	task task_op;
	strcpy(task_op.pipe_path, pipe_path);

    if (strcmp(request, "create") == 0){
        task_op.opcode = OP_CODE_CREATE;
        strncpy(task_op.box_name, box_name, PATH_MAX_SIZE);
    }
    else if(strcmp(request,"remove") == 0){
        task_op.opcode = OP_CODE_REMOVE;
        strncpy(task_op.box_name, box_name, PATH_MAX_SIZE);
    }
    else if(strcmp(request,"list") == 0){
        task_op.opcode = OP_CODE_LIST;
    }
    if (write(server_fd, &task_op, sizeof(task)) == -1) {
        close(server_fd);
		return -1;
	}
    return 0;
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

    int man_fd = open(manager_pipe, O_RDONLY);

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
    if (start_server_connection(server_pipe, manager_pipe, type_request, box_path) == -1){
        perror("Error wille Connecting to the server.");
        close(server_fd);
        unlink(manager_pipe);
        return -1;
    }

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

