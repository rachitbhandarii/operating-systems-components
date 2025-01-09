#include <iostream>
#include <vector>
#include <chrono>
#include <pthread.h>
#include <functional>
#include <stdlib.h>
#include <cstring>

using namespace std;

// data structure to store all the execution times
vector<int> exec_time;

// main wrapper method start
int user_main(int argc, char **argv);

int main(int argc, char **argv) {
  // start message
  cout<<"====== Welcome to Simple-MultiThreader ======\n";
  // actual main
  int rc = user_main(argc, argv);
  // printing execution times of all threads before termination
  for (auto &time:exec_time) cout<<"Execution Time: "<<time<<" ms\n";
  // termination message
  cout<<"====== Credits: Rachit Bhandari, Akshat Lakhera ======\n";
  return rc;
}

#define main user_main
// main wrapper method end


// struct to keep the constraint values for each thread
struct Args {
  int low1,low2,high1,high2;
  function<void(int)> lambda_one_d;
  function<void(int, int)> lambda_two_d;
};

// thread creation helper method for one - dimensional parallel_for method
static void *thread_creator_one_d(void *t_args) {
  Args *args = (Args *) t_args;
  for (int i = args->low1; i < args->high1; i++) {
    args->lambda_one_d(i);
  }
  pthread_exit(nullptr);
}

// one - dimensional parallel_for method
void parallel_for(int low, int high, function<void(int)> &&lambda, int numThreads) {
  
  //setup  
  auto start_time = chrono::high_resolution_clock::now();

  if (numThreads < 1) {
    cerr<<"Atleast one thread should be provided to execute."<<endl;
    exit(1);
  }

  pthread_t threads[numThreads];
  vector<Args *> args(numThreads);
  int range = (high - low) / numThreads;
  // end of setup

  // iterating through the number of threads
  for (int i = 0; i < numThreads; i++) {
    // defining constraints for the individual threads
    int t_low = low + (i * range);
    int t_high = t_low + range;
    if (i+1 == numThreads) t_high = high;
    
    // adding constraints to the struct
    args[i] = new Args();
    args[i]->low1 = t_low;
    args[i]->high1 = t_high;
    args[i]->lambda_one_d = lambda;

    // creating pthread
    if (pthread_create(&threads[i], nullptr, thread_creator_one_d, static_cast<void *>(args[i])) != 0) {
      cerr<<"Error occured while creating thread "<<i<<endl;
      int j=0;
      // delete previously created threads
      while (j < i) pthread_cancel(threads[j++]);
      while (j < numThreads) {
        // join all the threads
        if (pthread_join(threads[j], nullptr) != 0) {
          cerr<<"Error occured while joining thread "<<j++<<endl;
        }
      }
      exit(1);
    }
  }

  // cleanup
  for (int i = 0; i < numThreads; i++) {
    if (pthread_join(threads[i], nullptr) != 0) {
      cerr<<"Error occured while joining thread "<<i<<endl;
      int j=0;
      // delete previously created threads
      while (j < i) pthread_cancel(threads[j++]);
      exit(1);
    }
    // clear memory
    delete args[i];
  }
  
  // add the execution time to be printed at the end.
  exec_time.push_back(chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now() - start_time).count());

}

// thread creation helper method for two - dimensional parallel_for method
static void *thread_creator_two_d(void *t_args) {
    Args *args = (Args *) t_args;
    for (int i = args->low1; i < args->high1; i++) {
      for (int j = args->low2; j < args->high2; j++) {
        args->lambda_two_d(i, j);
      }
    }
    pthread_exit(nullptr);
}

// two - dimensional parallel_for method
void parallel_for(int low1, int high1, int low2, int high2, function<void(int, int)> &&lambda, int numThreads) {

  // setup
  auto start_time = chrono::high_resolution_clock::now();

  if (numThreads < 1) {
    cerr<<"Atleast one thread should be provided to execute."<<endl;
    exit(1);
  }

  pthread_t threads[numThreads];
  vector<Args *> args(numThreads);
  int range = (high1 - low1) / numThreads;
  // end of setup

  // iterating through the number of threads
  for (int i = 0; i < numThreads; i++) {
    // defining constraints for the individual threads
    int t_low1 = low1 + (i * range);
    int t_high1 = t_low1 + range;
    if (i+1 == numThreads) t_high1 = high1;
    
    // adding constraints to the struct
    args[i] = new Args();
    args[i]->low1 = t_low1;
    args[i]->high1 = t_high1;
    args[i]->low2 = low2;
    args[i]->high2 = high2;
    args[i]->lambda_two_d = lambda;

    // creating pthread
    if (pthread_create(&threads[i], nullptr, thread_creator_two_d, static_cast<void *>(args[i])) != 0) {
      cerr<<"Error occured while creating thread "<<i<<endl;
      int j=0;
      // delete previously created threads
      while (j < i) pthread_cancel(threads[j++]);
      while (j < numThreads) {
        // join all the threads
        if (pthread_join(threads[j], nullptr) != 0) {
          cerr<<"Error occured while joining thread "<<j++<<endl;
        }
      }
      exit(1);
    }
  }

  // cleanup
  for (int i = 0; i < numThreads; i++) {
    if (pthread_join(threads[i], nullptr) != 0) {
      cerr<<"Error occured while joining thread "<<i<<endl;
      int j=0;
      // delete previously created threads
      while (j < i) pthread_cancel(threads[j++]);
      exit(1);
    }
    // clear memory
    delete args[i];
  }
  
  // add the execution time to be printed at the end.
  exec_time.push_back(chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now() - start_time).count());

}
