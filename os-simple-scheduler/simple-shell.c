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
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#define BUF_SIZE 1024 // maximum size to be allocated for the input buffer.
#define CMD_SIZE 256 // maximum size to be allocated for the number of commands
#define HIS_SIZE 512 // maximum size to be allocated for the number of commands to be saved in history
#define PROC_SIZE 1024 // maximum size to be allocated for the number of processes to submit
#define SHM_NAME "/my_shared_memory" // Name for shared memory object

int NCPU;
int TSLICE;
int scheduler_pid;

typedef struct Process{
    int pid;         // Process ID
    char cmd[BUF_SIZE];		// the exec file's name
    int status;		// 0:exec 1:wait -1:terminate
    int arrival;	// Arrival time
    int run; // Completion time
    int wait;    // Waiting time
} Process;

typedef struct PQ{
  	Process processes[PROC_SIZE];
    int head;
    int tail;
} PQ;

typedef struct {
  	int local_time;
    sem_t time_sem;
    sem_t readyPQ_sem;
    sem_t terminatedPQ_sem;
    PQ readyPQ;
    PQ terminatedPQ;
} SHM;

int shm_fd;
SHM *shm;

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

void signal_handler(int signum) {
    if (signum == SIGINT) { // handling the sigint signal.
        char buff[88] = "\n\nThankyou for using the custom shell...\nCopyrights: Rachit Bhandari & Akshat Lakhera.\n\n";
        write(STDOUT_FILENO, buff, 88); // writing custom message before terminating.
        exit(0);
    }
    else if (signum == SIGCHLD) {
      	int status;
    	int pid;

    	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        	sem_wait(&shm->terminatedPQ_sem); // Lock the queue
            sem_wait(&shm->readyPQ_sem);
            shm->readyPQ.processes[shm->readyPQ.head].status = -1;
            shm->terminatedPQ.processes[++shm->terminatedPQ.tail] = shm->readyPQ.processes[shm->readyPQ.head++];
        	sem_post(&shm->terminatedPQ_sem); // Unlock the queue
            sem_post(&shm->readyPQ_sem);
        }
    }
}

void setup_shm(){
	shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(SHM)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    shm = mmap(0, sizeof(SHM), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Initialize shared memory data
    sem_init(&shm->time_sem, 1, 1);
    sem_init(&shm->readyPQ_sem, 1, 1);
    sem_init(&shm->terminatedPQ_sem, 1, 1);
    shm->readyPQ.head = 0;
    shm->terminatedPQ.head = 0;
    shm->readyPQ.tail = -1;
    shm->terminatedPQ.tail = -1;
    shm->local_time = 0;
}

void setup_sigaction() {
    struct sigaction signal_action; // defining a variable of the sigaction structure.
    memset(&signal_action, 0, sizeof(signal_action)); // initializing the components of this structure to zero.
    signal_action.sa_handler = signal_handler; // telling the variable to handle the signal using the "sigint_handler" function.
    signal_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &signal_action, NULL); // installing the signal handler for sigint.
    sigaction(SIGCHLD, &signal_action, NULL); // installing the signal handler for sigchld.
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

//...........................................scheduler methods..........................................................

void scheduler_exec(){
    for (int i = 0; i <NCPU; i++) {
      	sem_wait(&shm->readyPQ_sem);
        if (shm->readyPQ.head+i <= shm->readyPQ.tail && shm->readyPQ.processes[shm->readyPQ.head].status == 1){
          	sem_post(&shm->readyPQ_sem);
            sem_wait(&shm->readyPQ_sem);
        	shm->readyPQ.processes[shm->readyPQ.head].status = 0; //running
        	shm->readyPQ.processes[shm->readyPQ.head].run += TSLICE;
            sem_wait(&shm->time_sem);
        	shm->readyPQ.processes[shm->readyPQ.head].wait = shm->local_time - shm->readyPQ.processes[shm->readyPQ.head].arrival - shm->readyPQ.processes[shm->readyPQ.head].run;
            sem_post(&shm->time_sem);
        	kill(shm->readyPQ.processes[shm->readyPQ.head].pid, SIGCONT);
            //printf("%d\n",getpid());
            sem_post(&shm->readyPQ_sem);
        }
        else sem_post(&shm->readyPQ_sem);
        usleep(TSLICE * 1000);
        sem_wait(&shm->readyPQ_sem);
        if (shm->readyPQ.head+i <= shm->readyPQ.tail && shm->readyPQ.processes[shm->readyPQ.head].status == 0) {
          	sem_post(&shm->readyPQ_sem);
          	sem_wait(&shm->readyPQ_sem);
        	kill(shm->readyPQ.processes[shm->readyPQ.head].pid, SIGSTOP);
        	shm->readyPQ.processes[shm->readyPQ.head].status = 1; // waiting
        	Process proc = shm->readyPQ.processes[shm->readyPQ.head++];
        	shm->readyPQ.processes[++shm->readyPQ.tail] = proc;
        	sem_post(&shm->readyPQ_sem);
        }
        else sem_post(&shm->readyPQ_sem);
    }
    sem_wait(&shm->time_sem);
    shm->local_time+=TSLICE;
    sem_post(&shm->time_sem);
}

void execute_scheduler_command(char* cmd) {
    char *argv[CMD_SIZE]; // creating an array of arguments
    char command[BUF_SIZE];
    strncpy(command, cmd, sizeof(command)); // creating a copy of the original command.
    char *token = strtok(cmd, " "); // tokenizing the command (splitting the command into parts where whitespace character is present).
	token = strtok(NULL, " ");
    int i=0;
    while(token != NULL) {
        argv[i++] = token; // adding individual tokens to the array.
        token = strtok(NULL, " ");
    }
    argv[i] = NULL; // declaring the end of array.
    int pid = fork();
    if (pid < 0) {
      perror("Unable to instantiate a child process to run the command");
    }
    else if (pid == 0) {
      	Process *proc = (Process *)malloc(sizeof(Process));
        strncpy(proc->cmd, argv[0], BUF_SIZE);
    	proc->pid = getpid();
        sem_wait(&shm->time_sem);
    	proc->arrival = shm->local_time;
        sem_post(&shm->time_sem);
        proc->status = 1; // waiting
        proc->run = 0;
        proc->wait = 0;
        sem_wait(&shm->readyPQ_sem);
    	shm->readyPQ.processes[++shm->readyPQ.tail] = *proc;
        sem_post(&shm->readyPQ_sem);
        if (execlp(argv[0], argv[0],NULL) == -1) { // executing and error checking the command. I have used execvp as we don't need to specify the whole path of the command, so that it is user-friendly.
            perror("Unable to execute the command");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
}

//......................................................................................................................

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
    if (strstr(cmd, "submit") != NULL) {
        execute_scheduler_command(cmd);
        return 1;
    }
    else if (strcmp(cmd, "exit") == 0) {
        display_detailed_history();
        int avg_completion_time = 0;
        int avg_wait_time = 0;
        int n = shm->terminatedPQ.tail-shm->terminatedPQ.head+1;
        while(shm->terminatedPQ.head<=shm->terminatedPQ.tail) {
          	Process proc = shm->terminatedPQ.processes[shm->terminatedPQ.head++];
        	printf("Command: %s\n",proc.cmd);
            printf("PID: %d\n",proc.pid);
            printf("Arrival Time: %d ms\n",proc.arrival);
            printf("Completion Time: %d ms\n",proc.run+proc.wait);
            avg_completion_time += proc.run+proc.wait;
            printf("Waiting Time: %d ms\n",proc.wait);
            avg_wait_time += proc.wait;
        }
        printf("\n	Average Completion Time: %d ms\n",avg_completion_time/n);
        printf("Avergae Waiting Time: %d ms\n",avg_wait_time/n);
        printf("\n\nThankyou for using the custom shell...\nCopyrights: Rachit Bhandari & Akshat Lakhera.\n\n");
        return 0;
    }
    else if (strcmp(cmd, "history") == 0) {
        display_history();
        return 1;
    }
    else if (strstr(cmd, "cd") != NULL) {
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
  	int status;
    char cmd[BUF_SIZE];
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

void init(){
//  	if (chdir("/") == -1) { // changes the current directory to the root directory.
//        perror("Unable to execute the command ");
//        exit(EXIT_FAILURE);
//    }

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
}

int main(int argc, char** argv) {
  	if (argc < 3) {
    	printf("Not enough arguments.\n");
        exit(1);
  	}
    NCPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);
    setup_shm();
    setup_sigaction(); // setting up our custom sigaction that handles the sigint signal in a custom way.
    init();
    scheduler_pid = fork();
    if (scheduler_pid < 0) {
      perror("Unable to fork a child process");
      exit(EXIT_FAILURE);
    }
    else if (scheduler_pid == 0) {
      while (1) scheduler_exec();
      exit(EXIT_SUCCESS);
    }
    launch_shell(); // launching our simple-shell.
    munmap(shm, sizeof(SHM));  // unmapping shared memory
    shm_unlink(SHM_NAME);	// removing shared object
    return 0;
}
