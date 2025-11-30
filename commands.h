#ifndef COMMANDS_H
#define COMMANDS_H
/*=============================================================================
* includes, defines, usings
=============================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h> //Noa added
#include <stdbool.h> //Noa added
#include <unistd.h> //Noa added for getpid()


#define CMD_LENGTH_MAX 120
#define ARGS_NUM_MAX 20
#define JOBS_NUM_MAX 100

/*=============================================================================
* error handling - some useful macros and examples of error handling,
* feel free to not use any of this
=============================================================================*/
#define ERROR_EXIT(msg) \
    do { \
        fprintf(stderr, "%s: %d\n%s", __FILE__, __LINE__, msg); \
        exit(1); \
    } while(0);

static inline void* _validatedMalloc(size_t size)
{
    void* ptr = malloc(size);
    if(!ptr) ERROR_EXIT("malloc");
    return ptr;
}

// example usage:
// char* bufffer = MALLOC_VALIDATED(char, MAX_LINE_SIZE);
// which automatically includes error handling
#define MALLOC_VALIDATED(type, size) \
    ((type*)_validatedMalloc((size)))


/*=============================================================================
* error definitions
=============================================================================*/
typedef enum  {
    VALID_CMD = 0,
	INVALID_CMD,
    NULL_CMD,
    MALLOC_FAIL
	//feel free to add more values here or delete this
} ParsingResult;

typedef enum {
	SMASH_SUCCESS = 0,
	SMASH_QUIT,
	SMASH_FAIL
	//feel free to add more values here or delete this
} CommandResult;


//=============================================================
// global structs/type definitions
//=============================================================

typedef struct {
    char* cmd_name;
    int num_args;
    char* args[ARGS_NUM_MAX];
    bool internal;
    bool background;
    Command *nxt_cmd;
} Command;

typedef struct {
    Command* cmd;
    int pid;
    int job_id;
    int time_added_to_jobs;
} Process;

typedef struct {
    Command* jobs_list[JOBS_NUM_MAX];
    int jobs_count;
} JobsList;

typedef struct {
    int smash_pid;
    char* prev_path;
    JobsList* jobs;
} Smash;


/*=============================================================================
* global functions
=============================================================================*/
int parseCommandExample(char* line);
bool isInternalCommand(char *cmd_name);
void freeCommand(Command* cmd);
void executeCommand(Command* cmd, Smash* smash);

//=============================================================
// internal commands sugnatures
//=============================================================

#endif //COMMANDS_H