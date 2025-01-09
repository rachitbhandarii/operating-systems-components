# Simple Multithreader
This is a Simple Multithreader that helps execute one - dimensional as well as two - dimensional computations parallely using pthreads.

## Steps to use:

- Download the zip file
- Extract the file
- Right click and enter terminal
- Execute the following commands:
  
      make
      ./vector
      ./matrix

## Contribution:

1) Rachit Bhandari
- coded the parallel_for methods

2) Akshat Lakhera
- cleanup & main wrapper class

## Implementation:

1. The parallel_for method is invoked
2. Threads are created to divide the tasks through constraints
3. The lambda function is executed by the threads
4. Threads are joined
5. Cleanup is done
6. The execution time is printed
7. (Assumption) The lambda function doesn't lead to race condition, as locking the lambda using mutex would not lead to parallelization of tasks, which is the reason to use pthreads.

> Credits: Rachit Bhandari - 2023413, Akshat Lakhera - 2023061, Sophomores @IIIT-Delhi.
