# smallsh
This small shell written for CS 374: Operating Systems I at Oregon State University provides a prompt for running commands. 

## Features
- Handles blank lines and comments (lines beginning with the # character)
  - To comment out a line, use `#`. For example, `# this is a comment`.
- Executes the commands `exit`, `cd`, and `status` via code built into the shell
- Executes other commands by creating new processes
- Supports input and output redirection
- Supports running commands in foreground and background processes
  - Background processes can be ran by ending a command with `&`. For example, `status &` will run the `status` command in the background and return its PID.
- Implements custom handlers for 2 signals, `SIGINT` and `SIGTSTP`

## Usage
The syntax for a command is as follows, where items in square brackets are optional:

`command [arg1 arg2 ...] [< input_file] [> output_file] [&]`

## Credit
Sample code for parsing commands was used, and credit belongs to the professors of the class, Guillermo Tonsmann and Nauman Chaudhry.
<details>
  <summary>Click here for the original parser code.</summary>
  
```
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_LENGTH 2048
#define MAX_ARGS		 512


struct command_line
{
	char *argv[MAX_ARGS + 1];
	int argc;
	char *input_file;
	char *output_file;
	bool is_bg;
};


struct command_line *parse_input()
{
	char input[INPUT_LENGTH];
	struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

	// Get input
	printf(": ");
	fflush(stdout);
	fgets(input, INPUT_LENGTH, stdin);

	// Tokenize the input
	char *token = strtok(input, " \n");
	while(token){
		if(!strcmp(token,"<")){
			curr_command->input_file = strdup(strtok(NULL," \n"));
		} else if(!strcmp(token,">")){
			curr_command->output_file = strdup(strtok(NULL," \n"));
		} else if(!strcmp(token,"&")){
			curr_command->is_bg = true;
		} else{
			curr_command->argv[curr_command->argc++] = strdup(token);
		}
		token=strtok(NULL," \n");
	}
	return curr_command;
}

int main()
{
	struct command_line *curr_command;

	while(true)
	{
		curr_command = parse_input();

	}
	return EXIT_SUCCESS;
}
```
</details>
