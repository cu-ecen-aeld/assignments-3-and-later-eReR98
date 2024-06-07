// Code for assignment 6 pt 1

// modified to save to /dev/aesdchar instead of a file
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
#include <netdb.h>

#include "queue.h"
#include <sys/stat.h>

#include "../aesd-char-driver/aesd_ioctl.h"
#include <sys/ioctl.h>

#define IOCTL_CMD "AESDCHAR_IOCSEEKTO:"

#define USE_AESD_CHAR_DEVICE 1

#define MAX_CONNECTIONS 10
#define PORT 9000
#define BUFFER_SIZE 128

#if USE_AESD_CHAR_DEVICE
    #define FILE_PATH "/dev/aesdchar"
#else
    #define FILE_PATH "/dev/aesdchar"
#endif

struct aesd_seekto seekto;

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
        struct aesd_seekto seekto;
        bool cmdFound = false;


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
        printf("before open");
        int newfd = open(FILE_PATH, O_CREAT | O_RDWR | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);
        printf("after open");
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
            //fprintf(newfd, packetsReceived, 0);

            // checking for command in string

            char *cmdMatch = strstr(packetsReceived, IOCTL_CMD);
            int arg1 = 0;
            int arg2 = 0;

            if(cmdMatch)
            {
                
                cmdFound = true;
                cmdFound=cmdFound; // temp fix

                sscanf(cmdMatch, "%*[^0123456789]%d%*[^0123456789]%d", &arg1, &arg2);
                seekto.write_cmd=arg1;
                seekto.write_cmd_offset=arg2;

                printf("COMMAND FOUND: %s, arg1=%d, arg2=%d\n", cmdMatch, arg1, arg2);
                if(ioctl(newfd, AESDCHAR_IOCSEEKTO, &seekto) != 0)
                {
                    break;
                }
                
                
            } 
            else 
            {
                write(newfd, buff, recvRet);
                printf("current buff:\n");
                printf("%s",buff);
                printf("\n");
                if(strstr(packetsReceived, "\n"))
                {
                    newlineFound = true;
                    lseek(newfd, 0, SEEK_SET);
                }
            }




            // clearing out buffers
            printf("startclearbuffer\n");
            memset(&packetsReceived[0], '\0', sizeof(packetsReceived));
            memset(&buff[0], '\0', sizeof(buff));
            printf("endclearbuffer\n");
        }
        printf("startlseek\n");
        //https://stackoverflow.com/questions/71976433/using-fread-to-read-a-text-based-file-best-practices
        
        // if(cmdFound)
        // {
        //     lseek(newfd, 0, SEEK_CUR);
        // }
        // else
        // {
            
        // }

        printf("endlseek\n");
        int bytesRead = BUFFER_SIZE;
        memset(buff, 0, sizeof(buff));

        while((bytesRead > 0))
        {
            bytesRead = read(newfd, buff, BUFFER_SIZE);
            syslog(LOG_CRIT, "buffer read by aesdsocket: %s", buff);
            send(connecFd, buff, bytesRead, 0);
            memset(buff, 0, sizeof(buff));
            
            
        }

        // long filesize = ftell(fp);
        // rewind(fp);
        // char fileText[filesize];

        // fread(fileText, filesize, 1, fp);

        // send(connecFd, fileText, sizeof(fileText), 0);
        // fclose(fp);

        pthread_mutex_unlock(&file_mutex);
        syslog(LOG_DEBUG, "Mutex unlock from %d", connecFd);
        syslog(LOG_DEBUG, "Closed connection from %s", addrStr);
        printf("thread closed\n");

        close(connecFd);
        threadData->threadCompletion_t=true;
        return NULL;
}

// Commenting out timer for this code

// void *runTimer() 
// {
//     time_t prevTime = time(NULL);
//     time_t currTime = prevTime;
//     bool stopTimer = false;
//     syslog(LOG_DEBUG, "stoptimer is %d", stopTimer);
//     syslog(LOG_DEBUG, "IN TIMER");
//     printf("start timer\n");
//     while(stopTimer == false)
//     {
//         //syslog(LOG_DEBUG, "IN TIMER LOOP");
//         if( caught_sigint ) 
//         {
//             printf("\nCaught SIGINT! in timer\n");
//             syslog(LOG_DEBUG, "Caught signal in timer, exiting");
//             stopTimer=true;
//             break;
//         }

//         if( caught_sigterm ) 
//         {
//             printf("\nCaught SIGTERM! in timer\n");
//             syslog(LOG_DEBUG, "Caught signal in timer, exiting");
//             stopTimer=true;
//             break;
//         }

//         currTime = time(NULL);

//         if(timerReady==false)
//         {
//             timerReady = true;
//             syslog(LOG_DEBUG, "Timer ready");
//             printf("timer ready\n");
//         }
//         //syslog(LOG_DEBUG, "Current time %d", (int) currTime);
//         //syslog(LOG_DEBUG, "previous time %d", (int) currTime);
//         //syslog(LOG_DEBUG, "Time difference %f", difftime(currTime, prevTime));
//         if(difftime(currTime, prevTime) < 10.0)
//         {
//             continue;
//         }
        
//         prevTime = currTime;

//         char outstr[200];
//         time_t t;
//         struct tm *tmp;

//         t = time(NULL);
//         tmp = localtime(&t);
//         if (tmp == NULL) {
//             perror("localtime");
//             exit(EXIT_FAILURE);
//         }
//         int numChars = strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z", tmp);
//         if (numChars == 0) {
//             fprintf(stderr, "strftime returned 0");
//             exit(EXIT_FAILURE);
//         }

//         //printf("Result string is \"%s\"\n", outstr);

//         pthread_mutex_lock(&file_mutex);
//         syslog(LOG_DEBUG, "got file mutex in timer");
//         int fd = open(FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
        
//         //printf("num of chars printed: %d\n", ret);
//         // if(ret < 0)
//         // {
//         //     printf("error in writing date. Return code: %d", ret);
//         // }

//         outstr[numChars] = '\n';
//         char prefix[] = "timestamp:";
//         write(fd, prefix, sizeof(prefix)-1);
//         write(fd, outstr, numChars+1);
//         close(fd);

//         pthread_mutex_unlock(&file_mutex);
//         printf("timestamp recorded\n");
//         syslog(LOG_DEBUG, "released file mutex in timer");
       
//     }
    
//     return NULL;
// }


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
    
    //remove(FILE_PATH);

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

    // Trying alternate way of setting up connection created by referencing rajohnson

    struct addrinfo hints;
    struct addrinfo* serverInfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    getaddrinfo(NULL, "9000", &hints, &serverInfo);
    

    int option = 1;
    printf("REACHED SOCKET\n");
    sockfd = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(int));
    // // int option = 1;
    // // setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    // addr.sin_family = AF_INET;
    // addr.sin_addr.s_addr = INADDR_ANY;
    // addr.sin_port = htons(PORT);

    ret = bind(sockfd, serverInfo->ai_addr, serverInfo->ai_addrlen);
    // int errsv = errno;
    // if(ret != 0)
    // {
    //     fprintf(stderr, "Error in binding to port. Error code: %d\n", errsv);
    //     return -1;
    // }

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

    //syslog(LOG_DEBUG, "creating timer pthread");
    //printf("CREATING TIMER\n");
    
    // removed for A8
    //pthread_create(&timerThread, NULL, runTimer, NULL);
    
    // while(timerReady == false)
    // {
    //     // do nothing
    // }
    //syslog(LOG_DEBUG, "finish creating timer");
    //printf("FINISH CREATING TIMER\n");

    threadEntry_temp = malloc(sizeof(slist_data_t));
    slist_data_t *threadEntry_temp_clean = threadEntry_temp;

    // begin main loop
    printf("BEGIN MAIN LOOP\n");
    while(success==true)
    {
        int connecFd;
        printf("WAITING FOR CONNECTION\n");
        connecFd = accept(sockfd, (struct sockaddr*) &addr, &addrlen);
        printf("CONNECTION FOUND\n");
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
    
}