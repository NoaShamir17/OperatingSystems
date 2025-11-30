//smash.c

/*=============================================================================
* includes, defines, usings
=============================================================================*/
#include <stdlib.h>

#include "commands.h"
#include "signals.h"

/*=============================================================================
* classes/structs declarations
=============================================================================*/

/*=============================================================================
* global variables & data structures
=============================================================================*/
Smash smash;
char _line[CMD_LENGTH_MAX];

smash.smash_pid = getpid();
smash.prev_path = NULL;
smash.jobs = MALLOC_VALIDATED(JobsList, sizeof(JobsList));

/*=============================================================================
* main function
=============================================================================*/
int main(int argc, char* argv[])
{
	while(1) {
		printf("smash > ");
		fgets(_line, CMD_LENGTH_MAX, stdin);
		//execute command
		Command* cmd = MALLOC_VALIDATED(Command, sizeof(Command));
		switch(parseCmd(_line, cmd)) {
			case VALID_CMD:
				//command is valid, execute it
				executeCommand(cmd, &smash);
				break;
			case INVALID_CMD:
				//invalid command, print error
				//TODO: check is that is the desired error message
				//TODO: chech whether to print to stdout or stderr
				fprintf(stderr, "smash error: external: cannot find program\n");
				break;
			case NULL_CMD:
				//empty command, do nothing
				break;
			case MALLOC_FAIL:
				freeCommand(cmd);
				ERROR_EXIT("malloc failed\n");
				break;
		}
		freeCommand(cmd);
		
		//initialize buffers for next command
		_line[0] = '\0';
	}

	return 0;
}
