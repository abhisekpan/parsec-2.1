// main.cpp
//
// Created by Daniel Schwartz-Narbonne on 13/04/07.
// Modified by Christian Bienia
//
// Copyright 2007-2008 Princeton University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.


#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

#ifdef ENABLE_PARSEC_HOOKS
#include <hooks.h>
//Abhi===
#include <sched.h>
#include <stdbool.h>
#define __PARSEC_CPU_BASE "PARSEC_CPU_BASE"
#define __PARSEC_CPU_NUM "PARSEC_CPU_NUM"

pthread_barrier_t warmup_barrier;
//=======
#endif

#include "annealer_types.h"
#include "annealer_thread.h"
#include "netlist.h"
#include "rng.h"

using namespace std;

//Abhi===
struct ThreadArg {
  annealer_thread* t_obj;
  int tid;
};

void* entry_pt(void*);



int main (int argc, char * const argv[]) {
#ifdef PARSEC_VERSION
#define __PARSEC_STRING(x) #x
#define __PARSEC_XSTRING(x) __PARSEC_STRING(x)
        cout << "PARSEC Benchmark Suite Version "__PARSEC_XSTRING(PARSEC_VERSION) << endl << flush;
#else
        cout << "PARSEC Benchmark Suite" << endl << flush;
#endif //PARSEC_VERSION
#ifdef ENABLE_PARSEC_HOOKS
	__parsec_bench_begin(__parsec_canneal);
#endif

	srandom(3);

	if(argc != 5 && argc != 6) {
		cout << "Usage: " << argv[0] << " NTHREADS NSWAPS TEMP NETLIST [NSTEPS]" << endl;
		exit(1);
	}	
	
	//argument 1 is numthreads
	int num_threads = atoi(argv[1]);
	cout << "Threadcount: " << num_threads << endl;
#ifndef ENABLE_THREADS
	if (num_threads != 1){
		cout << "NTHREADS must be 1 (serial version)" <<endl;
		exit(1);
	}
#endif
		
	//argument 2 is the num moves / temp
	int swaps_per_temp = atoi(argv[2]);
	cout << swaps_per_temp << " swaps per temperature step" << endl;

	//argument 3 is the start temp
	int start_temp =  atoi(argv[3]);
	cout << "start temperature: " << start_temp << endl;
	
	//argument 4 is the netlist filename
	string filename(argv[4]);
	cout << "netlist filename: " << filename << endl;
	
	//argument 5 (optional) is the number of temperature steps before termination
	int number_temp_steps = -1;
        if(argc == 6) {
		number_temp_steps = atoi(argv[5]);
		cout << "number of temperature steps: " << number_temp_steps << endl;
        }

	//now that we've read in the commandline, run the program
	netlist my_netlist(filename);
	
	annealer_thread a_thread(&my_netlist,num_threads,swaps_per_temp,start_temp,number_temp_steps);
	
#ifdef ENABLE_PARSEC_HOOKS
	__parsec_roi_begin();
#endif
#ifdef ENABLE_THREADS
	std::vector<pthread_t> threads(num_threads);
	std::vector<ThreadArg> thread_arg(num_threads); //Abhi
	pthread_attr_t attr[num_threads]; //Abhi
	cpu_set_t mask[num_threads]; //Abhi
	//Abhi == initialize warm up barrier
	int ret = pthread_barrier_init(&warmup_barrier, NULL, num_threads);
	if (ret != 0) {
	  printf("Failed to initialize warmup barrier. Exiting...\n");
	  exit(1);
	}
	//====
	void* thread_in = static_cast<void*>(&a_thread);
	for(int i=0; i<num_threads; i++){
	  //Abhi===
	  if(pthread_attr_init(&attr[i])) {
	    fprintf(stderr,"Failed to initialize attribute for thread. Exiting...\n");
	    exit(1);
	  }
#ifdef ENABLE_PARSEC_HOOKS
	  bool set_range = false;
	  int cpu_base = 0, cpu_num = 0;
	  char *str_num = getenv(__PARSEC_CPU_NUM);
	  char *str_base = getenv(__PARSEC_CPU_BASE);
	  if ((str_num != NULL)&& (str_base != NULL)) {
	    cpu_num = atoi(str_num);
	    cpu_base = atoi(str_base);
	    set_range = true;
	  }
	  //set affinity
	  if(set_range) {
	    CPU_ZERO(&mask[i]);
	    int core_id = cpu_base + (i % cpu_num);
	    CPU_SET(core_id, &mask[i]);
	    if (!pthread_attr_setaffinity_np(&attr[i], sizeof(mask[i]), &mask[i])) {
	      fprintf(stderr, "thread %d bound to core %d\n", i, core_id);
	    } else {
	      fprintf(stderr, " Error: failure in setting affinity for thread %d\n", i);
	      exit(1);
	    }
	  }
#endif
	  thread_arg[i].tid = i;
	  thread_arg[i].t_obj = &a_thread;
//	  pthread_create(&threads[i], NULL, entry_pt,thread_in);
	  pthread_create(&threads[i], &attr[i], entry_pt,static_cast<void *>(&thread_arg[i]));
	  //=======
	}
	for (int i=0; i<num_threads; i++){
	  pthread_join(threads[i], NULL);
	  pthread_attr_destroy(&attr[i]); //Abhi
	}
#else
	a_thread.Run();
#endif
#ifdef ENABLE_PARSEC_HOOKS
	__parsec_roi_end();
#endif
	//cout << "sum: " << sum << std::endl;
	cout << "Final routing is: " << my_netlist.total_routing_cost() << endl;

#ifdef ENABLE_PARSEC_HOOKS
	__parsec_bench_end();
#endif

	return 0;
}

void* entry_pt(void* data)
{
  //Abhi======
  ThreadArg * thread_arg = static_cast<ThreadArg *> (data);
  annealer_thread* ptr = thread_arg->t_obj;
  //annealer_thread* ptr = static_cast<annealer_thread*>(data);
  int tid = thread_arg->tid;
  //thread binding to core is complete
#ifdef ENABLE_PARSEC_HOOKS
    bool set_range = false;
    int cpu_base = 0, cpu_num = 0;
    char *str_num = getenv(__PARSEC_CPU_NUM);
    char *str_base = getenv(__PARSEC_CPU_BASE);
    if ((str_num != NULL)&& (str_base != NULL)) {
      cpu_num = atoi(str_num);
      cpu_base = atoi(str_base);
      set_range = true;
    }
    //set affinity
    if(set_range) {
      int core_id = cpu_base + (tid % cpu_num);
      __parsec_binding_done((unsigned int)core_id);
    }
    //barrier to indicate end of warmup
    int wait_over = pthread_barrier_wait(&warmup_barrier);
    if (wait_over == PTHREAD_BARRIER_SERIAL_THREAD)  __parsec_warmup_over();
    else {
      if (wait_over != 0) {
	printf ("Error in barrier wait. Exiting...\n");
	exit(1);
      }
    }
#endif // ENABLE_PARSEC_HOOKS
//============
    //Abhi
	ptr->Run(tid);
}
