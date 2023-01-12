#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdint.h>
#include "errno.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#define FILE_NAME_MAX_SIZE 256
#define PIPE_PATH_MAX_SIZE 32
#define MESSAGE_MAX_SIZE 1024
#define MAX_NUM_BOXES 1024 
#define PATH_MAX_SIZE 32

enum {
	OP_CODE_LOGIN_PUB= 1,
    OP_CODE_LOGIN_SUB = 2,
	OP_CODE_CREATE = 3,
    OP_CODE_CR_RESPONSE = 4,
	OP_CODE_REMOVE = 5,
	OP_CODE_REM_RESPONSE = 6,
    OP_CODE_LIST = 7,
    OP_CODE_LIS_RESPONSE = 8,
	OP_CODE_WRITE = 9,
	OP_CODE_READ = 10
};

typedef struct{
    char opcode;
	int session_id;
	char box_name[32];
	char pipe_path[PIPE_PATH_MAX_SIZE];
}task;

typedef struct{
    int fhandle;
    uint64_t n_publishers;
    uint64_t n_subscribers;
    uint64_t box_size;
    char box_name[32];
}mail_box;

