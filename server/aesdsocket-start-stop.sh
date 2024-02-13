#!/bin/sh

# from lecture
case "$1" in
    start)
        echo "Starting aesd-socket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    
    stop)
        echo "Stopping aesd-socket"
        start-stop-daemon -K -n aesdsocket --signal SIGTERM
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac