#!/bin/sh
# writer.sh script for Assignment 1. Creates or overwrites the file specified by arg1 with the string in arg2 
# Syntax is "./writer.sh (filepath) (string to write)"
# Returns an error and prints out statements if path to directory is invalid or parameters are missing
# Author: Aaron Bruderer
writefile=$1
writestr=$2

# Checks that at least 2 input arguments are present. Prints message and exits if they are not
if [ $# -lt 2 ];
then
    echo "missing argument"
    exit 1
fi

# Retrieves just the directory that needs to be created and not the file
fileDir=$(dirname "${writefile}")

# Creates directories with printout for errors
if ! mkdir -p "${fileDir}";
then
    echo "failed to create directory for file"
    exit 1
fi

# Creates the file in the directory with printout for errors
if ! touch "${writefile}";
then
    echo "Failed to create file"
    exit 1
fi

# Places the text from the second input argument into the text file
echo "${writestr}" > "${writefile}"