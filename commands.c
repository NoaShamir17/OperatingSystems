//commands.c
#define _XOPEN_SOURCE 500 // Enable POSIX extensions like setpgrp
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
	char* delimiters = " \t\n"; //parsing should be done by spaces, tabs or newlines
	char* token = strtok(line, delimiters); //get first token
    if(!token)
        return NULL_CMD; //this means no tokens were found, i.e., empty command
    cmd->cmd_name = strdup(token);
    if (!cmd->cmd_name) return MALLOC_FAIL;
	
	//fill cmd structure
    cmd->background = false;
	cmd->cmd_num = getInternalCommandNum(cmd->cmd_name);
	cmd->internal = !(cmd->cmd_num == EXTERNAL_CMD);
	cmd->num_args = 0;
	//strcpy(cmd->args[0], cmd->cmd_name); //first arg is the command name itself
    cmd->args[0] = strdup(cmd->cmd_name);
	for(int i = 1; i < ARGS_NUM_MAX; i++)
	{
		token = strtok(NULL, delimiters);
        //cmd->args[i] = strdup(token); //first arg NULL -> keep tokenizing from previous call
		if(token == NULL){//no more args
			if(i > 1  &&  strcmp(cmd->args[i-1],"&") == 0){ //check for background symbol '&'
				cmd->background = true;
                free(cmd->args[i-1]);
				cmd->args[i-1] = NULL; //remove '&' from args list
                cmd->num_args--; //do not count '&' as an argument
			}
			
			break;
		}
        cmd->args[i] = strdup(token); //token is not NULL
        if (!cmd->args[i]) return MALLOC_FAIL;
		if(strcmp(cmd->args[i],"&&") == 0){ //check for symbol '&&'
            free(cmd->args[i]);
			cmd->args[i] = NULL; //remove '&&' from args list
			cmd->nxt_cmd = (Command*)calloc(1, sizeof(Command));
			if(!cmd->nxt_cmd){
				return MALLOC_FAIL;
			}
			return parseCmd(NULL, cmd->nxt_cmd); //recursively parse next command
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
    if (cmd == NULL) return;

    // 1. Recursively free next commands in the chain
    if(cmd->nxt_cmd != NULL){
        freeCommand(cmd->nxt_cmd);
        cmd->nxt_cmd = NULL;
    }

    // 2. Free the command name (Missing in your original code)
    if (cmd->cmd_name != NULL) {
        free(cmd->cmd_name);
        cmd->cmd_name = NULL;
    }

    // 3. Free ALL arguments (0 to num_args)
    // args[0] is the command name copy, args[1..N] are arguments.
    // Using ARGS_NUM_MAX ensures we catch everything, which is safer given 
    // we use calloc/NULL pointers.
    for(int i = 0; i < ARGS_NUM_MAX + 1; i++){
        if (cmd->args[i] != NULL) {
            free(cmd->args[i]);
            cmd->args[i] = NULL;
        }
    }

    // 4. Free the struct itself
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
            return pwdCommand(cmd);
		case CD_CMD:
			return cdCommand(cmd, smash);
		case JOBS_CMD:
			return jobsCommand(cmd, smash);
		case KILL_CMD:
			return killCommand(cmd, smash);
		case FG_CMD:
			return fgCommand(cmd, smash);
		case BG_CMD:
			return bgCommand(cmd, smash);
        case QUIT_CMD:
            return quitCommand(cmd, smash);
        case DIFF_CMD:
            return diffCommand(cmd);
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
        printf("%s\n", target_dir);

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

CommandResult jobsCommand(Command* cmd, Smash* smash) {
    // Check for arguments
    if (cmd->num_args > 0) {
        perrorSmash("jobs", "expected 0 arguments");
        return SMASH_FAIL;
    }

    UpdateJobs(smash); // Always reap zombies before printing
    PrintJobs(smash->job_manager);
    return SMASH_SUCCESS;
}



CommandResult killCommand(Command* cmd, Smash* smash) {
    if (cmd->num_args != 2) {
        perrorSmash("kill", "invalid arguments");
        return SMASH_FAIL;
    }

    char* signum_str = cmd->args[1];
    char* jobid_str = cmd->args[2];

    if (!isNumber(signum_str) || !isNumber(jobid_str)) {
        perrorSmash("kill", "invalid arguments");
        return SMASH_FAIL;
    }
    // 1. Parse signal number directly
    // We expect a raw number string (e.g. "9").
    int signum = atoi(signum_str); 

    // 2. Validate signal number
    // Must be positive. If user entered "-9", atoi returns -9, which is <= 0.
    if (signum < 0) {
        perrorSmash("kill", "invalid arguments");
        return SMASH_FAIL;
    }

    int job_id = atoi(jobid_str);
    
    // 3. Verify job_id is valid
    // (job_id must be > 0, and verify it wasn't a parsing error)
    if (job_id < 0) { 
        perrorSmash("kill", "invalid arguments");
        return SMASH_FAIL; 
    }

    Job* job = GetJobById(smash->job_manager, job_id);
    if (job == NULL) {
        fprintf(stderr, "smash error: kill: job id %d does not exist\n", job_id);
        return SMASH_FAIL;
    }

    if (my_system_call(SYS_KILL, job->pid, signum) == -1) {
        perrorSmash("kill", "job cannot be signaled");
        return SMASH_FAIL;
    }
    
    printf("signal number %d was sent to pid %d\n", signum, job->pid);
    
    return SMASH_SUCCESS;
}

CommandResult fgCommand(Command* cmd, Smash* smash) {
    int job_id;
    Job* job;

    // 1. Parse Arguments
    if (cmd->num_args == 0) {
        // No args: Find the maximal job ID
        if (smash->job_manager->jobs_count == 0) {
            perrorSmash("fg", "jobs list is empty");
            return SMASH_FAIL;
        }
        
        // Find the job with the highest ID
        int max_id = -1;
        for (int i = 0; i < JOBS_NUM_MAX; i++) {
            if (smash->job_manager->jobs_list[i] != NULL) {
                max_id = i;
            }
        }
        job_id = max_id;
        
    } else if (cmd->num_args == 1) {
        // Specific Job ID: Validate it is numeric
        if (!isNumber(cmd->args[1])) {
             perrorSmash("fg", "invalid arguments");
             return SMASH_FAIL;
        }
        job_id = atoi(cmd->args[1]);
        
    } else {
        // Too many arguments
        perrorSmash("fg", "invalid arguments");
        return SMASH_FAIL;
    }

    // 2. Retrieve Job
    job = GetJobById(smash->job_manager, job_id);
    if (job == NULL) {
        fprintf(stderr, "smash error: fg: job id %d does not exist\n", job_id);
        return SMASH_FAIL;
    }

    // 3. Execution Logic
    // Format: [job_id] command arguments... (&): job_pid
    
    printf("[%d] %s", job_id, job->cmd->cmd_name);
    
    // Print all arguments
    for (int i = 1; i <= job->cmd->num_args; i++) {
        printf(" %s", job->cmd->args[i]);
    }
    
    // Print '&' if it was a background command
    if (job->cmd->background) {
        printf(" &");
    }

    printf(": %d", job->pid);
    printf("\n");


    // If the job was stopped, send SIGCONT to wake it up
    if (job->is_stopped) {
        if (my_system_call(SYS_KILL, job->pid, SIGCONT) == -1) {
            perrorSmash("fg", "kill failed");
            return SMASH_FAIL;
        }
        job->is_stopped = false;
    }

    // 4. Wait (BLOCKING)
    int status;
    int pid = job->pid;

    if (my_system_call(SYS_WAITPID, pid, &status, WUNTRACED) == -1) {
        perror("waitpid failed");
        return SMASH_FAIL;
    }

    // 5. Handle Status
    if (WIFSTOPPED(status)) {
        //by Ctrl+Z
        job->is_stopped = true;
    } else {
        RemoveJobById(smash->job_manager, job_id);
    }

    return SMASH_SUCCESS;
}


CommandResult bgCommand(Command* cmd, Smash* smash) {
    int job_id;
    Job* job;

    // 1. Parse Arguments
    if (cmd->num_args == 0) {
        Job* curr;
        int max_stopped_id = -1;
        for (int i = 0; i < JOBS_NUM_MAX; i++) {
            curr = smash->job_manager->jobs_list[i];
            if (curr != NULL && curr->is_stopped) {
                max_stopped_id = i;
            }
        }
        
        if (max_stopped_id == -1) {
            perrorSmash("bg", "there are no stopped jobs to resume");
            return SMASH_FAIL;
        }
        job_id = max_stopped_id;

    } else if (cmd->num_args == 1) {
        if (!isNumber(cmd->args[1])) {
             perrorSmash("bg", "invalid arguments");
             return SMASH_FAIL;
        }
        job_id = atoi(cmd->args[1]);
        
    } else {
        perrorSmash("bg", "invalid arguments");
        return SMASH_FAIL;
    }

    // 2. Retrieve Job
    job = GetJobById(smash->job_manager, job_id);
    if (job == NULL) {
        fprintf(stderr, "smash error: bg: job id %d does not exist\n", job_id);
        return SMASH_FAIL;
    }

    if (job->is_stopped == false) {
        fprintf(stderr, "smash error: bg: job id %d is already in background\n", job_id);
        return SMASH_FAIL;
    }

    // 3. Execution
    // Format: [job_id] command arguments... (&): job_pid
    printf("[%d] %s", job_id, job->cmd->cmd_name);
    
    for (int i = 1; i <= job->cmd->num_args; i++) {
        printf(" %s", job->cmd->args[i]);
    }
    
    if (job->cmd->background) {
        printf(" &");
    }
    
    printf(": %d", job->pid);
    printf("\n");

    if (my_system_call(SYS_KILL, job->pid, SIGCONT) == -1) {
        perrorSmash("bg", "kill failed");
        return SMASH_FAIL;
    }
    
    // Mark as running
    job->is_stopped = false;

    return SMASH_SUCCESS;
}



CommandResult quitCommand(Command* cmd, Smash* smash) {
    // 1. Check Argument Count
    // Error message from image: "smash error: quit: expected 0 or 1 arguments"
    if (cmd->num_args > 1) {
        perrorSmash("quit", "expected 0 or 1 arguments");
        return SMASH_FAIL;
    }

    // 2. Check Argument Validity
    if (cmd->num_args == 1) {
        // Error message from image: "smash error: quit: unexpected arguments"
        if (strcmp(cmd->args[1], "kill") != 0) {
            perrorSmash("quit", "unexpected arguments");
            return SMASH_FAIL;
        }

        // --- Execute 'quit kill' Logic ---
        
        
        for (int i = 0; i < JOBS_NUM_MAX; i++) {
            Job* job = smash->job_manager->jobs_list[i];
            
            if (job != NULL) {
                // Print format: [job_id] command - 
                printf("[%d] %s - ", job->job_id, job->cmd->cmd_name);
                
                // 1. Send SIGTERM
                printf("sending SIGTERM... ");
                fflush(stdout); // Force print before waiting
                my_system_call(SYS_KILL, job->pid, SIGTERM);

                // 2. Wait up to 5 seconds
                bool terminated = false;
                time_t start_time = time(NULL);
                int status;

                while (difftime(time(NULL), start_time) < 5) {
                    // Check if process has terminated (reap it if so)
                    pid_t result = my_system_call(SYS_WAITPID, job->pid, &status, WNOHANG);
                    
                    if (result == job->pid) {
                        terminated = true;
                        break;
                    }
                    sleep(1); // Wait 1 second before checking again
                }

                // 3. Output result
                if (terminated) {
                    printf("done\n");
                } else {
                    // Timeout reached (5 sec passed)
                    // Format from image: "... sending SIGTERM... sending SIGKILL... done"
                    printf("sending SIGKILL... done\n");
                    my_system_call(SYS_KILL, job->pid, SIGKILL);
                }
            }
        }
    }

    // --- CLEANUP MEMORY (Anti-Leak) ---
    freeSmash(smash);    
    freeCommand(cmd);    

    // --- EXIT SHELL ---
    exit(0);
    
    return SMASH_QUIT; 
}

CommandResult diffCommand(Command* cmd) {
    // 1. Check Argument Count
    if (cmd->num_args != 2) {
        perrorSmash("diff", "expected 2 arguments");
        return SMASH_FAIL;
    }

    char* file1 = cmd->args[1];
    char* file2 = cmd->args[2];
    int fd1, fd2;
    char dummy_buf[1]; 
    int res;

    // =========================================================
    // 2. Check Existence (using allowed wrapper SYS_OPEN)
    // =========================================================
    
    // Try to open file 1
    fd1 = my_system_call(SYS_OPEN, file1, O_RDONLY);
    if (fd1 < 0) {
        perrorSmash("diff", "expected valid paths for files");
        return SMASH_FAIL;
    }

    // Try to open file 2
    fd2 = my_system_call(SYS_OPEN, file2, O_RDONLY);
    if (fd2 < 0) {
        my_system_call(SYS_CLOSE, fd1);
        perrorSmash("diff", "expected valid paths for files");
        return SMASH_FAIL;
    }

    // =========================================================
    // 3. Check if Directory (using wrapper SYS_READ)
    // =========================================================
    // Strategy: Try to read 1 byte. On Linux, reading a directory returns -1 with errno=EISDIR.
    // We avoid using <sys/stat.h> completely.
    
    // Check File 1
    errno = 0;
    res = my_system_call(SYS_READ, fd1, dummy_buf, 1);
    if (res < 0 && errno == EISDIR) {
        my_system_call(SYS_CLOSE, fd1);
        my_system_call(SYS_CLOSE, fd2);
        perrorSmash("diff", "paths are not files"); //
        return SMASH_FAIL;
    }

    // Check File 2
    errno = 0;
    res = my_system_call(SYS_READ, fd2, dummy_buf, 1);
    if (res < 0 && errno == EISDIR) {
        my_system_call(SYS_CLOSE, fd1);
        my_system_call(SYS_CLOSE, fd2);
        perrorSmash("diff", "paths are not files");
        return SMASH_FAIL;
    }

    // =========================================================
    // 4. Reset Files (using direct lseek)
    // =========================================================
    // lseek is NOT in the forbidden list (image_5de09f.png), so we can use it directly.
    // We need to rewind because the checks above might have consumed a byte.
    
    lseek(fd1, 0, SEEK_SET);
    lseek(fd2, 0, SEEK_SET);

    // =========================================================
    // 5. Compare Content (Byte by Byte using wrapper SYS_READ)
    // =========================================================
    char c1, c2;
    int r1, r2;
    int same = 1;

    while (1) {
        r1 = my_system_call(SYS_READ, fd1, &c1, 1);
        r2 = my_system_call(SYS_READ, fd2, &c2, 1);

        // Check if both reads succeeded
        if (r1 < 0 || r2 < 0) {
            // Read error (shouldn't happen on valid files)
            same = 0;
            break;
        }

        // Check if one file ended but the other didn't
        if (r1 != r2) {
            same = 0;
            break;
        }

        // Check for EOF (both returned 0)
        if (r1 == 0) {
            break; 
        }

        // Compare the actual byte
        if (c1 != c2) {
            same = 0;
            break;
        }
    }

    my_system_call(SYS_CLOSE, fd1);
    my_system_call(SYS_CLOSE, fd2);

    // 6. Output and Return
    if (same) {
        printf("0\n");
        return SMASH_SUCCESS;
    } else {
        printf("1\n");
        return SMASH_FAIL;
    }
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

            RemoveJobByPid(smash->job_manager, pid); // Action: Remove job from your shell's job list
          
        } else if (WIFSTOPPED(status)) {
            // Child was stopped (e.g., by SIGTSTP/Ctrl+Z)
			MarkJobAsStopped(smash->job_manager, pid); // Action: Mark job as stopped in your shell's job list}
    	} else if (WIFCONTINUED(status)) {
            // Child resumed after being stopped
            Job* job = GetJobByPid(smash->job_manager, pid);
            if (job) {
                job->is_stopped = false;
            }
        }
    
		
	}
    // Handle the case where waitpid returns -1 due to an actual error
    // (excluding ECHILD, which just means there are no children left)
    if (pid == -1 && errno != ECHILD) {
        perror("waitpid error"); //TODO: is this the desired error handling?
    }
		
	
}


Job* CreateJob(Command* cmd, int pid){
	Job* new_job = (Job*)malloc(sizeof(Job));
	if (!new_job) {
        perrorSmash("CreateJob", "malloc failed");
		return NULL; 
	}

	new_job->pid = pid; 
	new_job->job_id = -1; // Assigned in addJob
	new_job->time_added_to_jobs = -1; // Assigned in addJob
	new_job->is_stopped = false; 

    // --- DEEP COPY COMMAND ---
    new_job->cmd = (Command*)malloc(sizeof(Command));
    if (!new_job->cmd) {
        perrorSmash("CreateJob", "malloc failed");
        free(new_job);
        return NULL;
    }

    // 1. Copy Primitives
    new_job->cmd->num_args = cmd->num_args;
    new_job->cmd->cmd_num = cmd->cmd_num;
    new_job->cmd->internal = cmd->internal;
    new_job->cmd->background = cmd->background;
    new_job->cmd->nxt_cmd = NULL; // Jobs track specific processes, not the chain

    // 2. Deep Copy Strings (cmd_name)
    if (cmd->cmd_name) {
        new_job->cmd->cmd_name = strdup(cmd->cmd_name);
    } else {
        new_job->cmd->cmd_name = NULL;
    }

    // 3. Deep Copy Strings (args array)
    for (int i = 0; i < ARGS_NUM_MAX; i++) {
        if (cmd->args[i] != NULL) {
            new_job->cmd->args[i] = strdup(cmd->args[i]);
        } else {
            new_job->cmd->args[i] = NULL;
        }
    }

	return new_job;
}

void addJob(Smash* smash, Job* job){
    UpdateJobs(smash);

    if (smash->job_manager->jobs_count >= JOBS_NUM_MAX) {
        perrorSmash("addJob", "too many jobs");
        freeJob(job);
        return;
    }

    int current_id = smash->job_manager->next_job_id;
    job->job_id = current_id;
    job->time_added_to_jobs = time(NULL);

    smash->job_manager->jobs_list[current_id] = job;
    smash->job_manager->jobs_count++;

    // Find new smallest ID (Scan upwards)
    int i = current_id + 1;
    while (i < JOBS_NUM_MAX) {
        if (smash->job_manager->jobs_list[i] == NULL) {
            break; 
        }
        i++;
    }
    smash->job_manager->next_job_id = i;
}

void freeJob(Job* job) {
    if (job == NULL) return;

    // Free the deep-copied command inside the job
    if (job->cmd) {
        freeCommand(job->cmd);
        job->cmd = NULL; // Good practice to nullify after freeing
    }

    // Free the job struct itself
    free(job);
}

void RemoveJobByPid(JobManager* job_manager, int pid){
	Job* job = GetJobByPid(job_manager, pid);
    if (job != NULL) {
        int job_id = job->job_id;
        job_manager->jobs_list[job_id] = NULL; // Remove from list
        freeJob(job); // Free the memory allocated for the job
        job_manager->jobs_count--;
        
        // Update next_job_id if needed
        if (job_id < job_manager->next_job_id) {
            job_manager->next_job_id = job_id;
        }
    }
}

void RemoveJobById(JobManager* job_manager, int job_id) {
    // 1. Validate ID range
    if (job_id < 0 || job_id >= JOBS_NUM_MAX) return;

    // 2. Get the job pointer
    Job* job = job_manager->jobs_list[job_id];

    if (job != NULL) {
        // 3. Free memory (using your existing helper)
        freeJob(job);

        // 4. Remove from list and update count
        job_manager->jobs_list[job_id] = NULL;
        job_manager->jobs_count--;

        // 5. Update next_job_id
        // If we freed a slot index smaller than the current "next" ID,
        // then THIS slot is now the new smallest available ID.
        if (job_id < job_manager->next_job_id) {
            job_manager->next_job_id = job_id;
        }
    }
}

void MarkJobAsStopped(JobManager* job_manager, int pid){
	Job* job = GetJobByPid(job_manager, pid);
    if (job != NULL) {
        job->is_stopped = true;
    }
}

Job* GetJobById(JobManager* job_manager, int job_id) {
    // 1. Check if the ID is within the valid range of the array
    if (job_id < 0 || job_id >= JOBS_NUM_MAX) {
        return NULL;
    }

    // 2. Return the job at that index
    // If the slot is empty, this will correctly return NULL.
    return job_manager->jobs_list[job_id];
}

Job* GetJobByPid(JobManager* job_manager, int pid) {
	for (int i = 0; i < JOBS_NUM_MAX; i++) {
		Job* job = job_manager->jobs_list[i];
		if (job != NULL && job->pid == pid) {
			return job;
		}
	}
	return NULL; // No job found with the given PID
}


void PrintJobs(JobManager* job_manager) {
    for (int i = 0; i < JOBS_NUM_MAX; i++) {
        Job* job = job_manager->jobs_list[i];
        if (job != NULL) {
            // Calculate elapsed time
            time_t now = time(NULL);
            double elapsed = difftime(now, job->time_added_to_jobs);
            
            // --- 1. Print Job ID and Command Name ---
            printf("[%d] %s", i, job->cmd->cmd_name);
            
            // --- 2. Print Arguments ---
            // num_args is the count of arguments after cmd_name (index 1 to num_args)
            for (int k = 1; k <= job->cmd->num_args; k++) {
                printf(" %s", job->cmd->args[k]);
            }
            
            // --- 3. Print Background Symbol, PID, Time, and Status ---
            printf("%s: %d %.0f secs", 
                   (job->cmd->background ? " &" : ""), // Conditionally adds " &"
                   job->pid, 
                   elapsed);
                   
            if (job->is_stopped) {
                printf(" (stopped)");
            }
            printf("\n");
        }
    }
}

// Main Cleanup Function: Frees the entire Shell state
void freeSmash(Smash* smash) {
    if (!smash) return;

    // 1. Clean up Job Manager
    if (smash->job_manager) {
        // Iterate through all possible slots (0 to MAX)
        for (int i = 0; i < JOBS_NUM_MAX; i++) {
            if (smash->job_manager->jobs_list[i] != NULL) {
                freeJob(smash->job_manager->jobs_list[i]);
                smash->job_manager->jobs_list[i] = NULL;
            }
        }
        
        // Free the manager container
        free(smash->job_manager);
        smash->job_manager = NULL;
    }

    // 2. Clean up Previous Path
    if (smash->prev_path) {
        free(smash->prev_path);
        smash->prev_path = NULL;
    }
}

//=============================================================
// helpers implementations
//=============================================================

bool isNumber(const char* str) {
    if (str == NULL || *str == '\0') {
        return false; // Empty or NULL string is not a valid number
    }
    
    // Check if the initial segment of 'str' consisting only of digits 
    // is equal to the total length of 'str'.
    // If they are equal, the string is entirely numeric.
    return strspn(str, "0123456789") == strlen(str);
}