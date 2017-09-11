#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>

pthread_mutex_t alnumLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t spaceLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t otherLock = PTHREAD_MUTEX_INITIALIZER;

int alnum_total = 0;
int space_total = 0;
int other_total = 0;

//Iterates over a file and counts the number of alnum characters.
//Returns this count.
void * countAlnum(void* fileName)
{
  int* alnum_count = calloc(sizeof(int), 1);

  FILE* readin = fopen(fileName, "r");
  char temp[1];
  if(readin != NULL)
  {
    while( fscanf(readin, "%c", temp) != EOF)
    {
      if(isalnum(temp[0]))
      {
        *alnum_count+=1;
      }
    }
  } else {
    perror("NULL FILE");
  }

  fclose(readin);

  printf("THREAD %u: Added alphanumeric count of %d to totals (then exiting)\n", (unsigned int)pthread_self(), *alnum_count);
  fflush(stdout);

  return alnum_count;
}

//Iterates over a file and counts the number of whitespace characters.
//Returns this count.
void * countSpace(void* fileName)
{
  int* space_count = calloc(sizeof(int), 1);

  FILE* readin = fopen(fileName, "r");
  char temp[1];
  if(readin != NULL)
  {
    while( fscanf(readin, "%c", temp) != EOF)
    {
      if(isspace(temp[0]))
      {
        *space_count+=1;
      }
    }
  } else {
    perror("NULL FILE");
  }

  fclose(readin);

  printf("THREAD %u: Added whitespace count of %d to totals (then exiting)\n", (unsigned int)pthread_self(), *space_count);
  fflush(stdout);

  return space_count;
}

//Iterates over a file and counts the number of other characters.
//Returns this count.
void * countOther(void* fileName)
{
  int* other_count = calloc(sizeof(int), 1);

  FILE* readin = fopen(fileName, "r");
  char temp[1];
  if(readin != NULL)
  {
    while( fscanf(readin, "%c", temp) != EOF)
    {
      if(!isspace(temp[0]) && !isalnum(temp[0]))
      {
        *other_count+=1;
      }
    }
  } else {
    perror("NULL FILE");
  }

  fclose(readin);

  printf("THREAD %u: Added other count of %d to totals (then exiting)\n", (unsigned int)pthread_self(), *other_count);
  fflush(stdout);

  return other_count;
}

//Creates a countAlnum, countSpace, and countOther thread for the filename argument.
//Returns NULL.
void * midMethod(void* fileName)
{
  printf("THREAD %u: Processing %s (created three child threads)\n", (unsigned int)pthread_self(), (char*)fileName);
  fflush(stdout);

  void* alnum_mid = NULL;
  void* space_mid = NULL;
  void* other_mid = NULL;

  //Keep track of the TIDs.
  pthread_t* grandthreads = calloc(sizeof(pthread_t), 3);

  pthread_create(&grandthreads[0], NULL, &countAlnum, fileName);
  pthread_create(&grandthreads[1], NULL, &countSpace, fileName);
  pthread_create(&grandthreads[2], NULL, &countOther, fileName);

  pthread_join(grandthreads[0], &alnum_mid);
  pthread_join(grandthreads[1], &space_mid);
  pthread_join(grandthreads[2], &other_mid);

  free(grandthreads);

  printf("THREAD %u: File %s contains %d alnum, %d space, and %d other characters\n", (unsigned int)pthread_self(), (char*)fileName, *(int*)alnum_mid, *(int*)space_mid, *(int*)other_mid);
  fflush(stdout);

  //Will lock resources as appropraite for each addition operation.
  //Does not need to lock alnum when editing space, etc.
  pthread_mutex_lock(&alnumLock);
  alnum_total+=*(int*)alnum_mid;
  pthread_mutex_unlock(&alnumLock);

  pthread_mutex_lock(&spaceLock);
  space_total+=*(int*)space_mid;
  pthread_mutex_unlock(&spaceLock);

  pthread_mutex_lock(&otherLock);
  other_total+=*(int*)other_mid;
  pthread_mutex_unlock(&otherLock);

  //Frees memory allocated in the thread methods, countAlnum, countSpace, etc.
  free(alnum_mid);
  free(space_mid);
  free(other_mid);

  return NULL;
}

int main(int argc, char* argv[])
{
  int a = 1;

  if(argc == 1)
  {
    fprintf(stderr, "ERROR: Not enough arguments given. Expected: PROGRAM input1 ...\n");
    return EXIT_FAILURE;
  }

  int oneValid = 0;

  //Checks if there is at least one valid file in the arguments.
  //Will print to STDERR for any non-valid files.
  for(a = 1; a < argc; a++)
  {
    if( access( argv[a], F_OK ) == -1 )
    {
      fprintf(stderr, "ERROR: File %s does not exist!\n", argv[a]);
      fflush(stderr);
    }
    else
    {
      oneValid = 1;
    }
  }

  //If there are no valid files, exit.
  if(!oneValid)
  {
    return EXIT_FAILURE;
  }

  printf("THREAD %u: Program started (top-level thread)\n", (unsigned int)pthread_self());
  fflush(stdout);

  pthread_t* midthreads = calloc(sizeof(pthread_t), argc-1);

  for(a = 1; a < argc; a++)
  {
    //Doesn't run on non-valid files.
    if( access( argv[a], F_OK ) == -1 )
    {

    }
    //Creates a thread for the valid file we've chosen, giving the filename as an argument
    //and creating the thread with startpoint midMethod().
    //Prints error messages if thread creation fails.
    else
    {
      if(pthread_create(&midthreads[a-1], NULL, &midMethod, argv[a]))
      {
        fprintf(stderr, "ERROR: Creating thread\n");
        return EXIT_FAILURE;
      }
      else
      {
        printf("THREAD %u: Created child thread for %s\n", (unsigned int)pthread_self(), argv[a]);
        fflush(stdout);
      }
    }
  }

  //Blocks until all valid threads are finished executing.
  for(a = 0; a < argc-1; a++)
  {
    if(&midthreads[a] != NULL)
      pthread_join(midthreads[a], NULL);
  }

  free(midthreads);

  printf("THREAD %u: All files contain %d alnum, %d space, and %d other characters\n", (unsigned int)pthread_self(), alnum_total, space_total, other_total);
  printf("THREAD %u: Program ended (top-level thread)\n", (unsigned int)pthread_self());
  fflush(stdout);

  return EXIT_SUCCESS;
}
