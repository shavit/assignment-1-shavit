#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>


void print_help() {
    fprintf(stderr, "Usage: writer FILEPATH TEXT\n\n");
}


int main(int argc, char** argv) {
    if (argc < 3) {
        print_help();
        return 1;
    }

    const char* filepath = argv[1];
    const char* text = argv[2];

    int fd = open(filepath, O_CREAT|O_WRONLY, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Could not open flie %s: %s", filepath, strerror(errno));
        return fd;
    }

    openlog(NULL, 0, LOG_USER); 
    syslog(LOG_DEBUG, "Writing %s to %s", text, filepath);
    ssize_t n = write(fd, text, strlen(text));
    if (n == -1) {
        syslog(LOG_ERR, "Could not write to %s: %s", filepath, strerror(errno));
        return n;
    }


    return 0;
}
