#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <linux/limits.h>

#define BUF_SIZE 1024 // maximum size to be allocated for the input buffer.
#define CMD_SIZE 256 // maximum size to be allocated for the number of commands
#define HIS_SIZE 512 // maximum size to be allocated for the number of commands to be saved in history

typedef struct{
    char cmd[BUF_SIZE];
    int pid;
    bool bg;
    time_t st_time;
} History;

History history[HIS_SIZE];
int hist_itr = 0;

void display_history() {
    printf("History:\n");
    for (int i = 0; i < hist_itr; i++) {
        printf("%d %s\n",i+1,history[i].cmd);
    }
}

void display_detailed_history() {
    printf("History:\n");
    for (int i = 0; i < hist_itr; i++) {
        printf("%d %s\n", i+1, history[i].cmd);
        printf("    PID: %d\n", history[i].pid);
        printf("    Start Time: %s", ctime(&history[i].st_time));
        if (history[i].bg) printf("    Background Process\n");
        else printf("    Execution Time: %ld ms\n", time(NULL) - history[i].st_time);
    }
}

void sigint_handler(int signum) {
    if (signum == SIGINT) { // handling the sigint signal.
        char buff[88] = "\n\nThankyou for using the custom shell...\nCopyrights: Rachit Bhandari & Akshat Lakhera.\n\n";
        write(STDOUT_FILENO, buff, 88); // writing custom message before terminating.
        exit(0);
    }
}

void setup_sigaction() {
    struct sigaction signal_action; // defining a variable of the sigaction structure.
    memset(&signal_action, 0, sizeof(signal_action)); // initializing the components of this structure to zero.
    signal_action.sa_handler = sigint_handler; // telling the variable to handle the signal using the "sigint_handler" function.
    sigaction(SIGINT, &signal_action, NULL); // installing the signal handler.
}

void strip(char* str) {
    int k=0;
    while (k<BUF_SIZE && str[k] != '\n') ++k;
    str[k] = '\0'; // changing the newline character to the end of the string.

    int len = strlen(str); // getting the length of the string.
    int l = 0, r = len-1;

    while (isspace(str[l])) ++l; // shifting the beginning pointer to the point of first non-whitespace character.
    while (r >= 0 && isspace(str[r])) --r; // shifting the ending pointer to the point of last non-whitespace character.

    int i=0,j=l;
    while(j<=r) str[i++] = str[j++]; // copying all those elements to the start.
    str[i] = '\0'; // declaring the end of string.
}

void execute_cd_command(char* cmd) {
    char *argv[3]; // creating an array of arguments
    char command[BUF_SIZE];
    strncpy(command, cmd, sizeof(command)); // creating a copy of the original command.
    char *token = strtok(cmd, " "); // tokenizing the command (splitting the command into parts where whitespace character is present).

    int i=0;
    while(token != NULL) {
        argv[i++] = token; // adding individual tokens to the array.
        token = strtok(NULL, " ");
    }
    argv[i] = NULL; // declaring the end of array.
    if (argv[1] == NULL) {
        argv[1] = "/"; // if no path is specified then change directory to the root directory.
        argv[2] = NULL;
    }

    if (chdir(argv[1]) == -1) { // changing the directory and doing the error handling.
        perror("Unable to execute the cd command");
    }

    if (hist_itr < HIS_SIZE) {
        strncpy(history[hist_itr].cmd, command, BUF_SIZE);
        history[hist_itr].pid = getpid();
        time(&history[hist_itr].st_time);
        history[hist_itr].bg = false;
        hist_itr++;
    }
    else {
        perror("Cannot add more entries to history.\n");
    }

}

void execute_single_command(char* cmd, bool bg) {
    char *argv[CMD_SIZE]; // creating an array of arguments
    char command[BUF_SIZE];
    strncpy(command, cmd, sizeof(command)); // creating a copy of the original command.
    char *token = strtok(cmd, " "); // tokenizing the command (splitting the command into parts where whitespace character is present).

    int i=0;
    while(token != NULL) {
        argv[i++] = token; // adding individual tokens to the array.
        token = strtok(NULL, " ");
    }
    argv[i] = NULL; // declaring the end of array.

    int pid = fork(); // creating a child process to run the command.

    if (pid < 0) { // handling the error that could be occured while creating the child process.
        perror("Unable to instantiate a child process to run the command");
    }
    else if (pid == 0) {
        if (bg && setsid() == -1) { // creating background process and error handling.
            perror("Unable to create a background process");
        }
        if (execvp(argv[0], argv) == -1) { // executing and error checking the command. I have used execvp as we don't need to specify the whole path of the command, so that it is user-friendly.
            perror("Unable to execute the command");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    else {
        if (bg) printf("Background process instantiated with PID: %d\n",pid); // writing the pid of the background process created.
        else if (waitpid(pid, NULL,0) == -1) // waiting for child process only if it is not a background process and doing the error handling.
                perror("Unable to wait for a child process");
    }

    if (hist_itr < HIS_SIZE) {
        strncpy(history[hist_itr].cmd, command, BUF_SIZE);
        history[hist_itr].pid = pid;
        time(&history[hist_itr].st_time);
        history[hist_itr].bg = false;
        hist_itr++;
    }
    else {
        perror("Cannot add more entries to history.\n");
    }

}

void execute_piped_command(char* cmd, bool bg) {
    char* argv[BUF_SIZE]; // creating an array of arguments
    char command[BUF_SIZE];
    strncpy(command, cmd, sizeof(command)); // creating a copy of the original command.
    char* token = strtok(cmd, "|"); // tokenizing the command (splitting the command into parts where pipe character is present).
    int i = 0;
    while (token != NULL) {
        argv[i++] = token;
        strip(argv[i-1]); // adding individual tokens to the array.
        token = strtok(NULL, "|");
    }
    argv[i] = NULL; // declaring the end of array.
    int pid;
    int no_of_pipes = i-1; // getting the number of pipes in the command.
    int pipes[no_of_pipes][2]; // declaring file descriptors.

    for (int i = 0; i <= no_of_pipes; i++) {
        if (i < no_of_pipes) {
            if (pipe(pipes[i]) == -1) { // creating pipe and handling error.
                perror("Unable to create pipe");
            }
        }

        pid = fork(); // creating a child process to run the command.
        if (pid < 0) { // handling the error that could be occured while creating the child process.
            perror("Unable to instantiate a child process to run the command");
        }
        else if (pid == 0) {
            if (bg && setsid() == -1) { // creating background process and error handling.
                perror("Unable to create a background process");
            }

            if (i>0) { // doesn't run for the first command.
                dup2(pipes[i-1][0], STDIN_FILENO);
                close(pipes[i-1][0]);
                close(pipes[i-1][1]); // closing both the pipes.
            }

            if (i<no_of_pipes) { // doesn't run for the last command.
                dup2(pipes[i][1], STDOUT_FILENO);
                close(pipes[i][0]);
                close(pipes[i][1]); // closing both the pipes.
            }

            char *argargv[CMD_SIZE]; // creating an array of arguments
            token = strtok(argv[i], " "); // tokenizing the command (splitting the command into parts where pipe character is present).
            int j = 0;
            while (token != NULL) {
                argargv[j++] = token; // adding individual tokens to the array.
                token = strtok(NULL, " ");
            }
            argargv[j] = NULL; // declaring the end of array.

            if (execvp(argargv[0], argargv) == -1) { // executing the command and performing error handling.
                perror("Unable to execute the command");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }
        else {
            if (i>0) {
                close(pipes[i-1][0]);
                close(pipes[i-1][1]); // closing both the pipes.
            }
            if (bg) printf("Background process instantiated with PID: %d\n",pid); // writing the pid of the background process created.
            else if (waitpid(pid, NULL,0) == -1) // waiting for child process only if it is not a background process and doing the error handling.
                perror("Unable to wait for a child process");
        }
    }

    if (hist_itr < HIS_SIZE) {
        strncpy(history[hist_itr].cmd, command, BUF_SIZE);
        history[hist_itr].pid = pid;
        time(&history[hist_itr].st_time);
        history[hist_itr].bg = bg;
        hist_itr++;
    }
    else {
        perror("Cannot add more entries to history.\n");
    }
}

int handle_single_command(char* cmd) {
    if (strcmp(cmd, "exit") == 0) {
        display_detailed_history();
        printf("\n\nThankyou for using the custom shell...\nCopyrights: Rachit Bhandari & Akshat Lakhera.\n\n");
        return 0;
    }
    if (strcmp(cmd, "history") == 0) {
        display_history();
        return 1;
    }
    if (strstr(cmd, "cd") != NULL) {
        execute_cd_command(cmd);
        return 1;
    }
    else {
        if (strstr(cmd, "&") != NULL) {
            strtok(cmd, "&");
            execute_single_command(cmd,true);
        }
        else execute_single_command(cmd,false);
        return 1;
    };

}

void launch_shell() {
    if (chdir("/") == -1) { // changes the current directory to the root directory.
        perror("Unable to execute the command ");
        exit(EXIT_FAILURE);
    }

    int status;
    char cmd[BUF_SIZE];
    char* init[] = {"clear",NULL};

    int pid = fork();
    if (pid < 0) {
        perror("Unable to instantiate a child process to run the command");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) {
        if (execvp(init[0],init) == -1) { // clears the screen.
            perror("Unable to execute the command ");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    else wait(NULL);

    printf("~welcome to the simple-shell~\n\n"); // initial message.

    do
    {
        char cwd[PATH_MAX];

        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("custom@shell:~%s$ ",cwd);
        }
        else { // error handling for getcwd ( get current working directory).
            perror("Cannot fetch current directory");
            exit(EXIT_FAILURE);
        }
        if (fgets(cmd, sizeof(cmd), stdin) == NULL) { // reading the command from stdin using "fgets" function and handling error.
            perror("Unable to read from stdin.");
            exit(EXIT_FAILURE);
        }

        strip(cmd); // stripping the whitespace characters from left and right side.

        if (strstr(cmd,"|") != NULL)
        {
            if (strstr(cmd, "&") != NULL) {
                strtok(cmd, "&");
                execute_piped_command(cmd,true);
            }
            else execute_piped_command(cmd,false);
            status=1;
        } // handling piped commands.
        else status = handle_single_command(cmd); // handling single commands.

    } while (status); // running the loop until the shell is terminated by the user.
}

int main() {
    setup_sigaction(); // setting up our custom sigaction that handles the sigint signal in a custom way.
    launch_shell(); // launching our simple-shell.
    return 0;
}
