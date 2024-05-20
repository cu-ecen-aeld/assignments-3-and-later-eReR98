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
#include <fcntl.h>

#include "queue.h"

#define MAX_CONNECTIONS 10
#define PORT 9000
#define BUFFER_SIZE 40
#define FILE_PATH "/var/tmp/aesdsocketdata"

bool caught_sigint = false;
bool caught_sigterm = false;
bool success = true;

bool timerReady = false;

struct sockaddr_in addr;

int sockfd;

pthread_mutex_t file_mutex;
pthread_t timerThread;

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
        printf("thread spawned\n");
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

        pthread_mutex_lock(&file_mutex);
        syslog(LOG_DEBUG, "Mutex lock from %d", connecFd);

        FILE *fp = fopen(FILE_PATH, "a+");

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
            fprintf(fp, packetsReceived, 0);

            if(strstr(packetsReceived, "\n"))
            {
                newlineFound = true;
            }
            // clearing out buffers
            memset(&packetsReceived[0], '\0', sizeof(packetsReceived));
            memset(&buff[0], '\0', sizeof(buff));
        }

        //https://stackoverflow.com/questions/71976433/using-fread-to-read-a-text-based-file-best-practices
        fseek(fp, 0, SEEK_END);

        long filesize = ftell(fp);
        rewind(fp);
        char fileText[filesize];

        fread(fileText, filesize, 1, fp);

        send(connecFd, fileText, sizeof(fileText), 0);
        fclose(fp);

        pthread_mutex_unlock(&file_mutex);
        syslog(LOG_DEBUG, "Mutex unlock from %d", connecFd);
        syslog(LOG_DEBUG, "Closed connection from %s", addrStr);
        printf("thread closed\n");

        close(connecFd);
        threadData->threadCompletion_t=true;
        return NULL;
}

void *runTimer() 
{
    time_t prevTime = time(NULL);
    time_t currTime = prevTime;
    bool stopTimer = false;
    syslog(LOG_DEBUG, "stoptimer is %d", stopTimer);
    syslog(LOG_DEBUG, "IN TIMER");
    printf("start timer\n");
    while(stopTimer == false)
    {
        //syslog(LOG_DEBUG, "IN TIMER LOOP");
        if( caught_sigint ) 
        {
            printf("\nCaught SIGINT! in timer\n");
            syslog(LOG_DEBUG, "Caught signal in timer, exiting");
            stopTimer=true;
            break;
        }

        if( caught_sigterm ) 
        {
            printf("\nCaught SIGTERM! in timer\n");
            syslog(LOG_DEBUG, "Caught signal in timer, exiting");
            stopTimer=true;
            break;
        }

        currTime = time(NULL);

        if(timerReady==false)
        {
            timerReady = true;
            syslog(LOG_DEBUG, "Timer ready");
            printf("timer ready\n");
        }
        //syslog(LOG_DEBUG, "Current time %d", (int) currTime);
        //syslog(LOG_DEBUG, "previous time %d", (int) currTime);
        //syslog(LOG_DEBUG, "Time difference %f", difftime(currTime, prevTime));
        if(difftime(currTime, prevTime) < 10.0)
        {
            continue;
        }
        
        prevTime = currTime;

        char outstr[200];
        time_t t;
        struct tm *tmp;

        t = time(NULL);
        tmp = localtime(&t);
        if (tmp == NULL) {
            perror("localtime");
            exit(EXIT_FAILURE);
        }
        int numChars = strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z", tmp);
        if (numChars == 0) {
            fprintf(stderr, "strftime returned 0");
            exit(EXIT_FAILURE);
        }

        //printf("Result string is \"%s\"\n", outstr);

        pthread_mutex_lock(&file_mutex);
        syslog(LOG_DEBUG, "got file mutex in timer");
        int fd = open(FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
        
        //printf("num of chars printed: %d\n", ret);
        // if(ret < 0)
        // {
        //     printf("error in writing date. Return code: %d", ret);
        // }

        outstr[numChars] = '\n';
        char prefix[] = "timestamp:";
        write(fd, prefix, sizeof(prefix)-1);
        write(fd, outstr, numChars+1);
        close(fd);

        pthread_mutex_unlock(&file_mutex);
        printf(outstr);
        syslog(LOG_DEBUG, "released file mutex in timer");
       
    }
    
    return NULL;
}


int main(int argc, char*argv[])
{
    openlog(NULL, 0, LOG_USER);
    pthread_mutex_init(&file_mutex, NULL);
    bool run_as_daemon = false;
    
    if (argc > 1)
    {   
        if(strstr(argv[1], "-d"))
        {
            run_as_daemon=true;
        }
    }
    
    remove(FILE_PATH);

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
    
    
    socklen_t addrlen = sizeof(addr);
    
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    int option = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    ret = bind(sockfd, (struct sockaddr*) &addr, sizeof(addr));
    int errsv = errno;
    if(ret != 0)
    {
        fprintf(stderr, "Error in binding to port. Error code: %d\n", errsv);
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

    syslog(LOG_DEBUG, "creating timer pthread");
    pthread_create(&timerThread, NULL, runTimer, NULL);
    while(timerReady == false)
    {
        // do nothing
    }
    syslog(LOG_DEBUG, "finish creating timer");

    threadEntry_temp = malloc(sizeof(slist_data_t));
    slist_data_t *threadEntry_temp_clean = threadEntry_temp;

    // begin main loop
    while(success==true)
    {
        int connecFd;
        connecFd = accept(sockfd, (struct sockaddr*) &addr, &addrlen);

        if(connecFd < 0)
        {
            fprintf(stderr, "Error in accepting connection\n");
            success=false;
            
        }

        if( caught_sigint ) 
        {
            printf("\nCaught SIGINT!\n");
            syslog(LOG_DEBUG, "Caught signal, exiting");
            success=false;
            
        }

        if( caught_sigterm ) 
        {
            printf("\nCaught SIGTERM!\n");
            syslog(LOG_DEBUG, "Caught signal, exiting");
            success=false;
        }


        struct threadParams_t *threadData = (threadParams_t*)malloc(sizeof(threadParams_t));
        threadData->connecFd_t = connecFd;
        threadData->threadCompletion_t = false;

        pthread_t newThread;

        pthread_create(&newThread, NULL, threadProc, threadData);

        threadEntry = malloc(sizeof(slist_data_t));
        threadEntry->threadId = newThread;
        threadEntry->threadInfo = threadData;
        SLIST_INSERT_HEAD(&head, threadEntry, entries);

        SLIST_FOREACH_SAFE(threadEntry, &head, entries, threadEntry_temp) {
            if ((threadEntry->threadInfo->threadCompletion_t == true) || (success == false))
            {
                pthread_join(threadEntry->threadId, NULL);
                SLIST_REMOVE(&head, threadEntry, slist_data_s, entries);
                free(threadEntry->threadInfo);
                free(threadEntry);
            }
        }
        free(threadEntry_temp);
    }

    close(sockfd);
    free(threadEntry);
    free(threadEntry_temp);
    free(threadEntry_temp_clean);
    pthread_join(timerThread, NULL);
    
    remove(FILE_PATH);
    
}