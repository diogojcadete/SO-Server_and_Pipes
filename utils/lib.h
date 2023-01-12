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
#define PIPE_PATH_MAX_SIZE 32
#define MESSAGE_MAX_SIZE 1024
#define MAX_NUM_BOXES 1024 
#define PATH_MAX_SIZE 32
#define MAX_CLIENT_NAME (256)
#define BOX_NAME (32)
#define MESSAGE_SIZE (1024)
#define MAX_REQUEST_SIZE (1030)

enum {
	OP_CODE_LOGIN_PUB = 1,
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

enum{
    OP_CODE_PUB = 1,
    OP_CODE_MANAGER = 2,
    OP_CODE_SUBBSCRIBER = 3,
};

typedef struct{
    char opcode;                            //Type of task, deffined by the OP_CODE
	int session_id;
	char box_name[32];
	char pipe_path[PIPE_PATH_MAX_SIZE];
    char buffer[MAX_REQUEST_SIZE];          // Buffer to read requests
    pthread_mutex_t lock;                   //Pthread Look
    pthread_cond_t flag;                    //Pthred Flag
    int user_type;                          //Type of user, deffined by the enum
    bool not_building;                      //If is building a session
    pthread_t thread;
}task;

typedef struct{
    int fhandle;
    uint64_t n_publishers;
    uint64_t n_subscribers;
    uint64_t box_size;
    char box_name[32];
}mail_box;

