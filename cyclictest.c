/*

Ref. from RHEL Hardware Certification 1.0, Test Suite User Guide, A.1.1.31:

"... cyclictest, which starts a measurement thread per cpu, running at a high realtime priority. These threads have a period (100 microseconds) where they perfom the following caculation:

  1. get a timestamp (t1)
  2. sleep for period
  3. get a second timestamp (t2)
  4. latency = t2 - (t1 + period)
  5. goto 1

The latency is the time difference between the theoretical wakeup time (t1+period) and the actual wakeup time (t2). Each measurement thread tracks minimum, maximum and average latency as well as reporting each datapoint.

Once cyclictest is running, rteval starts a pair of system loads, one being a parallel linux kernel compile and the other being a scheduler benchmark called hackbench.

... “


make : gcc cyclictest,c -o cyclictest -lrt

usage : cyclictest [-t sleepPeriodTimeInSeconds]
                   [-i iterations]
                   [-c cpuUnderTest]
                   [-p priority]
                   [-r]
        -t : sleep for n seconds, default is 1s.
             每次迭代睡眠时间，默认为1秒。
        -i : iterations, default is 3.
             迭代次数，默认为3次。
        -c : running under which cpu, from 0 to max-1, default is unspecified.
             指定在哪个CPU上运行，从0开始数，默认不指定。
        -p : priority, from 0 to 99, default is unspecified.
             指定进程优先级，0-99，默认指不定。
        -r : uses realtime clock TSC Register.
             使用TSC寄存器获得纳秒级时钟，否则使用微秒级时钟。

samples : cyclictest
          cyclictest -t 2 -i 10 -c 1 -p 1
          cyclictest -t2 -i10 -c1 -p1


v0.1 by ZZX @ 2020-04-03, Beijing
(C)CESI

 */

#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>
#include <pthread.h>


long main(int argc, char* argv[])
{
  struct timeval tv[2];
  struct timezone tz;
  struct timespec ts[2];
  int cpu = -1;
  int slp = 1;
  int ite = 3;
  int pri = -1;

  int created_thread = 0;
  cpu_set_t mask;
  cpu_set_t get;

  pthread_attr_t pthattr;
  int policy;
  static char policy_fifo[]  = "SCHED_FIFO";
  static char policy_rr[]    = "SCHED_RR";
  static char policy_other[] = "SCHED_OTHER";
  struct sched_param param;
#define POLICY_TO_STRING(p) ((p)==SCHED_FIFO?policy_fifo:(p)==SCHED_RR?policy_rr:policy_other)

  long sec;
  long usec;
  long nsec;
  long avg = 0;
  long max = 0;
  long min = 1999999999;
  long lat = 0;
  int use_TSC = 0;

  int i;
  int ch;

  while ( (ch=getopt(argc, argv, "t:p:i:c:rhv")) != EOF )
  {
    switch(ch)
    {
    case 'r':
      use_TSC = 1;
      break;
    case 'c':
      cpu = atoi(optarg);
      if( cpu >= sysconf(_SC_NPROCESSORS_CONF) || cpu < 0)
      {
        printf("ERR:out of cpu number\n");
        cpu = -1;
      }
      break;
    case 'p':
      pri = atoi(optarg);
      if( pri > sched_get_priority_max(SCHED_RR) ||
          pri < sched_get_priority_min(SCHED_RR) )
      {
        printf("ERR:out of priority number (%d-%d)\n",
               sched_get_priority_min(SCHED_RR),
               sched_get_priority_max(SCHED_RR));
        pri = -1;
      }
      break;
    case 't':
//      slp = atoi(optarg);
      sscanf(optarg, "%d", &slp);
      break;
    case 'i':
//      ite = atoi(optarg);
      sscanf(optarg, "%d", &ite);
      break;
    case 'v':
      printf("version 0.1.0\n");
      return 0;
      break;
    case 'h':
    default:
      printf("Realtime latency test.  V0.1 (C)CESI,2020-04-03, ZZX@Beijing\n");
      printf("USAGE: cyclictest [-t sleepPeriodTimeInSeconds]\n");
      printf("                  [-i iterations]\n");
      printf("                  [-c cpuUnderTest]\n");
      printf("                  [-p priority]\n");
      printf("                  [-r]\n");
      printf("                  [-h]\n");
      printf("        -t : sleep for n seconds, default is 1s.\n");
      printf("             每次迭代睡眠时间，默认为1秒。\n");
      printf("        -i : iterations, default is 3.\n");
      printf("             迭代次数，默认为3次。\n");
      printf("        -c : running under which cpu, from 0 to max-1, default is unspecified.\n");
      printf("             指定在哪个CPU上运行，从0开始数，默认不指定。\n");
      printf("        -p : priority, from 0 to 99, default is unspecified.\n");
      printf("             指定进程优先级，0-99，默认指不定。\n");
      printf("        -r : uses realtime clock TSC Register.\n");
      printf("             使用TSC寄存器获得纳秒级时钟，否则使用微秒级时钟。\n");
      printf("        -h : this message.\n");
      printf("             打印简要说明。\n");
      printf("\n");
      printf("SAMPLES:\ncyclictest -t 1 -i 3 -c 0 -p 1 -r\n\n");
      return 0;
    }
  }

  printf("[ARGUMENTS]\n");

  // CPU setting
  if (cpu != -1)
  {
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
      printf("ERR:sched_setaffinity error\n");
  }
  sched_getaffinity(0, sizeof(get), &get);
  for (i=0; i<sysconf(_SC_NPROCESSORS_CONF); i++)
  {
    if (CPU_ISSET(i, &get))
      printf("running on : CPU%d, max %ldCPU(s)\n", i, sysconf(_SC_NPROCESSORS_CONF));
  }

  // priority setting
  if (pthread_attr_init( &pthattr ) != 0)
    printf("ERR:pthread_attr_init error\n");
  if (pri != -1)
  {
    if (pthread_attr_setschedpolicy(&pthattr, SCHED_RR) != 0)
      printf("ERR:pthread_attr_setschedpolicy error\n");
    if (pthread_attr_getschedparam(&pthattr, &param) != 0)
      printf("ERR:pthread_attr_getschedparam error\n");
    param.__sched_priority = pri;
    if (pthread_attr_setschedparam(&pthattr, &param) != 0)
      printf("ERR:pthread_attr_setschedparam error\n");
  }
  if (pthread_attr_getschedparam(&pthattr, &param) != 0)
    printf("ERR:pthread_attr_getschedparam error\n");
  if (pthread_attr_getschedpolicy(&pthattr, &policy) != 0)
    printf("ERR:pthread_attr_getschedpolicy error\n");
  printf("  priority : %d (%s)\n", param.__sched_priority, POLICY_TO_STRING(policy));

  // clock setting
  if (use_TSC==1)
    printf("  uses TSC : YES\n");
  else
    printf("  uses TSC : NO\n");

  // iteration setting
  printf("     sleep : %d (s)\n", slp);
  printf("iterations : %d\n\n", ite);


  printf("[RUN]\n");
  for (i=0; i<ite; i++)
  {
    if( use_TSC==1 )
    {
      clock_gettime(CLOCK_REALTIME, &ts[0]);
      sleep(slp);
      clock_gettime(CLOCK_REALTIME, &ts[1]);
      printf("  %ld.%09ld (s)\n", ts[0].tv_sec, ts[0].tv_nsec);
      printf("  %ld.%09ld (s)\n", ts[1].tv_sec, ts[1].tv_nsec);
      sec = ts[1].tv_sec - ts[0].tv_sec - slp;
      nsec = ts[1].tv_nsec - ts[0].tv_nsec;
      if (nsec<0)
      {
      nsec += 1000000000;
        sec -= 1;
      }
      printf(" latency = %ld.%09ld (s)\n\n", sec, nsec);
      lat = sec*1000000000 + nsec;
    }else{
      gettimeofday(&tv[0], &tz);
      sleep(slp);
      gettimeofday(&tv[1], &tz);
      printf("  %ld.%06ld (s)\n", tv[0].tv_sec, tv[0].tv_usec);
      printf("  %ld.%06ld (s)\n", tv[1].tv_sec, tv[1].tv_usec);
      sec = tv[1].tv_sec - tv[0].tv_sec - slp;
      usec = tv[1].tv_usec - tv[0].tv_usec;
      if (usec<0)
      {
      usec += 1000000;
        sec -= 1;
      }
      printf(" latency = %ld.%06ld (s)\n\n", sec, usec);
      lat = sec*1000000 + usec;
    }

    if(lat > max)
      max = lat;
    if(lat < min)
      min = lat;
    avg += lat;
  }
  avg /= ite;

  pthread_attr_destroy( &pthattr );

  printf("[RESULT]\n");
  if( use_TSC==1 )
  {
    printf("     min = %ld.%09ld (s)\n", min/1000000000, min%1000000000);
    printf("     max = %ld.%09ld (s)\n", max/1000000000, max%1000000000);
    printf("     avg = %ld.%09ld (s)\n", avg/1000000000, avg%1000000000);
  }else{
    printf("     min = %ld.%06ld (s)\n", min/1000000, min%1000000);
    printf("     max = %ld.%06ld (s)\n", max/1000000, max%1000000);
    printf("     avg = %ld.%06ld (s)\n", avg/1000000, avg%1000000);
  }
  return avg;
}
