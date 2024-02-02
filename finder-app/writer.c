// simple finder script but this time written in c
// writes the text in the second argument to text file specified by the first argument
// Aaron Bruderer

#include <stdio.h>
#include <syslog.h>

int main(int argc, char**argv)
{
    openlog(NULL, 0, LOG_USER);

    // Checks if enough input arguments are present
    if (argc < 3)
    {
        syslog(LOG_DEBUG, "Missing Arguments. 2 Arguments needed to run writer");
        fprintf(stderr, "Missing Arguments. Input arguments must contain\n1) Path to file\n2) String to write into the file\n");
        return 1;   
    }

    // Saves input arguments to char arrays
    char *writefile = argv[1];
    char *writestr = argv[2];

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
}