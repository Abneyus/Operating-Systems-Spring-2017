#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

int main(int argc, char** argv)
{
  struct hostent *host = gethostbyname("localhost");
  struct sockaddr_in sock;
  sock.sin_family = AF_INET;
  sock.sin_port = htons(8192);
  sock.sin_addr.s_addr = ((struct in_addr *)(host->h_addr))->s_addr;

  int s = socket(AF_INET, SOCK_STREAM, 0);

  connect(s, (struct sockaddr *)&sock, sizeof(sock));

  int fileSize = 0;
  int offset = 0;
  int a = 0;

  char* buffer;

  if(strcmp(argv[1], "mouse.txt") == 0)
  {
    fileSize = 917;
    offset = 19;
    buffer = calloc(sizeof(char), fileSize + offset);
    strcat(buffer, "SAVE mouse.txt 917\n");
  }
  else if(strcmp(argv[1], "legend.txt") == 0)
  {
    fileSize = 70672;
    offset = 22;
    buffer = calloc(sizeof(char), fileSize + offset);
    strcat(buffer, "SAVE legend.txt 70672\n");
  }
  else if(strcmp(argv[1], "chicken.txt") == 0)
  {
    fileSize = 31;
    offset = 20;
    buffer = calloc(sizeof(char), fileSize + offset);
    strcat(buffer, "SAVE chicken.txt 31\n");
  }
  else
  {
      printf("NO FILE FOUND\n");
      return EXIT_FAILURE;
  }

  FILE* reading = fopen(argv[1], "r");

  for( a = 0; a < fileSize; a++)
  {
    fscanf(reading, "%c", &buffer[a + offset]);
  }

  send(s, buffer, fileSize + offset, 0);

  sleep(10);

  fclose(reading);

  free(buffer);
  // sleep(3);

  return EXIT_SUCCESS;
}
