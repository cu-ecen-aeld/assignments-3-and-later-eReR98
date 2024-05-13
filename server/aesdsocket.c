// Code for assignment 6 pt 1
#include <stdio.h>
#include <stdlib.h>
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
#include <pthread.h>

#include "queue.h"

#define MAX_CONNECTIONS 1
#define PORT 9000
#define BUFFER_SIZE 40
#define FILE_PATH "/var/tmp/aesdsocketdata"

bool caught_sigint = false;
bool caught_sigterm = false;
bool success = true;

struct sockaddr_in addr;
FILE *wrPointer;
pthread_mutex_t mutex;

typedef struct threadParams_t
{
    int connecFd_t;
    bool threadCompletion_t;
} threadParams_t;

SLIST_HEAD(slisthead, slist_data_s);
typedef struct slist_data_s slist_data_t;
struct slist_data_s
{
    struct threadParams_t *threadInfo;
    pthread_t threadId;
    SLIST_ENTRY(slist_data_s) entries;
} ;

// just to avoid compiler warning
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
        syslog(LOG_DEBUG, "Caught signal, exiting");
        printf("caught SIGINT\n");
    } else if ( signal_number == SIGTERM ) {
        caught_sigterm = true;
        syslog(LOG_DEBUG, "Caught signal, exiting");
        printf("caught SIGTERM\n");
    }
    errno = errno_saved;
}

void *threadProc(void *threadParams)
{
    //https://stackoverflow.com/questions/3060950/how-to-get-ip-address-from-sock-structure-in-c

        struct threadParams_t* threadData = (struct threadParams_t *) threadParams;

        int connecFd = threadData->connecFd_t;
        threadData->threadCompletion_t = false;

        ssize_t recvRet;
        char buff[BUFFER_SIZE] = {0};

        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&addr;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop( AF_INET, &ipAddr, addrStr, INET_ADDRSTRLEN );

        syslog(LOG_DEBUG, "Accepted connection from %s", addrStr);

        bool newlineFound = false;

        pthread_mutex_lock(&mutex);

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
            // clearing out buffers
            memset(&packetsReceived[0], '\0', sizeof(packetsReceived));
            memset(&buff[0], '\0', sizeof(buff));
        }

        //https://stackoverflow.com/questions/71976433/using-fread-to-read-a-text-based-file-best-practices
        fseek(wrPointer, 0, SEEK_END);

        long filesize = ftell(wrPointer);
        rewind(wrPointer);
        char fileText[filesize];

        fread(fileText, filesize, 1, wrPointer);

        send(connecFd, fileText, sizeof(fileText), 0);

        pthread_mutex_unlock(&mutex);

        syslog(LOG_DEBUG, "Closed connection from %s", addrStr);

        close(connecFd);
        threadData->threadCompletion_t=true;
        return NULL;
}

void runTimer() 
{
    while(true)
    {
        sleep(10);

        char outstr[200];
        time_t t;
        struct tm *tmp;

        t = time(NULL);
        tmp = localtime(&t);
        if (tmp == NULL) {
            perror("localtime");
            exit(EXIT_FAILURE);
        }
        
        if (strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z", tmp) == 0) {
            fprintf(stderr, "strftime returned 0");
            exit(EXIT_FAILURE);
        }

        printf("Result string is \"%s\"\n", outstr);

        printf("timer ran\n");

        if(caught_sigint || caught_sigterm)
        {
            exit(0);
        }
    }

    //return NULL;
}

int main(int argc, char*argv[])
{
    openlog(NULL, 0, LOG_USER);
    pthread_mutex_init(&mutex, NULL);
    bool run_as_daemon = false;
    
    if (argc > 1)
    {   
        if(strstr(argv[1], "-d"))
        {
            run_as_daemon=true;
        }
    }
    
    struct sigaction sigAct = {0};

    sigAct.sa_handler=signal_handler;

    if( sigaction(SIGTERM, &sigAct, NULL) != 0 ) {
        printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
        return -1;
    }
    if( sigaction(SIGINT, &sigAct, NULL) ) {
        printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
        return -1;
    }

    slist_data_t *threadEntry = NULL;
    slist_data_t *threadEntry_temp = NULL;

    struct slisthead head;
    SLIST_INIT(&head);

    int ret;
    int sockfd;
    
    socklen_t addrlen = sizeof(addr);
    remove(FILE_PATH);
    sockfd = socket(PF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    ret = bind(sockfd, (struct sockaddr*) &addr, sizeof(addr));

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
        exit(0);
    }

    ret = listen(sockfd, MAX_CONNECTIONS);

    if(ret != 0)
    {
        fprintf(stderr, "Error in calling listen()\n");
        return -1;
    }


    wrPointer = fopen(FILE_PATH, "a+");
    if(wrPointer==NULL)
    {
        syslog(LOG_DEBUG, "Unable to open file");
        return -1;
    }

    // Forking for timer

    pid = fork();

    if(pid == 0)
    {
        runTimer();
        return 0;
    }

    // begin main loop
    while(success==true)
    {
        int connecFd;
        connecFd = accept(sockfd, (struct sockaddr*) &addr, &addrlen);

        if(connecFd < 0)
        {
            fprintf(stderr, "Error in accepting connection\n");
            success=false;
            break;
        }

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


        struct threadParams_t *threadData = (threadParams_t*)malloc(sizeof(threadParams_t));
        threadData->connecFd_t = connecFd;
        threadData->threadCompletion_t = false;

        pthread_t newThread;

        pthread_create(&newThread, NULL, threadProc, threadData);

        threadEntry = malloc(sizeof(slist_data_t));
        threadEntry_temp = malloc(sizeof(slist_data_t));
        threadEntry->threadId = newThread;
        threadEntry->threadInfo = threadData;
        SLIST_INSERT_HEAD(&head, threadEntry, entries);

        SLIST_FOREACH_SAFE(threadEntry, &head, entries, threadEntry_temp) {
            if (threadEntry->threadInfo->threadCompletion_t == true)
            {
                pthread_join(threadEntry->threadId, NULL);
                SLIST_REMOVE(&head, threadEntry, slist_data_s, entries);
            }
        }
    }
    remove(FILE_PATH);

    close(sockfd);

}