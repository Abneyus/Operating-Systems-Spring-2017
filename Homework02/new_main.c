#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char* argv[]) {
  int fileIterator = 0;
  int numFiles = argc-1;

  int alnum_total = 0;
  int space_total = 0;
  int other_total = 0;

  int a = 1;

  for(; a < argc; a++) {
    if( access( argv[a], F_OK ) == -1 ) {
      fprintf(stderr, "ERROR: File %s does not exist!\n", argv[a]);
      fflush(stderr);
      return EXIT_FAILURE;
    }
  }

  if(argc == 1) {
    fprintf(stderr, "ERROR: Not enough arguments given. Expected: PROGRAM input1 ...\n");
    return EXIT_FAILURE;
  }

  printf("PID %d: Program started (top-level process)\n", getpid());
  fflush(stdout);

  pid_t childpid;

  char* fileName;

  int p0_alnum[2];
  int p0_space[2];
  int p0_other[2];

  pipe(p0_alnum);
  pipe(p0_space);
  pipe(p0_other);

  for(fileIterator = 1; fileIterator < argc; fileIterator++) {
    fileName = argv[fileIterator];
    childpid = fork();
    if(childpid == -1) {
      perror("fork() failed\n");
      return EXIT_FAILURE;
    }
    if(childpid == 0) {
      break;
    } else {
      printf("PID %d: Created child process for %s\n", getpid(), fileName);
      fflush(stdout);
    }
  }

  if(childpid == -1) {
    perror("fork() failed");
    return EXIT_FAILURE;
  }

  if(childpid == 0) {

    printf("PID %d: Processing %s (created three child processes)\n", getpid(), fileName);
    fflush(stdout);


    int p1_alnum[2];
    int p1_space[2];
    int p1_other[2];

    pipe(p1_alnum);
    pipe(p1_space);
    pipe(p1_other);

    int forkIterator = 0;

    pid_t gpid;

    int chartype = 0;

    for(; forkIterator < 3; forkIterator++) {
      chartype = forkIterator;
      gpid = fork();
      if(gpid == -1) {
        perror("fork() failed\n");
        return EXIT_FAILURE;
      }
      if(gpid == 0) {
        break;
      }
    }

    if(gpid == 0) {
      int alnum_count = 0;
      int space_count = 0;
      int other_count = 0;

      FILE* readin = fopen(fileName, "r");
      char temp[1];
      if(readin != NULL) {
        while( fscanf(readin, "%c", temp) != EOF) {
          if(chartype == 0 && isalnum(temp[0])) {
            alnum_count+=1;
          }
          if(chartype == 1 && isspace(temp[0])) {
            space_count+=1;
          }
          if(chartype == 2 && !isalnum(temp[0]) && !isspace(temp[0])) {
            other_count+=1;
          }
        }
      } else {
        perror("NULL FILE");
      }
      fclose(readin);

      if(chartype == 0) {
        printf("PID %d: Sent alphanumeric count of %d to parent (then exiting)\n", getpid(), alnum_count);
        fflush(stdout);
        write(p1_alnum[1], &alnum_count, 4);
      }
      if(chartype == 1) {
        printf("PID %d: Sent whitespace count of %d to parent (then exiting)\n", getpid(), space_count);
        fflush(stdout);
        write(p1_space[1], &space_count, 4);
      }
      if(chartype == 2) {
        printf("PID %d: Sent other count of %d to parent (then exiting)\n", getpid(), other_count);
        fflush(stdout);
        write(p1_other[1], &other_count, 4);
      }
      return EXIT_SUCCESS;
    } else {
      int status;
      wait(&status);
      wait(&status);
      wait(&status);

      int alnum[1];
      int space[1];
      int other[1];

      read(p1_alnum[0], alnum, 4);
      read(p1_space[0], space, 4);
      read(p1_other[0], other, 4);

      printf("PID %d: File %s contains %d alnum, %d space, and %d other characters\n", getpid(), fileName, alnum[0], space[0], other[0]);
      fflush(stdout);

      write(p0_alnum[1], alnum, 4);
      write(p0_space[1], space, 4);
      write(p0_other[1], other, 4);

      printf("PID %d: Sent %s counts to parent (then exiting)\n", getpid(), fileName);
      fflush(stdout);
      return EXIT_SUCCESS;
    }

  } else {
    int a = 0;
    int status;
    for(a = 0; a < numFiles; a++) {
      wait(&status);
    }
    int buffer[10];
    read(p0_alnum[0], buffer, 4 * numFiles);
    // for(a = 0; a < 10; a++) {
    //   printf("%d:", buffer[a]);
    // }
    // printf("\n");
    // fflush(stdout);

    for(a = 0; a < numFiles; a++) {
      alnum_total+=buffer[a];
    }
    memset(buffer, 0, 10);
    read(p0_space[0], buffer, 4 * numFiles);
    for(a = 0; a < numFiles; a++) {
      space_total+=buffer[a];
    }
    memset(buffer, 0, 10);
    read(p0_other[0], buffer, 4 * numFiles);
    for(a = 0; a < numFiles; a++) {
      other_total+=buffer[a];
    }
    printf("PID %d: All files contain %d alnum, %d space, and %d other characters\n", getpid(), alnum_total, space_total, other_total);
    fflush(stdout);

    printf("PID %d: Program ended (top-level process)\n", getpid());
    fflush(stdout);
  }

  return EXIT_SUCCESS;
}
