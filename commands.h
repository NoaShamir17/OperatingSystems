#ifndef COMMANDS_H
#define COMMANDS_H
/*=============================================================================
* includes, defines, usings
=============================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h> 
#include <unistd.h> 
#include <errno.h> //TODO: is this needed?
#include <time.h>
#include <limits.h> //for PATH_MAX


#define CMD_LENGTH_MAX 120
#define ARGS_NUM_MAX 20
#define JOBS_NUM_MAX 100


typedef enum {
    SHOWPID_CMD = 0,
    PWD_CMD = 1,
    CD_CMD = 2,
    JOBS_CMD = 3,
    KILL_CMD = 4,
    FG_CMD = 5,
    BG_CMD = 6,
    QUIT_CMD = 7,
    DIFF_CMD = 8,
    // Explicit value for commands that are NOT internal
    EXTERNAL_CMD = 9 
} CmdNum;

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

typedef struct Command Command;
typedef struct Job Job;
typedef struct JobManager JobManager;
typedef struct Smash Smash;

typedef struct {
    char* cmd_name;
    int num_args;
    char* args[ARGS_NUM_MAX+1]; //args[0] is the command name itself, last arg is NULL
    bool internal;
    CmdNum cmd_num;
    bool background;
    Command *nxt_cmd;
} Command;

typedef struct {
    Command* cmd;
    int pid;
    int job_id;
    int time_added_to_jobs;
    bool is_stopped;
} Job;

typedef struct {
    Job* jobs_list[JOBS_NUM_MAX];
    int jobs_count;
    int next_job_id;
} JobManager;

typedef struct {
    int smash_pid;
    char* prev_path;
    JobManager* job_manager;
} Smash;


/*=============================================================================
* global functions
=============================================================================*/

//--------------Command parsing and execution----------------
int parseCommand(char* line);
CmdNum getInternalCommandNum(char *cmd_name);
//bool isInternalCommand(char *cmd_name);
void freeCommand(Command* cmd);
void executeCommand(Command* cmd, Smash* smash);
void execExternalCommand(Command* cmd);
CommandResult execInternalCommand(Command* cmd, Smash* smash);

//--------------Internal commands----------------
CommandResult showpidCommand(Command* cmd,Smash* smash);
CommandResult pwdCommand(Command* cmd);
CommandResult cdCommand(Command* cmd, Smash* smash);
CommandResult jobsCommand(Command* cmd, Smash* smash);
CommandResult killCommand(Command* cmd, Smash* smash);
CommandResult fgCommand(Command* cmd, Smash* smash);
CommandResult bgCommand(Command* cmd, Smash* smash);
CommandResult quitCommand(Command* cmd, Smash* smash);
CommandResult diffCommand(Command* cmd);





//-------------Jobs list management----------------
void UpdateJobs(Smash* smash); 
Job* CreateJob(Command* cmd, int pid);
void addJob(Smash* smash, Job* job);
void freeJob(Job* job);
void RemoveJobByPid(JobManager* job_manager, int pid);
void MarkJobAsStopped(JobManager* job_manager, int pid);
Job* GetJobById(JobManager* job_manager, int job_id);
Job* GetJobByPid(JobManager* job_manager, int pid);
void PrintJobs(JobManager* job_manager);
void freeSmash(Smash* smash);

//=============================================================
// internal commands sugnatures
//=============================================================


//TODO:need to implement:
//execInternalCommand
//CreateJob
//CheckJobs


#endif //COMMANDS_H
