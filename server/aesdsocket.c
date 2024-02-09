#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>

int main(int argc, char**argv)
{
    openlog(NULL, 0, LOG_USER);

    int sockDesc = socket(PF_INET, SOCK_STREAM, 0);

    // borrowed from lecture
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, "3490", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    }

    //const struct sockaddr custAddr = { .sa_family = AF_INET, .sa_data = "127.0.0.1:9000"};

    int bindRet = bind(sockDesc, (struct sockaddr*) &servinfo, sizeof(struct sockaddr));

    if(bindRet != 0)
    {
        fprintf(stderr, "failed to bind to address/port");
    }


    /*
    // Opens a file specified by writefile
    FILE *filepointer = fopen(writefile, "w");

    // Checks if opening file was successful
    if (filepointer == NULL)
    {
        syslog(LOG_DEBUG, "Unable to open file");
        fprintf(stderr, "Unable to open file");
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    // Writes the string to the file and closes file
    fprintf(filepointer, writestr, 0);

    fclose(filepointer);
    */

   freeaddrinfo(servinfo);
}