# Simple Shell

## Steps to use:

- Download the zip file
- Extract the file
- Right click and enter terminal
- Execute the following commands:
  
      gcc -o shell simple-shell.c
      ./shell
  
## Contribution:

1) Rachit Bhandari
- made the top down design of the working of shell, implemented the execution of normal commands as well as piped commands while handling the background processes as well.

2) Akshat Lakhera
- implented history, pipe history and did all the error handling.

## Implementation:

1) Continues an infinite loop until the shell is exited or terminated using the SIGINT signal.
2) Implements simple commands using execvp function in c.
3) Implements piped commands by forking multiple times.
4) Maintains a history of the commands.
  
> Credits: Rachit Bhandari - 2023413, Akshat Lakhera - 2023061, Sophomores @IIIT-Delhi.
