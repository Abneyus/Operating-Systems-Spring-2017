#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/select.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <dirent.h>

#define BUFFER_SIZE 10000

// Break out of master server loop if set to 1.
int stopNow = 0;
int filecount = 0;
int mem_files = 4;
char** files;
pthread_mutex_t* file_locks;

//Helper function to determine the index of a file in the files array.
//ARGUMENTS: fileName to check the index of
//RETURNS: index of the fileName argument, or -1 if fileName not in the array
int getFileIndex(char* fileName)
{
  int a = 0;
  for(a = 0; a < filecount; a++)
  {
    // pthread_mutex_lock(&file_locks[a]);
    if(strcmp(fileName, files[a]) == 0)
    {
      // pthread_mutex_unlock(&file_locks[a]);
      return a;
    }
    // pthread_mutex_unlock(&file_locks[a]);
  }
  return -1;
}

static int compareString(const void* a, const void* b)
{
  return strcmp(*(char * const *)a, *(char * const *)b);
}

void* thandler(void* input)
{
  pthread_detach(pthread_self());
  int* fd = (int*) (input);
  int a = 0;

  while(1)
  {
    char buffer[BUFFER_SIZE];

    int readBytes = recv(*fd, buffer, sizeof(buffer), 0);

    if(readBytes < 1)
    {
      close(*fd);
      #ifdef DEBUG_MODE
        printf("DEBUG: Closing %d\n", *fd);
        fflush(stdout);
      #endif
      printf("[child %u] Client disconnected\n", (unsigned int) pthread_self());
      fflush(stdout);
      return NULL;
    }

    if(strncmp(buffer, "LIST", 4) == 0)
    {
      #ifdef DEBUG_MODE
        printf("DEBUG: Got LIST TCP command\n");
        fflush(stdout);
      #endif

      printf("[child %u] Received LIST\n", (unsigned int) pthread_self());
      fflush(stdout);

      //Locks all the filenames so that they might be sorted.
      for(a = 0; a < filecount; a++)
      {
        pthread_mutex_lock(&file_locks[a]);
      }

      qsort(&files[0], filecount, sizeof(char*), &compareString);

      for(a = 0; a < filecount; a++)
      {
        pthread_mutex_unlock(&file_locks[a]);
      }

      char message[BUFFER_SIZE];
      if(filecount == 0)
      {
        strcpy(message,"0\n");
      }
      else
      {
        sprintf(message, "%d", filecount);
        for(a = 0; a < filecount; a++)
        {
          pthread_mutex_lock(&file_locks[a]);
          if(a+1 == filecount)
          {
            sprintf(message, "%s %s\n", message, files[a]);
          }
          else
          {
            sprintf(message, "%s %s", message, files[a]);
          }
          pthread_mutex_unlock(&file_locks[a]);
        }
      }
      send(*fd, message, strlen(message), 0);

      printf("[child %u] Sent %s", (unsigned int) pthread_self(), message);
      fflush(stdout);
    }
    else if (strncmp(buffer, "READ", 4) == 0)
    {
      #ifdef DEBUG_MODE
        printf("DEBUG: Got READ TCP command\n");
        fflush(stdout);
      #endif

      char filename[32];
      int offset = 0;
      int length = 0;

      sscanf(buffer, "%*s %s %d %d", filename, &offset, &length);

      #ifdef DEBUG_MODE
        printf("DEBUG: FILENAME: %s\tBYTE-OFFSET: %d\tLENGTH: %d\n", filename, offset, length);
        fflush(stdout);
      #endif

      printf("[child %u] Received READ %s %d %d\n", (unsigned int) pthread_self(), filename, offset, length);
      fflush(stdout);

      if(length == 0 || strlen(filename) == 0)
      {
        char* error_message = "ERROR INVALID REQUEST\n";
        send(*fd, error_message, strlen(error_message), 0);
        printf("[child %u] Sent ERROR INVALID REQUEST\n", (unsigned int) pthread_self());
        fflush(stdout);
        continue;
      }

      int fileIndex = getFileIndex(filename);

      if(fileIndex == -1)
      {
        char* error_message = "ERROR NO SUCH FILE\n";
        send(*fd, error_message, strlen(error_message), 0);
        printf("[child %u] Sent ERROR NO SUCH FILE\n", (unsigned int) pthread_self());
        fflush(stdout);
        continue;
      }

      char* toReturn = calloc(sizeof(char), length);

      char* fullPath = calloc(sizeof(char), 8 + strlen(filename));
      strcpy(fullPath, "storage/");
      strcpy(fullPath+8, filename);

      pthread_mutex_lock(&file_locks[fileIndex]);
      FILE* toRead = fopen(fullPath, "r");

      //Simply scans past the first offset number of bytes.
      for(a = 0; a < offset; a++)
      {
        fscanf(toRead, "%*c");
      }

      bool invalid = 0;

      char temp[2];
      temp[1] = 0;

      for(a = 0; a < length; a++)
      {
        if(fscanf(toRead, "%c", temp) != EOF)
        {
          strcat(toReturn, temp);
        }
        else
        {
          invalid = 1;
          char* message = "ERROR INVALID BYTE RANGE\n";
          send(*fd, message, strlen(message), 0);
          printf("[child %u] Sent ERROR INVALID BYTE RANGE\n", (unsigned int) pthread_self());
          fflush(stdout);
          break;
        }
      }
      fclose(toRead);
      pthread_mutex_unlock(&file_locks[fileIndex]);

      char* ack_message = calloc(sizeof(char), 16);

      sprintf(ack_message, "%s %d\n", "ACK", length);

      send(*fd, ack_message, strlen(ack_message), 0);
      printf("[child %u] Sent ACK %d\n", (unsigned int) pthread_self(), length);
      fflush(stdout);

      free(ack_message);

      if(!invalid)
      {
        send(*fd, toReturn, strlen(toReturn), 0);
        printf("[child %u] Sent %d bytes of \"%s\" from offset %d\n", (unsigned int) pthread_self(), length, filename, offset);
        fflush(stdout);
      }
      free(fullPath);
      free(toReturn);
    }
    else if (strncmp(buffer, "SAVE", 4) == 0)
    {
      #ifdef DEBUG_MODE
        printf("DEBUG: Got SAVE TCP command\n");
        fflush(stdout);
      #endif

      char filename[32];
      int bytes = 0;

      sscanf(buffer, "%*s %s %d", filename, &bytes);

      #ifdef DEBUG_MODE
        printf("DEBUG: FILENAME: %s\tBYTES: %d\n", filename, bytes);
        fflush(stdout);
      #endif

      printf("[child %u] Received SAVE %s %d\n", (unsigned int) pthread_self(), filename, bytes);
      fflush(stdout);

      if(strlen(filename) == 0 || bytes == 0)
      {
        char* error_message = "ERROR INVALID REQUEST\n";
        send(*fd, error_message, strlen(error_message), 0);
        continue;
      }

      bool fileExists = 0;

      for(a = 0; a < filecount; a++)
      {
        if(strcmp(files[a], filename) == 0)
        {
          printf("[child %u] Sent ERROR FILE EXISTS\n", (unsigned int) pthread_self());
          fflush(stdout);
          char* error_message = "ERROR FILE EXISTS\n";
          send(*fd, error_message, strlen(error_message), 0);
          fileExists = 1;
          break;
        }
      }

      if(fileExists)
      {
        continue;
      }

      //Sets a equal to the index of the start of the data.
      for(a = 0; a < 64; a++)
      {
        if(buffer[a] == '\n')
        {
          break;
        }
      }
      //Index of the beginning of data in the packet.
      int start = a + 1;

      #ifdef DEBUG_MODE
        printf("DEBUG: start %d\n", start);
        fflush(stdout);
      #endif

      pthread_mutex_lock(&file_locks[filecount]);
      //Add filename as tracked.
      files[filecount] = calloc(sizeof(char), strlen(filename));
      strcpy(files[filecount], filename);
      filecount+=1;
      pthread_mutex_unlock(&file_locks[filecount-1]);

      #ifdef DEBUG_MODE
        printf("DEBUG: files[filecount] = %s\n", files[filecount-1]);
        fflush(stdout);
      #endif

      //Creates the full path of the file from the filename.
      char* fullPath = calloc(sizeof(char), 8 + strlen(filename));
      strcpy(fullPath, "storage/");
      strcpy(fullPath+8, filename);

      #ifdef DEBUG_MODE
        printf("DEBUG: %s\n", fullPath);
        fflush(stdout);
      #endif

      //Creates an array of appropriate size for storing the data.
      char* fullData = calloc(sizeof(char), bytes);
      //Copies all the data we got in the initial packet to the full data array.
      memcpy(fullData, buffer+start, fmin(BUFFER_SIZE - start, bytes));

      //If we didn't get all the data in the initial packet of BUFFER_SIZE, we have to
      //read in more.
      int bytesLeft = bytes - (readBytes - start);
      #ifdef DEBUG_MODE
        printf("DEBUG: bytesLeft:\t%d\n", bytesLeft);
        fflush(stdout);
      #endif
      while(bytesLeft > 0)
      {
        // printf("DEBUG: bytesLeft:\t%d\n", bytesLeft);
        // fflush(stdout);
        char* extraData = calloc(sizeof(char), bytesLeft);
        int extraBytes = recv(*fd, extraData, bytesLeft, 0);
        memcpy(fullData + (bytes - bytesLeft), extraData, extraBytes);
        bytesLeft -= extraBytes;
        free(extraData);
      }

      // #ifdef DEBUG_MODE
      //   printf("%s\n", fullData);
      // #endif

      int fileIndex = getFileIndex(filename);

      //Write all the data from the array to file.
      pthread_mutex_lock(&file_locks[fileIndex]);
      FILE* toWrite = fopen(fullPath, "w");
      for(a = 0; a < bytes; a++)
      {
        fprintf(toWrite, "%c", fullData[a]);
      }
      fclose(toWrite);
      pthread_mutex_unlock(&file_locks[fileIndex]);

      free(fullData);
      free(fullPath);

      printf("[child %u] Stored file \"%s\" (%d bytes)\n", (unsigned int) pthread_self(), filename, bytes);
      fflush(stdout);

      char* ack_message = "ACK\n";
      send(*fd, ack_message, strlen(ack_message), 0);

      printf("[child %u] Sent ACK\n", (unsigned int) pthread_self());
      fflush(stdout);
    }
    else
    {
      char* message = "ERROR: Invalid TCP command.\n";
      send(*fd, message, strlen(message), 0);
      printf("[child %u] Sent ERROR INVALID TCP COMMAND\n", (unsigned int) pthread_self());
      fflush(stdout);
    }
  }

  return NULL;
}

int main(int argc, char** argv)
{

  printf("Started server\n");
  fflush(stdout);

  int a = 0;

  files = calloc(sizeof(char*), mem_files);
  file_locks = calloc(sizeof(pthread_mutex_t), mem_files);

  for(a = 0; a < mem_files; a++)
  {
    pthread_mutex_init(&file_locks[a], NULL);
  }

  int tport = 8192;
  int uport = 8193;

  if(argc == 3)
  {
    tport = atoi(argv[1]);
    uport = atoi(argv[2]);
  }
  else
  {
    free(files);
    free(file_locks);
    fprintf(stderr, "ERROR: Not enough arguments given.\n");
    fprintf(stderr, "ERROR: Expected [program] [tcp port] [udp port]\n");
    return EXIT_FAILURE;
  }

  DIR* checkDir = opendir("storage");
  if(checkDir)
  {
    closedir(checkDir);
  }
  else if (ENOENT == errno) {
    free(files);
    // free(threads);
    free(file_locks);
    fprintf(stderr, "ERROR: storage directory does not exist\n");
    return EXIT_FAILURE;
  }

  int tfd_server = socket(AF_INET, SOCK_STREAM, 0);
  if(tfd_server == -1)
  {
    fprintf(stderr, "ERROR: Error on TCP socket() call\n");
    return EXIT_FAILURE;
  }
  int ufd_server = socket(AF_INET, SOCK_DGRAM, 0);
  if(ufd_server == -1)
  {
    fprintf(stderr, "ERROR: Error on UDP socket() call\n");
    return EXIT_FAILURE;
  }

  #ifdef DEBUG_MODE
    printf("DEBUG: TCP:\t%d UDP:\t%d\n", tfd_server, ufd_server);
  #endif

  struct sockaddr_in tsock_server;
  socklen_t tsocklen_server = sizeof(tsock_server);
  tsock_server.sin_family = AF_INET;
  tsock_server.sin_addr.s_addr = INADDR_ANY;
  tsock_server.sin_port = htons(tport);

  struct sockaddr_in usock_server;
  socklen_t usocklen_server = sizeof(usock_server);
  usock_server.sin_family = AF_INET;
  usock_server.sin_addr.s_addr = INADDR_ANY;
  usock_server.sin_port = htons(uport);

  if(bind(tfd_server, (struct sockaddr*)&tsock_server, tsocklen_server) < 0)
  {
    fprintf(stderr, "ERROR: Error on TCP bind() call\n");
    return EXIT_FAILURE;
  }

  if(bind(ufd_server, (struct sockaddr*)&usock_server, usocklen_server) < 0)
  {
    fprintf(stderr, "ERROR: Error on UDP bind() call\n");
    return EXIT_FAILURE;
  }

  if(getsockname(tfd_server, (struct sockaddr*)&tsock_server, &tsocklen_server) < 0)
  {
    fprintf(stderr, "ERROR: Error on TCP getsockname() call\n");
    return EXIT_FAILURE;
  }

  if(getsockname(ufd_server, (struct sockaddr*)&usock_server, &usocklen_server) < 0)
  {
    fprintf(stderr, "ERROR: Error on UDP getsockname() call\n");
    return EXIT_FAILURE;
  }

  printf("Listening for TCP connections on port: %d\n", tport);
  printf("Listening for UDP datagrams on port: %d\n", uport);
  fflush(stdout);

  listen(tfd_server, 5);

  fd_set fds_server;

  while(!stopNow)
  {
    FD_ZERO(&fds_server);
    FD_SET(tfd_server, &fds_server);
    FD_SET(ufd_server, &fds_server);

    if(select(ufd_server + 1, &fds_server, NULL, NULL, NULL) < 0)
    {
      fprintf(stderr, "ERROR: Error on select() call\n");
      return EXIT_FAILURE;
    }

    if(FD_ISSET(tfd_server, &fds_server))
    {
      #ifdef DEBUG_MODE
        printf("DEBUG: TCP set\n");
        fflush(stdout);
      #endif

      if(filecount + 1 > mem_files)
      {
        for(a = 0; a < filecount; a++)
        {
          pthread_mutex_lock(&file_locks[a]);
        }

        char** nFiles = calloc(sizeof(char*), mem_files*2);
        pthread_mutex_t* nFile_locks = calloc(sizeof(pthread_mutex_t), mem_files*2);

        for(a = 0; a < mem_files; a++)
        {
          nFiles[a] = files[a];
          nFile_locks[a] = file_locks[a];
        }

        for(a = mem_files; a < mem_files*2; a++)
        {
          pthread_mutex_init(&nFile_locks[a], NULL);
        }

        free(files);
        free(file_locks);

        files = nFiles;
        file_locks = nFile_locks;

        for(a = 0; a < filecount; a++)
        {
          pthread_mutex_unlock(&file_locks[a]);
        }

        mem_files *= 2;
      }

      struct sockaddr_in tsock_client;
      socklen_t tsocklen_client = sizeof(tsock_client);

      int* new = calloc(sizeof(int), 1);
      new[0] = accept(tfd_server, (struct sockaddr*)&tsock_client, &tsocklen_client);

      #ifdef DEBUG_MODE
        printf("DEBUG: New TCP connection on %d\n", new[0]);
        fflush(stdout);
      #endif

      char clientName[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &tsock_client.sin_addr.s_addr, clientName, sizeof(clientName));
      printf("Rcvd incoming TCP connection from: %s\n", clientName);
      fflush(stdout);

      pthread_t thread;
      pthread_create(&thread, NULL, &thandler, &new[0]);
    }
    if(FD_ISSET(ufd_server, &fds_server))
    {
      #ifdef DEBUG_MODE
        printf("DEBUG: UDP set\n");
        fflush(stdout);
      #endif

      struct sockaddr_in usock_client;
      socklen_t usocklen_client = sizeof(usock_client);

      char buffer[256];
      int read = recvfrom(ufd_server, &buffer, sizeof(buffer), 0, (struct sockaddr*)&usock_client, &usocklen_client);
      buffer[read] = 0;

      char clientName[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &usock_client.sin_addr.s_addr, clientName, sizeof(clientName));
      printf("Rcvd incoming UDP datagram from: %s\n", clientName);
      fflush(stdout);

      if(strncmp(buffer, "LIST", 4) == 0)
      {
        #ifdef DEBUG_MODE
          printf("DEBUG: Got UDP list command\n");
          fflush(stdout);
        #endif

        printf("Received LIST\n");
        fflush(stdout);

        char* message = calloc(sizeof(char), BUFFER_SIZE);
        if(filecount == 0)
        {
          char* temp = "0\n";
          strcpy(message, temp);
        }
        else
        {

          for(a = 0; a < filecount; a++)
          {
            pthread_mutex_lock(&file_locks[a]);
          }

          qsort(&files[0], filecount, sizeof(char*), &compareString);

          for(a = 0; a < filecount; a++)
          {
            pthread_mutex_unlock(&file_locks[a]);
          }

          sprintf(message, "%d", filecount);
          for(a = 0; a < filecount; a++)
          {
            pthread_mutex_lock(&file_locks[a]);
            if(a+1 == filecount)
            {
              sprintf(message, "%s %s\n", message, files[a]);
            }
            else
            {
              sprintf(message, "%s %s", message, files[a]);
            }
            pthread_mutex_unlock(&file_locks[a]);
          }
        }
        sendto(ufd_server, message, strlen(message), 0, (struct sockaddr*)&usock_client, usocklen_client);

        printf("Sent %s", message);
        fflush(stdout);

        free(message);
      }
      else
      {
        #ifdef DEBUG_MODE
          printf("DEBUG: Got malformed UDP command\n");
          fflush(stdout);
        #endif
        char* message = "ERROR: Invalid UDP command.\n";
        sendto(ufd_server, message, strlen(message), 0, (struct sockaddr*)&usock_client, usocklen_client);
      }
    }
  }

  return EXIT_SUCCESS;

}
