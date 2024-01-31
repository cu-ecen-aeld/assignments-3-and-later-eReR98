#!/bin/sh
# finder.sh script for Assignment 1. Prints the number of files in a directory and the number of files that match the specified search string. 
# Syntax is "./finder.sh (path to directory) (string to look for)"
# Returns an error and prints out statements if path to directory is invalid or parameters are missing
# Author: Aaron Bruderer
filesdir=$1
searchstr=$2

# Checks that at least 2 input arguments are present. Prints message and exits if they are not
if [ $# -lt 2 ];
then
    echo "missing argument. Arguments should be 1) path to directory 2) string to search for"
    exit 1
fi

# Checks if the directory exists and exits if it is not
if [ ! -d "$filesdir" ];
then
    echo "Directory does not exist or is invalid"
    exit 1
fi

# uses find to find all files in directory
filesFound=$(find "${filesdir}" -type f | wc -l)

# pipes the files to wordcount (wc) to count the number of lines and thus number of files
numMatchingFiles=$(grep -r "${searchstr}" "${filesdir}" | wc -l )

# Completes final printout
echo "The number of files are ${filesFound} and the number of matching lines are ${numMatchingFiles}"