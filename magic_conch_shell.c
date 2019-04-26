/* 
 * File:   magic_conch_shell.c
 * Author: Thorsten Passfeld
 *
 * Created on 3. April 2019, 12:23
 */

#include <time.h>

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

extern char** environ;
static bool exit_val;

void formatValue(char* rawValue, char* value, char escapedLimiter) {
    //Check the string character by character to disallow open quotes
    bool foundClosingQuote = 0;
    int insertIndex = 0;
    for (int i = 1; i < strlen(rawValue); i++) {
        char current = rawValue[i];
        if (current == escapedLimiter) {
            foundClosingQuote = 1;
            //value[insertIndex] = ' ';
            break;
        }
        value[insertIndex] = rawValue[i];
        insertIndex++;
    }
    //String was not properly enclosed in single quotes. Empty the value.
    if (!foundClosingQuote) {
        strcpy(value, "");
    }
}

void handleExport(char** args, int args_size) {
    //Input comes in like  export test="abc" test2=5 ...
    //Or just export test=abc or export test='abc'

    //Parse every argument except for the command itself
    for (int i = 1; i < args_size; i++) {
        //Continue if no "=" sign was found
        if (strchr(args[i], '=') == NULL)
            continue;

        //To handle if the supplied command is like -> export test=
        bool valueWasEmpty = 0;
        //The target variable
        char* target = strtok(args[i], "=");

        char* rawValue = strtok(NULL, "=");
        char* value = NULL;
        if (rawValue == NULL) {
            //Still write an empty string
            value = (char*) calloc(1, sizeof (char));
            valueWasEmpty = 1;
        } else {
            //Allocate memory and set it to zero, so that the string would not
            //accidentally get reused in the next iteration.
            value = (char*) calloc(strlen(rawValue) + 1, sizeof (char));
        }

        if (valueWasEmpty) {
            //Set the value to an empty string
            strcpy(value, "");
        } else {
            //Check for and try to parse single quotes
            if (strchr(rawValue, '\'') != NULL) {
                formatValue(rawValue, value, '\'');
            } else if (strchr(rawValue, '\"') != NULL) {
                formatValue(rawValue, value, '\"');
            } else {
                strcpy(value, rawValue);
            }
        }

        //printf("DEBUG: Setting environment variale \"%s\" to \"%s\"\n", target, value);
        setenv(target, value, 1);
        free(value);

    }

}

//Just for fun
void printConchShell() {
    char* filename = "conchShell.txt";

    FILE* fptr = NULL;

    if ((fptr = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "error opening %s\n", filename);
        return;
    }
    char read_string[6510];

    while (fgets(read_string, sizeof (read_string), fptr) != NULL)
        printf("%s", read_string);

    fclose(fptr);

    printf("\n\n");
}

void printPrompt() {

    char cwd[2048];

    getcwd(cwd, sizeof (cwd));

    //Windows - Cygwin
    //printf("\033[36m%s@%s:\033[32m%s\033[0m> ", getenv("USERNAME"), getenv("COMPUTERNAME"), cwd);

    //Linux/UNIX
    size_t MAX_HOSTNAME_LENGTH = 128;
    char hostname[MAX_HOSTNAME_LENGTH + 1];
    hostname[MAX_HOSTNAME_LENGTH] = 0; //Null termination in case the hostname is too long

    if (gethostname(hostname, MAX_HOSTNAME_LENGTH) != 0) {
        printf("Error while getting the hostname.");
    }

    printf("\033[36m%s@%s:\033[32m%s\033[0m> ", getenv("USER"), hostname, cwd);

}

char* getInput(int* errorCode) {
    //Get input from user
    char* input = NULL;
    size_t inputSize = 0;

    getline(&input, &inputSize, stdin); //Takes care of malloc/realloc. I love this function, it is the best!

    if (input[0] == '\n') {
        *errorCode = 1; //Error -> Empty input
    } else {

        input[strcspn(input, "\n")] = 0; //Remove the trailing newline
        *errorCode = 0;
    }

    return input;
}

void handleEnvVariables(char*** parsed_input, int parsed_input_size) {
    for (int i = 0; i < parsed_input_size; i++) {
        if ((*parsed_input)[i][0] != '$') {
            continue;
        }

        char* envVariable = strtok((*parsed_input)[i], "$");
        if (envVariable == NULL) {
            printf("No dollar sign was found.\n");
            continue;
        }

        char* returnedVariable = getenv(envVariable);
        if (returnedVariable == NULL) {
            printf("The variable associated with %s was not found.\n", envVariable);
            continue;
        }
        //Allocate enough memory
        (*parsed_input)[i] = (char*) realloc((*parsed_input)[i], (strlen(returnedVariable) + 1) * sizeof (char));
        if ((*parsed_input)[i] == NULL) {
            exit(EXIT_FAILURE); //Not enough free memory
        }

        strcpy((*parsed_input)[i], returnedVariable);

    }
}

//Handle builtin commands that do something that is only possible in the parent process.
bool handleBuiltins1(char** args, int args_size) {
    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting now...\n");
        exit_val = 1;
        return 1;

    } else if (strcmp(args[0], "cd") == 0) {
        char dir[2048];
        if (args_size < 2) {
            //Default to the home directory
            strcpy(dir, getenv("HOME")); //Linux/Unix
            //strcpy(dir, getenv("USERPROFILE")); //Windows - Cygwin
        } else {
            //Use the first supplied argument instead
            strcpy(dir, args[1]);
        }
        int error = chdir(dir);
        if (error) {
            perror("Error");
        }
        return 1;

    } else if (strcmp(args[0], "export") == 0) {
        if (args_size < 2) {
            printf("Error: No arguments supplied.\n");
        } else {
            handleExport(args, args_size);
        }
        return 1;
    }

    return 0; //No built-in command recognized
}

//Handle builtin commands that only output something as text.
bool handleBuiltins2(char** args, int args_size) {
    if (strcmp(args[0], "set") == 0) {
        //Just print all environment variables. Maybe improve this later?
        for (char **env = environ; *env != 0; env++) {
            char *currentEnv = *env;
            printf("%s\n", currentEnv);
        }
        return 1;

        //Just for fun
    } else if (strcmp(args[0], "conchShell") == 0) {
        //Print ASCII art
        printConchShell();
        int r = rand() % 3;
        if (r == 0)
            printf("=======> No.\n");
        else if (r == 1)
            printf("=======> Try asking again.\n");
        else
            printf("=======> Maybe someday.\n");
        printf("\n\e[1;36m>> The magic conch shell has spoken! <<\033[0m\n");

        return 1;
    }

    return 0; //No built-in command recognized
}

void addNullTermination(char*** args, int num_words) {
    //Allocate memory for the next string, which will be a null pointer
    *args = (char**) realloc(*args, (num_words + 1) * sizeof (**args));
    if (*args == NULL) {
        exit(EXIT_FAILURE); //No more free memory
    }
	
	//This has been commented out, because free ignores NULL pointers, so there would be a memory leak.
	//However, this solution without allocating memory for the NULL terminating string is sub-optimal, but seems to work.
	
    //printf("(char*) (*args)[%d] -- Allocating 1 byte for the NULL-terminating string -> %s.\n", num_words, (char*)0);
    //Allocate just enough memory for the null pointer(hopefully one byte?)
    //(*args)[num_words] = (char*) malloc(sizeof (char));
    //if ((*args)[num_words] == NULL) {
    //    exit(EXIT_FAILURE); //Not enough free memory
    //}

    //Add the null pointer to the string array as the next index.
    (*args)[num_words] = (char*) 0;

    //return(num_words + 1);
}

void pipe_commands(char**** args, int num_separated_args, int* parsed_input_sizes) {

    //2 Commands brauchen 1 Pipe - 3 brauchen 2 Pipes - 4 brauchen 3 Pipes,...
    int pipefd[num_separated_args - 1][2];

    pid_t pids[num_separated_args];
    int status[num_separated_args];

    //Create the pipes
    for (int i = 0; i < num_separated_args - 1; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("Error -- Pipe");
			//Close all pipes that were opened until now
            for (int j = 0; j < i; j++) {
              close(pipefd[j][0]);
              close(pipefd[j][1]);
            }
	    return;
        }
    }

    int pipeCounter = 0;
    for (int i = 0; i < num_separated_args; i++) {
        //printf("The current pipeCounter is -> %d\n", pipeCounter);
        pids[i] = fork();
        if (pids[i] == 0) {
            //Child process
            if (i == 0) {
                //printf("This is the first process with pipeCounter %d.\n", pipeCounter);

                dup2(pipefd[pipeCounter][1], STDOUT_FILENO);
                //close(pipefd[pipeCounter][0]);

            } else if (i == (num_separated_args - 1)) {
                //printf("This is the last process with pipeCounter %d.\n", pipeCounter);
                dup2(pipefd[pipeCounter][0], STDIN_FILENO);
            } else {
                //printf("This is a process in the middle with pipeCounter %d.\n", pipeCounter);
                //Write to the next fd[1]
                dup2(pipefd[pipeCounter + 1][1], STDOUT_FILENO);
                //Also read from the current fd[0]
                dup2(pipefd[pipeCounter][0], STDIN_FILENO);
            }
            //Close all pipes before exec
            for (int i = 0; i < num_separated_args - 1; i++) {
                close(pipefd[i][0]);
                close(pipefd[i][1]);
            }

            //Before executing anything else, check for and handle built-in commands.
            bool usedBuiltinCmd = handleBuiltins2((*args)[i], parsed_input_sizes[i]);
            if (!exit_val && !usedBuiltinCmd) {
                char** currentArr = (*args)[i];

                addNullTermination(&currentArr, parsed_input_sizes[i]);

                (*args)[i] = currentArr;

                //Execute the command
                if (execvp((*args)[i][0], (*args)[i]) == -1) {
                    perror("Error -- Exec");
                    //Exec has failed. Free up the used memory and exit the child process.
                    for (int i = 0; i < num_separated_args; i++) {
                        for (int j = 0; j < parsed_input_sizes[i]; j++) {
                            free((*args)[i][j]);
                        }
                        free((*args)[i]);
                    }
                    free(*args);
                    free(parsed_input_sizes);

                    exit(EXIT_FAILURE);
                }
            } else {
                //The builtin command has finished. Free up the memory to be safe and then exit the child process.
                for (int i = 0; i < num_separated_args; i++) {
                    for (int j = 0; j < parsed_input_sizes[i]; j++) {
                        free((*args)[i][j]);
                    }
                    free((*args)[i]);
                }
                free(*args);
                free(parsed_input_sizes);

                exit(EXIT_SUCCESS);
            }

        } else if (pids[i] < 0) {
            //Error while forking
            perror("Error -- Fork");
        } else {
            //Parent process
            if (i != 0 && i != (num_separated_args - 1)) {
                pipeCounter++;
            }
        }
    }
	
	//The parent now closes all open pipes and waits for every child process to finish.
    for (int i = 0; i < num_separated_args - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
	
    for (int i = 0; i < num_separated_args; i++) {
        //printf("Now waiting for child with pid %d\n", pids[i]);
        waitpid(pids[i], &status[i], 0);

        int returnStatus = WEXITSTATUS(status[i]);
        //printf("===> The process with PID %d exited with status \"%d\".\n", pids[i], returnStatus);
    }

}

void splitOnDelims(char* input, char*** parsed_input, int* num_words, char* delims) {
    int num_tokens = 0;
    char* token = strtok(input, delims);

    int i = 0;
    while (token != NULL) {
        num_tokens++;

        //printf("(char**) *parsed_input -- Reallocating string array with %d bytes for %d elements.\n", (num_tokens) * sizeof (**parsed_input), num_tokens);
        *parsed_input = (char**) realloc(*parsed_input, (num_tokens) * sizeof (**parsed_input));
        if (*parsed_input == NULL) {
            exit(EXIT_FAILURE); //Not enough free memory
        }

        //printf("(char*) (*parsed_input)[%d] -- Allocating %d bytes for string -> %s\n", i, sizeof (char) * (strlen(token) + 1), token);
        //Allocate just enough memory for the string plus a terminating character
        (*parsed_input)[i] = (char*) malloc(sizeof (char) * (strlen(token) + 1));
        if ((*parsed_input)[i] == NULL) {
            exit(EXIT_FAILURE); //Not enough free memory
        }

        strcpy((*parsed_input)[i], token);

        token = strtok(NULL, delims);
        i++;

    }
    *num_words = num_tokens;
}

void handleInput(char* input, char**** parsed_input, int** parsed_input_sizes, int* num_separated_args) {

    //Temporary string array for everything separated by the | symbol
    char** tempParsedPipes = NULL;
    int tempParsedPipesSize = 0;

    //Splitting the input into an array for every command
	//printf("(char**) tempParsedPipes -- Now entering splitOnDelims.\n");
    splitOnDelims(input, &tempParsedPipes, &tempParsedPipesSize, "|");

    //Allocate memory for all command arrays of strings in parsed_input
	//printf("(char***) *parsed_input -- Allocating %d bytes for the pipe array.\n", tempParsedPipesSize * sizeof (char**));
    *parsed_input = (char***) malloc(tempParsedPipesSize * sizeof (char**));

    //Allocate memory for all numbers of arguments
	//printf("(int*) *parsed_input_sizes -- Allocating %d bytes for the size array for every single pipe.\n", tempParsedPipesSize * sizeof (int));
    *parsed_input_sizes = (int*) malloc(tempParsedPipesSize * sizeof (int));

    //Remember the number of arguments separated by the | symbol
    *num_separated_args = tempParsedPipesSize;

    //Parse each found command separated by pipes individually
    for (int i = 0; i < tempParsedPipesSize; i++) {
        //printf("Now parsing pipe number %d -> %s\n", i, tempParsedPipes[i]);
        int tempNumArgs = 0;
        char** tempParsedArgs = NULL;

        //printf("(char**) tempParsedArgs -- Now entering splitOnDelims.\n");
        splitOnDelims(tempParsedPipes[i], &tempParsedArgs, &tempNumArgs, " ");

        //Remember the number of arguments of this string array
        (*parsed_input_sizes)[i] = tempNumArgs;

        //Parse environment variables and replace tokens with them
        handleEnvVariables(&tempParsedArgs, tempNumArgs);

        //Allocate memory for the strings
		//printf("(char**) (*parsed_input)[%d] -- Allocating %d bytes for the string array with %d elements/arguments.\n", i, tempNumArgs * sizeof (char*), tempNumArgs);
        (*parsed_input)[i] = (char**) malloc(tempNumArgs * sizeof (char*));
        if ((*parsed_input)[i] == NULL) {
            exit(EXIT_FAILURE); //Not enough free memory
        }

        for (int j = 0; j < tempNumArgs; j++) {
            //Insert tempParsedArgs into parsed_input
            //This is supposed to be a string, for example the ls in "ls -als" and later also the "-als"
			//printf("(char*) (*parsed_input)[%d][%d] -- Allocating %d bytes for the string -> %s.\n", i, j, (strlen(tempParsedArgs[j]) + 1) * sizeof (char), tempParsedArgs[j]);
            (*parsed_input)[i][j] = (char*) malloc((strlen(tempParsedArgs[j]) + 1) * sizeof (char));
            if ((*parsed_input)[i][j] == NULL) {
                exit(EXIT_FAILURE); //Not enough free memory
            }

            //Copy the string into parsed_input
            strcpy((*parsed_input)[i][j], tempParsedArgs[j]);

            //printf("(char*) tempParsedArgs[%d] -- Deleting temporary string -> %s\n", j, tempParsedArgs[j]);
            free(tempParsedArgs[j]);
        }
        //Free memory
        //printf("(char**) tempParsedArgs -- Deleting the temporary string array itself.\n");
        free(tempParsedArgs);
        //printf("(char*) tempParsedPipes[%d] -- Deleting temporary string -> %s\n", i, tempParsedPipes[i]);
        free(tempParsedPipes[i]);
    }
    //printf("(char**) tempParsedPipes -- Deleting the temporary string array itself\n");
    free(tempParsedPipes);
}

void handleCommandExecution(char**** parsed_input, int** parsed_input_sizes, int num_separated_args) {

    if (handleBuiltins1((*parsed_input)[0], (*parsed_input_sizes)[0]) == 0) {
        pipe_commands(parsed_input, num_separated_args, *parsed_input_sizes);
    }
}

int main(int argc, char** argv) {
    //Magic Conch Shell
    srand(time(NULL));
    printf("\e[1;36m>> The Magic Conch Shell <<\033[0m\n");

    exit_val = 0;
    while (!exit_val) {
        char* input = NULL;
        //parsed_input[0] == first command(multiple strings) ---- parsed_input[1] == second command,... -> First and second meaning "First | Second"
        char*** parsed_input = NULL; //Array of arrays of strings for every piped command
        int errorCode = 0;
        int num_separated_args = 0;
        int* parsed_input_sizes = NULL; //Array for the sizes of each individual string array

        printPrompt();

        input = getInput(&errorCode);
        if (errorCode) {
            continue; //Empty input
        }

        //Parse the input accordingly
        handleInput(input, &parsed_input, &parsed_input_sizes, &num_separated_args);
        //The input string is not needed anymore. Free it.
        free(input);

        //Now handle the command execution appropriately
        handleCommandExecution(&parsed_input, &parsed_input_sizes, num_separated_args);

        //printf("---Deleting arrays in main()---\n");
        for (int i = 0; i < num_separated_args; i++) {
            for (int j = 0; j < parsed_input_sizes[i]; j++) {
                //printf("(char*) parsed_input[%d][%d] -- Deleting string -> %s\n", i, j, parsed_input[i][j]);
                free(parsed_input[i][j]);
            }
            //printf("(char**) parsed_input[%d] -- Deleting the string array.\n", i);
            free(parsed_input[i]);
        }
        //printf("(char***) parsed_input -- Deleting the string array array itself.\n");
        free(parsed_input);
        //printf("(int*) parsed_input_sizes -- Deleting the int array itself.\n");
        free(parsed_input_sizes);
    }

    return (EXIT_SUCCESS);
}