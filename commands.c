//commands.c
#include "commands.h"

//example function for printing errors from internal commands
void perrorSmash(const char* cmd, const char* msg)
{
    fprintf(stderr, "smash error:%s%s%s\n",
        cmd ? cmd : "",
        cmd ? ": " : "",
        msg);
}//TODO: check spaces in error message



//=============================================================
//command parsing and execution
//=============================================================


//fills the cmd structure by parsing the given line
ParsingResult parseCmd(char* line, Command* cmd)
{
	ParsingResult result;
	char* delimiters = " \t\n"; //parsing should be done by spaces, tabs or newlines
	cmd->cmd_name = strtok(line, delimiters); 
	if(!cmd->cmd_name)
		return NULL_CMD; //this means no tokens were found, i.e., empty command
	
	//fill cmd structure
	cmd->cmd_num = getInternalCommandNum(cmd->cmd_name);
	cmd->internal = !(cmd->cmd_num == EXTERNAL_CMD);
	cmd->num_args = 0;
	strcpy(cmd->args[0], cmd->cmd_name); //first arg is the command name itself
	for(int i = 1; i < ARGS_NUM_MAX; i++)
	{
		cmd->args[i] = strtok(NULL, delimiters); //first arg NULL -> keep tokenizing from previous call
		if(!cmd->args[i]){//no more args
			if(i > 1  &&  strcmp(cmd->args[i-1],"&") == 0){ //check for background symbol '&'
				cmd->background = true;
				cmd->args[i-1] = NULL; //remove '&' from args list
			}
			else{
				cmd->background = false;
			}
			break;
		}

		if(i > 1  &&  strcmp(cmd->args[i],"&&") == 0){ //check for symbol '&&'
			cmd->args[i] = NULL; //remove '&&' from args list
			line = NULL; //next command to parse is after the &&
			cmd->nxt_cmd = (Command*)malloc(sizeof(Command));
			if(!cmd->nxt_cmd){
				return MALLOC_FAIL;
			}
			return parseCmd(line, nxt_cmd); //recursively parse next command
		}
		cmd->num_args++;

	}
	cmd->args[cmd->num_args + 1] = NULL; //last arg is NULL (+1 bc args[0] is cmd name)

	return VALID_CMD;
}

CmdNum getInternalCommandNum(char *cmd_name) {
    const char* internal_cmds[] = {"showpid", "pwd", "cd", "jobs", "kill", "fg", "bg", "quit", "diff", NULL};
    
    for(int i = 0; internal_cmds[i] != NULL; i++) {
        if(strcmp(cmd_name, internal_cmds[i]) == 0) {
            // Since the array index (i) matches the enum value (CmdNum), 
            // we can safely cast the integer index to the enum type.
            return (CmdNum)i;
        }
    }
	return EXTERNAL_CMD; // Command not found in internal commands
}

// bool isInternalCommand(char *cmd_name){
// 	const char* internal_cmds[] = {"showpid", "pwd", "cd", "jobs", "kill", "fg", "bg", "quit", "diff", NULL};
// 	for(int i = 0; internal_cmds[i] != NULL; i++){
// 		if(strcmp(cmd_name, internal_cmds[i]) == 0){
// 			return true;
// 		}
// 	}
// 	return false;
// }

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
	UpdateJobs(smash); //update jobs list before executing new command
	int pid;
	int status;
	CommandResult cmd_result = SMASH_SUCCESS;

	if(!cmd->background && cmd->internal){
		//foreground & internal
		cmd_result = execInternalCommand(cmd, smash);
	}
	else{
		pid =my_system_call(SYS_FORK);
		if (pid < 0){
			ERROR_EXIT("fork failed\n");//TODO: is this the desired error handling?
		}
		else if (pid == 0){setpgrp();} //new process group
		
		if(!cmd->background && !cmd->internal){
			//foreground & external
			if(pid == 0) {
				//child process
				execExternalCommand(cmd);
			}
			else{
				//parent process
				my_system_call(SYS_WAITPID, pid, &status, 0);
				// CHECK THE CHILD'S EXIT STATUS
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                cmd_result = SMASH_FAIL; 
            } else if (WIFSIGNALED(status)) {
                // If killed by signal, treat as failure
                cmd_result = SMASH_FAIL;
            }
			}
		}
		else{
			//background
			Job* job = CreateJob(cmd, pid);
			addJob(smash, job);
			if(cmd->internal){
				//background & internal
				if(pid == 0){
					//child process
					execInternalCommand(cmd, smash); //TODO: check return value?
					exit(EXIT_SUCCESS);//TODO: is this exit needed?
				}
			}
			else{
				//background & external
				if(pid == 0) {
					//child process
					execExternalCommand(cmd);
				}
				else{
					//parent process
					printf("%d\n", pid); //TODO: needed? 
				}
			}
		}		
	}
	//execute next command in chain if exists
	if(cmd->nxt_cmd != NULL){
		if(cmd_result == SMASH_SUCCESS){
			// Only execute the next command if the current one succeeded
			executeCommand(cmd->nxt_cmd, smash);
		}
	}
}

void execExternalCommand(Command* cmd){
	my_system_call(SYS_EXECVP, cmd->cmd_name, cmd->args); 
	// --- EXECVP FAILED (It only returns if there was an error) ---
    
    // Check the specific error stored in errno
    if (errno == ENOENT) {
        // Error 1: The file was not found in the PATH
        fprintf(stderr, "smash error: external: cannot find program\n");
    } else {
        // any other execvp error
        fprintf(stderr, "smash error: external: invalid command\n");
    }

    // The child process must terminate after an execvp failure
    exit(EXIT_FAILURE);

}


//=============================================================
// internal commands implementations
//=============================================================


CommandResult execInternalCommand(Command* cmd, Smash* smash){
    switch(cmd->cmd_num){
        case SHOWPID_CMD:
            return showpidCommand(cmd, smash);
        case PWD_CMD:
            return pwdCommand();
        // ... (update all cases to 'return' the result) ...
        case QUIT_CMD:
            return quitCommand(cmd, smash);
        default:
            ERROR_EXIT("execInternalCommand: invalid internal command number\n");
    }
}

CommandResult showpidCommand(Command* cmd, Smash* smash) {
    if (cmd->num_args > 0) {
        // Strict error message as shown in the instructions image
        perrorSmash("showpid", "expected 0 arguments");
        return SMASH_FAIL;
    }
    
    printf("smash pid is %d\n", smash->smash_pid);
	return SMASH_SUCCESS;
}

CommandResult pwdCommand(Command* cmd) {
	if(cmd->num_args > 0) {
		// Strict error message as shown in the instructions image
		perrorSmash("pwd", "expected 0 arguments");
		return SMASH_FAIL;
	}
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
		return SMASH_SUCCESS;
    } else {
        perrorSmash("pwd", "getcwd failed");
		return SMASH_FAIL;
    }
}

CommandResult cdCommand(Command* cmd, Smash* smash) {
    if (cmd->num_args > 1) {
        perrorSmash("cd", "expected 1 arguments");
        return SMASH_FAIL;
    }

    char* target_dir = cmd->args[1];
    char current_dir[PATH_MAX];
    
    // Save current directory before changing
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perrorSmash("cd", "getcwd failed"); //TODO: is this the desired error handling?
        return SMASH_FAIL;
    }

    if (target_dir == NULL) {
        // 'cd' with no arguments usually does nothing or goes home. 
        // Returning success as it's a valid operation.
        return SMASH_SUCCESS;
    }

    if (strcmp(target_dir, "-") == 0) {
        if (smash->prev_path == NULL) {
            perrorSmash("cd", "old pwd not set");
            return SMASH_FAIL;
        }
        target_dir = smash->prev_path;
    }

    // Attempt to change directory
    if (chdir(target_dir) != 0) {
        // --- ERROR HANDLING LOGIC STARTS HERE ---
        
        if (errno == ENOENT) {
            // Case 1: Directory doesn't exist 
            // Output: smash error: cd: target directory does not exist
            perrorSmash("cd", "target directory does not exist");
            
        } else if (errno == ENOTDIR) {
            // Case 2: Path is not a directory
            // Output: smash error: cd: <path>: not a directory
            
            char error_msg[PATH_MAX + 30]; // buffer for path + error text
            sprintf(error_msg, "%s: not a directory", target_dir);
            perrorSmash("cd", error_msg);
            
        } else {
            // Fallback for other errors (e.g., EACCES permission denied)
            perrorSmash("cd", "chdir failed"); //TODO: is this the desired error handling?
        }
        
        return SMASH_FAIL;
    } 

    // Success: Update prev_path
	//if command is background, it doesnt affect parent's smash.prev_path 
	//bc its a different process with different data segment (where global vars are kept, including smash.prev_path)
    if (smash->prev_path) free(smash->prev_path);
    smash->prev_path = strdup(current_dir);
    
    return SMASH_SUCCESS;

}



//=============================================================
// Jobs list management implementations
//=============================================================

void UpdateJobs(Smash* smash){
    pid_t pid;
    int status;
    
    // Loop until waitpid returns 0 (no more finished/stopped children) or -1 (error)
    // -1: wait for ANY child
    // WNOHANG: do not block
    // WUNTRACED: report stopped children as well
    while ((pid = my_system_call(SYS_WAITPID,-1, &status, WNOHANG | WUNTRACED)) > 0) {
        
        // --- PID > 0: A job was successfully reaped ---
        
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // WIFEXITED(status) = true :Child terminated normally (e.g., using exit())
			// WIFSIGNALED(status) = true:Child terminated due to an uncaught signal (e.g., SIGKILL, SIGINT)

            RemoveJobByPid(smash->job_list, pid); // Action: Remove job from your shell's job list
          
        } else if (WIFSTOPPED(status)) {
            // Child was stopped (e.g., by SIGTSTP/Ctrl+Z)
			MarkJobAsStopped(smash->job_list, pid); // Action: Mark job as stopped in your shell's job list}
    }
    
    // Handle the case where waitpid returns -1 due to an actual error
    // (excluding ECHILD, which just means there are no children left)
    if (pid == -1 && errno != ECHILD) {
        perror("waitpid error"); //TODO: is this the desired error handling?
    }
		
	}
}


Job* CreateJob(Command* cmd, int pid){
	Job* new_job = (Job*)malloc(sizeof(Job));
	if (!new_job) {
		return NULL; // Memory allocation failed
		//TODO: handle malloc failure appropriately
	}
	new_job->cmd = cmd;
	new_job->pid = pid; 
	new_job->job_id = -1; // Not yet added to jobs list
	new_job->time_added_to_jobs = -1; // Not yet added to jobs list
	new_job->is_stopped = false; // Initially not stopped
	return new_job;
}

void addJob(Smash* smash, Job* job){
	UpdateJobs(smash); // Update jobs list before adding new job
	if (smash->job_manager->jobs_count >= JOBS_NUM_MAX) {
		// Job list is full, cannot add more jobs
		//TODO: handle this case appropriately
		return;
	}
	// Initialize job fields
	job->job_id = job_list->next_job_id;
	job->time_added_to_jobs = time(NULL); // Set the time when the job was added
	// Add job to the jobs list
	smash->job_manager->jobs_list[job_list->next_job_id] = job;
	smash->job_manager->jobs_count++;
	smash->job_manager->next_job_id++;
}


void RemoveJobByPid(JobManager* job_manager, int pid){
	Job* job;
	for (int i = 0; i < job_manager->jobs_count; i++) {
		job = job_manager->jobs_list[i];
		if (job->pid == pid) {
			// Found the job with the matching PID
			//Set next_job_id to min available ID
			if (job->job_id < job_manager->next_job_id) {
				job_manager->next_job_id = job->job_id;
			}
			// Remove it from the jobs list
			job_manager->jobs_list[i] = NULL;
			free(job); // Free the memory allocated for the job
			// Shift remaining jobs in the list
			}
			job_manager->jobs_count--;
			return;
		}
}

void MarkJobAsStopped(JobManager* job_manager, int pid){
	Job* job;
	for (int i = 0; i < job_manager->jobs_count; i++) {
		job = job_manager->jobs_list[i];
		if (job->pid == pid) {
			// Found the job with the matching PID
			job->is_stopped = true; // Mark the job as stopped
			return;
		}
	}
}

