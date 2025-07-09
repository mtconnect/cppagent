#include <stdio.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#define SHUT_RDWR SD_BOTH
#else
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define SOCKET int
#endif
#include <sys/types.h>
#include <string.h>

#define PORT        7878
#define HOST        "localhost"
#define BUFFER_SIZE 1024

void cleanup_and_exit(int ret)
{
#ifdef _WIN32
  WSACleanup();
#endif
  exit(ret);
}

int main(int argc, char **argv)
{
  char hostname[100];
  SOCKET        sd;
  struct sockaddr_in sin;
  struct sockaddr_in pin;
  struct hostent *hp;
  char buffer[BUFFER_SIZE];
  int port = PORT;
  FILE *file;
  char dump = 0;

#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2,0), &wsaData) != 0) {
    fprintf(stderr, "WSAStartup failed\n");
    cleanup_and_exit(1);
  }
#endif
  
  if (argc > 1 && strcmp(argv[1], "-h") == 0) {
    fprintf(stderr, "Usage: dump [host] [port] [file]\n    host defaults to localhost\n    port defaults to 7878\n    file defaults to stdout\n");
    exit(0);
  }
  
  strcpy(hostname,HOST);
  if (argc>1) { 
    strcpy(hostname,argv[1]); 
  }
    
  if (argc > 2) {
    port = atoi(argv[2]);
  }

  file = stdout;
  if (argc > 3) {
    file = fopen(argv[3], "w");
    if (file == NULL) {
      perror("fopen");
      fprintf(stderr, "Cannot open file %s\n", argv[3]);
      exit(1);
    }
    dump = 1;
  }

  /* go find out about the desired host machine */
  if ((hp = gethostbyname(hostname)) == 0) {
    perror("gethostbyname");
    cleanup_and_exit(1);
  }

  /* fill in the socket structure with host information */
  memset(&pin, 0, sizeof(pin));
  pin.sin_family = AF_INET;
  pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
  pin.sin_port = htons(port);

  /* grab an Internet domain socket */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    cleanup_and_exit(1);
  }

  /* connect to PORT on HOST */
  if (connect(sd,(struct sockaddr *)  &pin, sizeof(pin)) == -1) {
    perror("connect");
    cleanup_and_exit(1);
  }
        
  if (dump) {
    printf("Connected to %s port %d\n", hostname, port);
  }
        
  /* wait for a message to come back from the server */
  while (1) {
    int count = recv(sd, buffer, BUFFER_SIZE, 0);
    if (count == -1) {
      shutdown(sd, SHUT_RDWR);
      perror("recv");
      cleanup_and_exit(1);
    }
    if (count == 0)
      break;
    fwrite(buffer, 1, count, file);
    if (dump) {
      fputc('.', stdout);
      fflush(stdout);
    }
    fflush(file);
  }

  fclose(file);
  shutdown(sd, SHUT_RDWR);

#ifdef _WIN32
  WSACleanup();
#endif
  
  return 0;
}
