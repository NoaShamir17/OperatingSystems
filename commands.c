//commands.c
#include "commands.h"

//example function for printing errors from internal commands
void perrorSmash(const char* cmd, const char* msg)
{
    fprintf(stderr, "smash error:%s%s%s\n",
        cmd ? cmd : "",
        cmd ? ": " : "",
        msg);
}

//fills the cmd structure by parsing the given line
ParsingResult parseCmd(char* line, Command* cmd)
{
	ParsingResult result;
	char* delimiters = " \t\n"; //parsing should be done by spaces, tabs or newlines
	cmd->cmd_name = strtok(line, delimiters); 
	if(!cmd->cmd_name)
		return NULL_CMD; //this means no tokens were found, i.e., empty command
	
	//fill cmd structure
	cmd->internal = isInternalCommand(cmd->cmd_name);
	cmd->num_args = 0;
	for(int i = 0; i < ARGS_NUM_MAX; i++)
	{
		args[i] = strtok(NULL, delimiters); //first arg NULL -> keep tokenizing from previous call
		if(!args[i]){//no more args
			if(i > 0  &&  strcmp(args[i-1],"&") == 0){ //check for background symbol '&'
				cmd->background = true;
				args[i-1] = NULL; //remove '&' from args list
			}
			else{
				cmd->background = false;
			}
			break;
		}

		if(i > 1  &&  strcmp(args[i],"&&") == 0){ //check for symbol '&&'
			args[i] = NULL; //remove '&&' from args list
			line = NULL; //next command to parse is after the &&
			cmd->nxt_cmd = (Command*)malloc(sizeof(Command));
			if(!cmd->nxt_cmd){
				return MALLOC_FAIL;
			}
			return parseCmd(line, nxt_cmd); //recursively parse next command
		}
		cmd->num_args++;

	}

	return VALID_CMD;
}

bool isInternalCommand(char *cmd_name){
	const char* internal_cmds[] = {"showpid", "pwd", "cd", "jobs", "kill", "fg", "bg", "quit", "diff", NULL};
	for(int i = 0; internal_cmds[i] != NULL; i++){
		if(strcmp(cmd_name, internal_cmds[i]) == 0){
			return true;
		}
	}
	return false;
}

void freeCommand(Command* cmd){
	if(cmd->nxt_cmd != NULL){
		freeCommand(cmd->nxt_cmd);
	}
	free(cmd->cmd_name);
	for(int i = 0; i < cmd->num_args; i++){
		free(cmd->args[i]);
	}
	free(cmd);
}


void executeCommand(Command* cmd, Smash* smash){
	int pid;
	Process *prc = MALLOC_VALIDATED(Process, sizeof(Process));
	prc->cmd = cmd;
	prc->pid = -1; //not yet started
	prc->job_id = -1; //not yet added to jobs list
	prc->time_added_to_jobs = -1; //not yet added to jobs list

	if(cmd->internal && !cmd->background){
		//internal & foreground
		execInternalCommand(&prc, smash);
	}
	else{
		pid =my_system_call(SYS_FORK);
		if (pid < 0){
			ERROR_EXIT("fork failed\n");
		}
		else if (pid == 0){
			//child process
			if(cmd->internal){
				//internal & background
				execInternalCommand(&prc, smash);
			}
			else{
				//external command
				execExternalCommand(&prc, smash);
			}
			exit(0); //should never reach here
		}
		else{
			//parent process
			prc->pid = pid;
			if(!cmd->background){
				//foreground process
				int status;
				my_system_call(SYS_WAITPID, pid, &status, 0);
			}
			else{
				//background process
				addJob(smash->jobs, prc);
				printf("[%d] %d\n", prc->job_id, prc->pid);
			}
		}

		//execute next command in chain if exists
		if(cmd->nxt_cmd != NULL)
		
		execExternalCommand(&prc, smash);
	}
	
}



//=============================================================
// internal commands implementations
//=============================================================


void execInternalCommand(Process *prc, Smash* smash){
	//switch(prc->cmd->cmd_name){
		//case "showpid":
			//showpid(prc, smash);
			//break;
		//case "pwd":
			//pwd(prc, smash);
			//break;
		//...
}