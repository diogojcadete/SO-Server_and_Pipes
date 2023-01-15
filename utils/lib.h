#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdint.h>
#include "errno.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#define FILE_NAME_MAX_SIZE 256
#define PIPE_PATH_MAX_SIZE 256
#define MESSAGE_MAX_SIZE 1024
#define MAX_NUM_BOXES 1024 
#define PATH_MAX_SIZE 32
#define MAX_BOXES 32
#define MESSAGE_SIZE 1024
#define MAX_REQUEST_SIZE 1030
#define FILE_MAX_SIZE 1024
#define MAX_BOX_NAME 32
#define MAX_ERROR_SIZE 1024

enum {
    OP_CODE_LOGIN_PUB = 1,
    OP_CODE_LOGIN_SUB = 2,
    OP_CODE_CREATE_BOX = 3,
    OP_CODE_CREATE_BOX_RESPONSE = 4,
    OP_CODE_REMOVE_BOX = 5,
    OP_CODE_REMOVE_BOX_RESPONSE = 6,
    OP_CODE_LIST = 7,
    OP_CODE_LIST_BOXES_RESPONSE = 8,
    OP_CODE_WRITE = 9,
    OP_CODE_READ = 10
};

typedef struct{
    char *box_name;
    uint8_t last;
    uint64_t box_size;
    uint64_t num_publishers;
    uint64_t num_subscribers;
    char subs_array_pipe_path[1024];
}mail_box;


typedef struct{
    char pipe_path[PIPE_PATH_MAX_SIZE];                        // Pointer to piper path name
    uint8_t opcode;                            //Type of task, deffined by the OP_CODE
    char box_name[MAX_BOX_NAME];
    char message[MESSAGE_MAX_SIZE];
    pthread_t thread;
    char error[MAX_ERROR_SIZE];
    int return_value;
    uint64_t box_size;
    uint8_t last;
}task;

typedef struct{
    char message[MESSAGE_MAX_SIZE];
    uint8_t opcode;
}message;

task string_to_task(char* building) {
    task builder;
    memset(builder.pipe_path, 0, sizeof(builder.pipe_path));
    memset(builder.box_name, 0, sizeof(builder.box_name));
    memset(builder.error, 0, sizeof(builder.error));
    memset(builder.message, 0, sizeof(builder.message));
    sscanf(building, "%s|", &builder.opcode);
    switch(builder.opcode){
        case(OP_CODE_LOGIN_PUB):
        case(OP_CODE_LOGIN_SUB):
        case(OP_CODE_CREATE_BOX):
        case(OP_CODE_REMOVE_BOX):
            sscanf(building, "%s|%s|%s", &builder.opcode, builder.pipe_path, builder.box_name);
            return builder;
        case(OP_CODE_CREATE_BOX_RESPONSE):
        case(OP_CODE_REMOVE_BOX_RESPONSE):
            sscanf(building, "%s|%d|%s", &builder.opcode, &builder.return_value, builder.message);
            return builder;
        case(OP_CODE_LIST):
            sscanf(building, "%s|%s", &builder.opcode, builder.pipe_path);
            return builder;  
          /**  
        case(OP_CODE_LIST_BOXES_RESPONSE):
            sscanf(building, "%s|%d|%s|%llu|%llu|%llu", &builder.opcode, builder.last, builder.box_name, &builder.box_size, &builder.num_publishers, &builder.num_subscribers);
            return builder;

            */
        case(OP_CODE_WRITE):
        case(OP_CODE_READ):
            sscanf(building, "%s|%s", &builder.opcode, builder.message);
            return builder;
    }
    exit(EXIT_FAILURE);
}
