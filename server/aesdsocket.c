#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

#define MAX_CONNECTIONS 1
#define PORT 9000
#define BUFFER_SIZE 2048
#define FILE_PATH "/var/tmp/aesdsocketdata"

int findPacketSize(char* packet)
{
    int packetSize = 0;
    char currChar = packet[0];
    //perror("start counting");
    while(1)
    {
        currChar = packet[packetSize+1];

        if (currChar == '\000')
            break;
        
        packetSize++;
    }
    perror("done counting");
    return packetSize;
}

int main(int argc, char**argv)
{
    openlog(NULL, 0, LOG_USER);

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
    perror("bound");
    if(ret != 0)
    {
        fprintf(stderr, "Error in binding to port\n");
        return -1;
    }

    ret = listen(sockfd, MAX_CONNECTIONS);
    perror("listened");
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
    int i = 0;
    while(1)
    {
        perror("start accept");
        connecFd = accept(sockfd, (struct sockaddr*) &addr, &addrlen);
        perror("end accept");
        if(connecFd < 0)
        {
            fprintf(stderr, "Error in accepting connection\n");
            return -1;
        }

        //https://stackoverflow.com/questions/3060950/how-to-get-ip-address-from-sock-structure-in-c

        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&addr;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop( AF_INET, &ipAddr, addrStr, INET_ADDRSTRLEN );

        syslog(LOG_DEBUG, "Accepted connection from %s", addrStr);

        recvRet = recv(connecFd, buff, BUFFER_SIZE-1, 0);
        if(recvRet < 0)
        {
            fprintf(stderr, "Error in read()\n");
            return -1;
        }
        
        


        char packetsReceived[findPacketSize(buff)+1];
        strcpy(packetsReceived, buff);
        packetsReceived[sizeof(packetsReceived)-1] = '\n';
        fprintf(wrPointer, packetsReceived, 0);
        //fprintf(wrPointer, "\n");
        if(i==1){
            printf("test");
        }
        char sendBuff[BUFFER_SIZE] = {0};
        fread(sendBuff, sizeof(sendBuff), 1, wrPointer);
        //https://stackoverflow.com/questions/71976433/using-fread-to-read-a-text-based-file-best-practices
        fseek(wrPointer, 0, SEEK_END);

        long filesize = ftell(wrPointer);
        rewind(wrPointer);
        char fileText[filesize ];
        //fileText[filesize] = '\n';

        fread(fileText, filesize, 1, wrPointer);

        ret = send(connecFd, fileText, sizeof(fileText), 0);

        i++;
    }
    
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