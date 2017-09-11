#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char* argv[]) {
  int a = 0;
  int argLoop = 0;

  int alnum_total = 0;
  int space_total = 0;
  int other_total = 0;

  //throw error if not given enough arguments
  if(argc == 1) {
    fprintf(stderr, "ERROR: Insufficient number of files given as arguments. Expected: PROGRAM input1 input2...\n");
    return EXIT_FAILURE;
  }

  printf("PID %d: Program started (top-level process)\n", getpid());
  fflush(stdout);

  int numFiles = argc-1;

  pid_t pid[numFiles];

  //Fork for each filename.
  for(argLoop = 1; argLoop < argc; argLoop++) {
    int p0_alnum[2];
    int p0_space[2];
    int p0_other[2];

    pipe(p0_alnum);
    pipe(p0_space);
    pipe(p0_other);

    printf("PID %d: Created child process for %s\n", getpid(), argv[argLoop]);
    fflush(stdout);

    // pid_t pid;
    pid[argLoop] = fork();

    if( pid[argLoop] == -1) {
      //error
      perror("fork() failed");
      return EXIT_FAILURE;
    }

    if(pid[argLoop] == 0) {

      int p1_alnum[2];
      int p1_space[2];
      int p1_other[2];

      pipe(p1_alnum);
      pipe(p1_space);
      pipe(p1_other);

      char* input = argv[argLoop];

      printf("PID %d: Processing %s (created three child processes)\n", getpid(), input);
      fflush(stdout);

      pid_t gpid[3];
      for(a = 0; a < 3; a++) {
        gpid[a] = fork();

        if( gpid[a] == -1 ) {
          //error
          perror("fork() failed");
          return EXIT_FAILURE;
        }

        if (gpid[a] == 0) {
          int chartype = a;

          int alnum_count = 0;
          int space_count = 0;
          int other_count = 0;

          FILE* readin = fopen(input, "r");
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
          if(a == 2) {
            int alnum[1];
            int space[1];
            int other[1];

            read(p1_alnum[0], alnum, 4);
            read(p1_space[0], space, 4);
            read(p1_other[0], other, 4);

            printf("PID %d: File %s contains %d alnum, %d space, and %d other characters\n", getpid(), input, alnum[0], space[0], other[0]);
            fflush(stdout);

            write(p0_alnum[1], alnum, 4);
            write(p0_space[1], space, 4);
            write(p0_other[1], other, 4);

            printf("PID %d: Sent %s counts to parent (then exiting)\n", getpid(), input);
            fflush(stdout);
            return EXIT_SUCCESS;
          }
        }
      }
    } else {
      if(argLoop == numFiles) {
        int buffer[10];

        read(p0_alnum[0], buffer, 4 * numFiles);
        for(a = 0; a < 10; a++) {
          printf("%d:", buffer[a]);
        }
        printf("\n");
        fflush(stdout);

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
        printf("PID %d: All files contained %d alnum, %d space, and %d other characters.\n", getpid(), alnum_total, space_total, other_total);
        fflush(stdout);

        printf("PID %d: Program ended (top-level process)\n", getpid());
        fflush(stdout);
      }
    }
  }

  return EXIT_SUCCESS;
}
