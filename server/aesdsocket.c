#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

int server_socket;
int client_socket;

void cleanup() {
    if (client_socket >= 0) {
        close(client_socket);
    }
    if (server_socket >= 0) {
        close(server_socket);
    }
    unlink(DATA_FILE);
    syslog(LOG_INFO, "Caught signal, exiting");
}

void signal_handler(int signum) {
    (void)signum;
    cleanup();
    exit(0);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char *ip;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len);
    ip = inet_ntoa(client_addr.sin_addr);
    syslog(LOG_INFO, "Accepted connection from %s", ip);

    int fd = open(DATA_FILE, O_APPEND | O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        perror("Failed to open data file");
        return;
    }

    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        char *newline = strchr(buffer, '\n');
        if (newline) {
            *newline = '\0'; // Terminate the string at the newline
            write(fd, buffer, strlen(buffer));
            write(fd, "\n", 1); // Append newline to the file
            lseek(fd, 0, SEEK_SET); // Reset file pointer to the beginning
            ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE - 1);
            buffer[bytes_read] = '\0'; // Null-terminate the read data
            send(client_socket, buffer, bytes_read, 0);
        }
    }

    close(fd);
    syslog(LOG_INFO, "Closed connection from %s", ip);
    close(client_socket);
}

// Function to daemonize the process
void daemonize() {
    // First fork
    pid_t pid = fork();
    
    // Error check
    if (pid < 0) {
        perror("Failed to fork");
        exit(1);
    }
    
    // Parent exits
    if (pid > 0) {
        exit(0);
    }
    
    // Child continues
    
    // Create new session and process group
    if (setsid() < 0) {
        perror("Failed to create new session");
        exit(1);
    }
    
    // Set file permissions
    umask(0);
    
    // Change directory to root
    if (chdir("/") < 0) {
        perror("Failed to change directory");
        exit(1);
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Redirect standard file descriptors to /dev/null
    int fd = open("/dev/null", O_RDWR);
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) {
        close(fd);
    }
}

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    
    // Check if the daemon flag is provided
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    }
    
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting AESD Socket Server");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        return -1;
    }
    
    // Set socket options to allow port reuse
    int yes = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("Failed to set socket options");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to port 9000
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        return -1;
    }
    
    // Run as daemon if flag is provided
    if (daemon_mode) {
        syslog(LOG_INFO, "Running as daemon");
        daemonize();
    }

    if (listen(server_socket, 5) < 0) {
        perror("Failed to listen on socket");
        return -1;
    }

    while (1) {
        client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            perror("Failed to accept connection");
            continue;
        }
        handle_client(client_socket);
    }

    cleanup();
    return 0;
}