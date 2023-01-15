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

enum {
    OP_CODE_LOGIN_PUB = 1,
    OP_CODE_LOGIN_SUB = 2,
    OP_CODE_CREATE_BOX = 3,
    OP_CODE_REMOVE_BOX = 4,
    OP_CODE_LIST_BOX = 5,
    OP_CODE_LIST = 6,
    OP_CODE_REM_RESPONSE = 6,
    OP_CODE_LIS_RESPONSE = 8,
    OP_CODE_WRITE = 9,
    OP_CODE_READ = 10
};


typedef struct{
    char pipe_path[PIPE_PATH_MAX_SIZE];                        // Pointer to piper path name
    uint8_t opcode;                            //Type of task, deffined by the OP_CODE
    char box_name[MAX_BOX_NAME];
    char message[MESSAGE_MAX_SIZE];
    pthread_t thread;
}task;

typedef struct{
    char message[MESSAGE_MAX_SIZE];
    uint8_t opcode;
}message;

