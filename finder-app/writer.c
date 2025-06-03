#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <message>\n", argv[0]);
        openlog("writer", LOG_PID, LOG_USER);
        syslog(LOG_ERR, "Invalid number of arguments: %d", argc - 1);
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr  = argv[2];    

    openlog("writer", LOG_PID, LOG_USER);
    
    FILE* fp = fopen(writefile, "w");
    if(fp == NULL) {
        syslog(LOG_ERR, "Failed to open file %s", writefile);
        closelog();
        return 1;
    }

    if(fprintf(fp, "%s\n", writestr) < 0) {
        syslog(LOG_ERR, "Failed to write to file %s", writefile);
        fclose(fp);
        closelog();
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    fclose(fp);
    closelog();
    return 0;
}