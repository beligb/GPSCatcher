#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>


#define PORT_NUMBER 12345
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct packet{
    char date[20];
    char time[20];
    char latitude[20];
    char longitude[20];
} packet;

packet s_pack;
int readStatus = 1;//0:nothing, 1:wait for date, 2:wait for time, 3:wait for latitude, 4:wait for longtitude
int writeStatus = 0;

int readline(const char *in, int in_len, char *out, int* out_len);
void *start_server(void * arg);

int main()
{
    pthread_t s_id;
    int fd = open("/dev/ttyUSB10", O_RDWR);

    if (fd == -1)
    {
        fprintf(stderr, "cannot open USB\n");
        exit(1);
    }

    struct termios options;
    tcgetattr(fd, &options);

    cfsetispeed(&options, 9600);
    cfsetospeed(&options, 9600);

    tcsetattr(fd, TCSANOW, &options);

    pthread_create(&s_id, NULL, start_server, NULL);

    char buf[255];
    char line[255];
    int lineidx = 0;
    const char *work = "1";
    const char *stop = "0";
    int init = 0;
    memset(buf, 0, 255);
    memset(line, 0, 255);

    while(1)
    {
        memset(buf, 0, 255);
        //set gps status
        pthread_mutex_lock(&mutex);
        if (writeStatus == 1)
            write(fd, work, 1);
        else if (writeStatus == 0)
            write(fd, stop, 1);
        pthread_mutex_unlock(&mutex);

        int bytes_read = read(fd, buf, 1);

        if (bytes_read == 0)
            continue;

        if (buf[0] == '\n')
            continue;
        if (buf[0] == '\r')
        {
            line[lineidx] = 0;
            char p_buf[20];
            int buf_sz;
            memset(p_buf, 0, 20);
            int retval = readline(line, lineidx, p_buf, &buf_sz);

            switch (retval)
            {
                case 0:
            //        continue;
                    break;
                case 1://copy to date buf
                    pthread_mutex_lock(&mutex);
                    memset(s_pack.time, 0, 20);
                    memcpy(s_pack.time, p_buf, buf_sz + 1);
                    //printf("date is: %s", s_pack.date);
                    readStatus = 2;
                    pthread_mutex_unlock(&mutex);
                    break;
                case 2://copy to time buf
                    pthread_mutex_lock(&mutex);
                    memset(s_pack.date, 0, 20);
                    memcpy(s_pack.date, p_buf, buf_sz + 1);
                    //printf("time is: %s", s_pack.time);
                    readStatus = 3;
                    pthread_mutex_unlock(&mutex);
                    break;
                case 3://copy to latitude buf
                    pthread_mutex_lock(&mutex);
                    memset(s_pack.latitude, 0, 20);
                    memcpy(s_pack.latitude, p_buf, buf_sz + 1);
                    //printf("time is: %s", s_pack.latitude);
                    readStatus = 4;
                    pthread_mutex_unlock(&mutex);
                    break;
                case 4://copy to longitude buf
                    pthread_mutex_lock(&mutex);
                    memset(s_pack.longitude, 0, 20);
                    memcpy(s_pack.longitude, p_buf, buf_sz + 1);
                    //printf("data is: %s", (char*)&s_pack);
                    readStatus = 5;
                    pthread_mutex_unlock(&mutex);
                    break;
                default:
                    break;
            }



            lineidx = 0;
            memset(line, 0, 255);
        }
        line[lineidx++] = buf[0];

    }

    close(fd);
    return 0;
}

int readline(const char *in,int in_len, char *out, int* out_len)
{
    char *pch;

    pch = strstr(in, "Time");
    if (pch != NULL)
    {
        *out_len = strlen(pch+6);
        strcpy(out, pch+6);
        out[*out_len] = '\n';
        out[*out_len + 1] = 0;
        //printf("Current time is: %s\n", out);
        
        return 1;
    }
    
    pch = strstr(in, "Date");
    if (pch != NULL)
    {
        *out_len = strlen(pch+6);
        strcpy(out, pch+6);
        out[*out_len] = '\n';
        out[*out_len + 1] = 0;
        //printf("Current date is: %s\n", pch+6);
        return 2;
    }

    pch = strstr(in, "Lat");
    if (pch != NULL)
    {
        *out_len = strlen(pch+5);
        strcpy(out, pch+5);
        out[*out_len] = '\n';
        out[*out_len + 1] = 0;
        //printf("Current latitude is: %s\n", pch+5);
        return 3;
    }

    pch = strstr(in, "Long");
    if (pch != NULL)
    {
        *out_len = strlen(pch+6);
        strcpy(out, pch+6);
        out[*out_len] = '\n';
        out[*out_len + 1] = 0;
        //printf("Current longitude is: %s\n", pch+6);
        return 4;
    }
   
    return 0;
}

void *start_server(void * arg)
{

      // structs to represent the server and client
      struct sockaddr_in server_addr,client_addr;    
      
      int sock; // socket descriptor

      // 1. socket: creates a socket descriptor that you later use to make other system calls
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	perror("Socket");
	exit(1);
      }
      int temp;
      if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
	perror("Setsockopt");
	exit(1);
      }

      // configure the server
      server_addr.sin_port = htons(PORT_NUMBER); // specify port number
      server_addr.sin_family = AF_INET;         
      server_addr.sin_addr.s_addr = INADDR_ANY; 
      bzero(&(server_addr.sin_zero),8); 
      
      // 2. bind: use the socket and associate it with the port number
      if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
	perror("Unable to bind");
	exit(1);
      }

      // 3. listen: indicates that we want to listn to the port to which we bound; second arg is number of allowed connections
      if (listen(sock, 5) == -1) {
	perror("Listen");
	exit(1);
      }
          
      printf("\nServer waiting for connection on port %d\n", PORT_NUMBER);
      fflush(stdout);
     
      while (1)
      {

          // 4. accept: wait until we get a connection on that port
          int sin_size = sizeof(struct sockaddr_in);
          int fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
          printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
          
          // echo back the message to the client
          char send_data[100] = {0};
          char recv_data[100] = {0};

          //lock
          int loop = 1;
          while (loop)
          {
              pthread_mutex_lock(&mutex);
              if (writeStatus == 0)
              {
                  sprintf(send_data, "stop\n");
                  loop = 0;
              }
              else if (readStatus == 5)
              {    
                printf("spack: date:%stime:%slat:%slong:%s", s_pack.date, s_pack.time, s_pack.latitude, s_pack.longitude);
                sprintf(send_data, "%s%s%s%s", s_pack.date, s_pack.time, s_pack.latitude, s_pack.longitude);
                //timer = 0;
                memset(&s_pack, 0, 80);
                readStatus = 0;
                loop = 0;
              }
              pthread_mutex_unlock(&mutex);
          }
          //unlock

          // 6. send: send a message over the socket
          send(fd, send_data, strlen(send_data), 0);

          printf("Server sent message: %s\n", send_data);

          recv(fd, recv_data, 100, 0);

          if (strncmp(recv_data, "1", 1) == 0)
          {
              pthread_mutex_lock(&mutex);
              writeStatus = 1;
              pthread_mutex_unlock(&mutex);
          }
          else if (strncmp(recv_data, "0", 1) == 0)
          {
              pthread_mutex_lock(&mutex);
              writeStatus = 0;
              pthread_mutex_unlock(&mutex);
          }

          // 7. close: close the socket connection
          close(fd);
          
          printf("Server closed connection\n");

      }
  
  return 0;
} 


