#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <signal.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>


// test will not accept fragmented replies. wc long_string.txt 19999
// #define BUFFER_SIZE 32768
// stress to remove
#define BUFFER_SIZE 4
#define DATA_FILE "/var/tmp/aesdsocketdata"


static volatile sig_atomic_t need_exit = 0;

static void on_sigterm(int sig) { need_exit = sig; }

static void register_signals() {
    struct sigaction sigterm_handler = { .sa_handler = on_sigterm };

    if (sigaction(SIGTERM, &sigterm_handler, NULL) == -1) syslog(LOG_ERR, "error: failed sigaction SIGTERM\n"); 
    if (sigaction(SIGINT, &sigterm_handler, NULL) == -1) syslog(LOG_ERR, "error: failed sigaction SIGINT\n"); 
}

static void start_daemon() {
    pid_t pid = fork();
    if (pid < 0) { syslog(LOG_ERR, "error: failed fork()\n"); exit(1); }
    if (pid > 0) exit(0);

    if (setsid() == -1) { syslog(LOG_ERR, "error: failed setsid(): %s\n", strerror(errno)); exit(1); }
    if (chdir("/") == -1) syslog(LOG_ERR, "error: failed chdir(): %s\n", strerror(errno));
    umask(0);

    int d_null = open("/dev/null", O_RDWR);
    if (d_null == -1) { syslog(LOG_ERR, "[%d] error: failed open(): %s\n", getpid(), strerror(errno)); exit(1); }
    if (dup2(d_null, STDIN_FILENO) < 0 || dup2(d_null, STDOUT_FILENO) < 0 || dup2(d_null, STDERR_FILENO) < 0) { 
        syslog(LOG_ERR, "[%d] error: failed dup2(): %s\n", getpid(), strerror(errno));
        exit(1);
    }

    syslog(LOG_DEBUG, "[%d] success: daemon started.\n", getpid());
}

static int create_datafile() { return open(DATA_FILE, O_CREAT|O_WRONLY|O_APPEND, 0644); }
static int remove_datafile(int fd) { return !!(close(fd) | remove(DATA_FILE)); }
/*
static void print_datafile(int fd) {
    if (lseek(fd, 0, SEEK_SET) == -1) {
        syslog(LOG_ERR, "[%d] error: failed lseek\n", getpid());
        return;
    }

    char buf[BUFFER_SIZE];
    uint16_t n = 0;
    while ((n = read(fd, buf, BUFFER_SIZE)) > 0) { write(STDOUT_FILENO, buf, n); }
}
*/

// 2, 88 Positional Reads and Writes
static void socket_write_datafile(int fd, int sock) {
    fdatasync(fd);
    char buf[BUFFER_SIZE];
    ssize_t read_n;
    ssize_t sent_n = 0;
    off_t off_r = 0;

    while ((read_n = pread(fd, buf, BUFFER_SIZE, off_r)) > 0) {
        off_r += read_n; 
        for (ssize_t n = 0; n < read_n; n += sent_n) {
            if ((sent_n = send(sock, buf + n, read_n - n, 0)) <= 0) break;
        }
    }
}

static uint16_t process_message(int fd, char* buf, int n, uint16_t offset, int sock) {
    int err = 1;
    for (ssize_t i = n - 1; i > 0; --i) {
        if (buf[i] == '\n') {
            socket_write_datafile(fd, sock);
            err = 0;
            break;
        }
    }
    uint16_t off_w = ((offset + n) & (BUFFER_SIZE - 1));
    
    return off_w | (err << 15);
}

int main(int argc, char** argv) {
    register_signals();
    int err = 0;
    pid_t m_pid = getpid();

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        syslog(LOG_ERR, "[%d] error: failed socket(): %s\n", m_pid, strerror(errno));
        return -1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    struct addrinfo* ainfo;
    err = getaddrinfo(NULL, "9000", &hints, &ainfo);
    if (err != 0) {
        syslog(LOG_ERR, "[%d] error: getaddrinfo: %s\n", m_pid, gai_strerror(err));
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    socklen_t len = sizeof(*ainfo->ai_addr);
    if (bind(sock, ainfo->ai_addr, len) != 0) {
        freeaddrinfo(ainfo);
        syslog(LOG_ERR, "[%d] error: during bind(): %s\n", m_pid, strerror(errno));
        return -1;
    }

    if (getopt(argc, argv, "d") == 'd') start_daemon();

    if (listen(sock, 10) != 0) {
        syslog(LOG_ERR, "[%d] error: during listen(): %s\n", m_pid, strerror(errno));
        freeaddrinfo(ainfo);
        return -1;
    }

    char srv_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ainfo->ai_addr), srv_ip, INET_ADDRSTRLEN);
    syslog(LOG_DEBUG, "[%d] listening on %s\n", m_pid, srv_ip);

    char c_ip[INET_ADDRSTRLEN];
    struct sockaddr c_addr;
    socklen_t c_addr_size = sizeof(c_addr);
    int c_sock;
    struct sockaddr_in* ipv4;
    char buf[BUFFER_SIZE];
    ssize_t read_n = 0;
    uint16_t off_w = 0;

    int fd_data = create_datafile();
    int fd_rdata = open(DATA_FILE, O_RDONLY);
    if ((fd_data < 0) || (fd_rdata <0)) {
        syslog(LOG_ERR, "[%d] error: during open: %s\n", m_pid, strerror(errno));
        freeaddrinfo(ainfo);
        return 1;
    }

    while (!need_exit) {
        c_sock = accept(sock, &c_addr, &c_addr_size);
        if (c_sock < 0) {
            if (errno == EINTR) break; // Skip EAGAIN EWOULDBLOCK
            syslog(LOG_ERR, "[%d] error: during accept(): %s\n", m_pid, strerror(errno));
            err = -1;
            break;
        }
        setsockopt(c_sock, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));

        if (c_addr.sa_family != AF_INET) {
            syslog(LOG_ERR, "[%d] unsupported connection protocol: %d\n", m_pid, AF_INET);
            close(c_sock);
            err = -1;
            break;
        }

        ipv4 = (struct sockaddr_in*) &c_addr;
        inet_ntop(AF_INET, &ipv4->sin_addr, c_ip, sizeof(c_ip));
        syslog(LOG_DEBUG, "[%d] Accepted connection from %s\n", m_pid, c_ip);

        while((read_n = recv(c_sock, buf + off_w, BUFFER_SIZE - off_w, 0)) > 0 && !need_exit) {
            write(fd_data, buf + off_w, read_n);

            // Optional error handling if need validation
            uint16_t pout = process_message(fd_rdata, buf, read_n, off_w, c_sock);
            off_w = pout & ((1 << 15) - 1);
        }

        off_w = 0;
        close(c_sock);
        memset(&c_addr, 0, sizeof(c_addr));
        syslog(LOG_DEBUG, "[%d] Closed connection from %s\n", m_pid, c_ip);
    }

    switch (need_exit) {
        case SIGINT: syslog(LOG_DEBUG, "[%d] Caught SIGINT, exiting.\n", getpid()); break;
        case SIGTERM: syslog(LOG_DEBUG, "[%d] Caught SIGTERM, exiting.\n", getpid()); break;
        default: break;
    }
    
    // print_datafile(fd_rdata);

    close(sock);
    freeaddrinfo(ainfo);
    if (close(fd_rdata) < 0) syslog(LOG_ERR, "[%d] error: during close(): %s\n", m_pid, strerror(errno));
    if (remove_datafile(fd_data) < 0) syslog(LOG_ERR, "[%d] error: during remove_datafile: %s\n", m_pid, strerror(err));

    return err;
    
}
