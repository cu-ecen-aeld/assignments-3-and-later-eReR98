#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>

#define MAX_CONNECTIONS 1
#define PORT 9000
#define BUFFER_SIZE 40
#define FILE_PATH "/var/tmp/aesdsocketdata"

bool caught_sigint = false;
bool caught_sigterm = false;
bool success = true;

int inet_ntop( int,  const void *restrict, char[16] , socklen_t);

// borrowed from lecture
static void signal_handler ( int signal_number )
{
    /**
    * Save a copy of errno so we can restore it later.  See https://pubs.opengroup.org/onlinepubs/9699919799/
    * "Operations which obtain the value of errno and operations which assign a value to errno shall be
    *  async-signal-safe, provided that the signal-catching function saves the value of errno upon entry and
    *  restores it before it returns."
    */
    int errno_saved = errno;
    if ( signal_number == SIGINT ) {
        caught_sigint = true;
        printf("caught SIGINT\n");
    } else if ( signal_number == SIGTERM ) {
        caught_sigterm = true;
        printf("caught SIGTERM\n");
    }
    errno = errno_saved;
}

int findPacketSize(char* packet)
{
    int packetSize = 0;
    char currChar = packet[0];
    while(1)
    {
        currChar = packet[packetSize+1];

        if (currChar == '\000')
            break;
        
        packetSize++;
    }
    //perror("done counting");
    return packetSize;
}

int main(int argc, char*argv[])
{
    openlog(NULL, 0, LOG_USER);

    bool run_as_daemon = false;
    printf("\n%s\n", argv[1]);
    printf("\n%d\n", argc);
    
    if (argc > 1)
    {   
        printf("\nDAEMONTdfdfEST\n");
        if(strstr(argv[1], "-d"))
        {
            run_as_daemon=true;
            printf("\nDAEMONTEST\n");
        }
    }
    
    printf("\nendif\n");
    
    struct sigaction new_action;

    new_action.sa_handler=signal_handler;

    if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
        printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
        return -1;
    }
    if( sigaction(SIGINT, &new_action, NULL) ) {
        printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
        return -1;
    }

    int ret;
    int sockfd;
    int optval;
    int connecFd;
    ssize_t recvRet;
    struct sockaddr_in addr;
    char buff[BUFFER_SIZE] = {0};
    

    socklen_t addrlen = sizeof(addr);
    remove(FILE_PATH);
    sockfd = socket(PF_INET, SOCK_STREAM, 0);

    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if(ret != 0)
    {
        fprintf(stderr, "Error in setting socket options\n");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    ret = bind(sockfd, (struct sockaddr*) &addr, sizeof(addr));

    //perror("bound");
    if(ret != 0)
    {
        fprintf(stderr, "Error in binding to port\n");
        return -1;
    }

    pid_t pid = 0;

    if(run_as_daemon)
    {
        pid = fork();
    }
    
    // Indicates this is the parent thread and should not do anything
    if(pid != 0)
    {
        printf("daemon created\n");
        return 0;
    }

    ret = listen(sockfd, MAX_CONNECTIONS);
    //perror("listened");
    if(ret != 0)
    {
        fprintf(stderr, "Error in calling listen()\n");
        return -1;
    }


    FILE *wrPointer = fopen(FILE_PATH, "a+");
    if(wrPointer==NULL)
    {
        syslog(LOG_DEBUG, "Unable to open file");
        return -1;
    }

    while(success==true)
    {
        //perror("start accept");
        connecFd = accept(sockfd, (struct sockaddr*) &addr, &addrlen);
        //perror("end accept");
        if(connecFd < 0)
        {
            fprintf(stderr, "Error in accepting connection\n");
            success=false;
            break;
        }

        //https://stackoverflow.com/questions/3060950/how-to-get-ip-address-from-sock-structure-in-c

        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&addr;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop( AF_INET, &ipAddr, addrStr, INET_ADDRSTRLEN );

        syslog(LOG_DEBUG, "Accepted connection from %s", addrStr);

        

        bool newlineFound = false;

        while(newlineFound == false)
        {
            recvRet = recv(connecFd, buff, BUFFER_SIZE-1, 0);

            if(recvRet < 0)
            {
                fprintf(stderr, "Error in read()\n");
                success=false;
                break;
            }

            char packetsReceived[BUFFER_SIZE];
            strcpy(packetsReceived, buff); // this just removes extra null characters
            fprintf(wrPointer, packetsReceived, 0);

            if(strstr(packetsReceived, "\n"))
            {
                newlineFound = true;
            }
            
            memset(&packetsReceived[0], '\0', sizeof(packetsReceived));
            memset(&buff[0], '\0', sizeof(buff));
        }

        perror("newlinefound");
        //fprintf(wrPointer, "\n");

        // char sendBuff[BUFFER_SIZE] = {0};
        // fread(sendBuff, sizeof(sendBuff), 1, wrPointer);
        //https://stackoverflow.com/questions/71976433/using-fread-to-read-a-text-based-file-best-practices
        fseek(wrPointer, 0, SEEK_END);

        long filesize = ftell(wrPointer);
        rewind(wrPointer);
        char fileText[filesize];
        //fileText[filesize] = '\n';

        fread(fileText, filesize, 1, wrPointer);
        //fileText[filesize]='\n';

        ret = send(connecFd, fileText, sizeof(fileText), 0);

        if( caught_sigint ) 
        {
            printf("\nCaught SIGINT!\n");
            syslog(LOG_DEBUG, "Caught signal, exiting");
            success=false;
            break;
        }

        if( caught_sigterm ) 
        {
            printf("\nCaught SIGTERM!\n");
            syslog(LOG_DEBUG, "Caught signal, exiting");
            success=false;
            break;
        }
    }
    printf("Cleaning up\n");
    remove(FILE_PATH);

    close(connecFd);

    close(sockfd);
    /*
    // Opens a file specified by writefile
    FILE *wrPointer = fopen(writefile, "w");

    // Checks if opening file was successful
    if (wrPointer == NULL)
    {
        syslog(LOG_DEBUG, "Unable to open file");
        fprintf(stderr, "Unable to open file");
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    // Writes the string to the file and closes file
    fprintf(wrPointer, writestr, 0);

    fclose(wrPointer);
    */

   //freeaddrinfo(&addr);
}