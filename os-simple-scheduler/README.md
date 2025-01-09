# Simple Scheduler
This is a Simple Scheduler that helps schedule tasks.

## Steps to use:

- Download the zip file
- Extract the file
- Right click and enter terminal
- Execute the following commands:
  
      gcc -o shell simple-shell.c
      ./shell 4 100
      submit ./hw
  
  or
  
      gcc -o shell advanced-shell.c
      ./shell 4 100
      submit ./hw 1

## Contribution:

### 1) Rachit Bhandari
- did memory allocations, memory mapping, and coded the main logic of the scheduler.

### 2) Akshat Lakhera
- made all necessary structures and semaphores, and did error handling

## Implementation:

### Scheduler:

1) The scheduler is forked by the shell.
2) After every TSLICE milliseconds, it checks for NCPU number of processes and executes them for another TSLICE miliseconds.

> Credits: Rachit Bhandari - 2023413, Akshat Lakhera - 2023061, Sophomores @IIIT-Delhi.
