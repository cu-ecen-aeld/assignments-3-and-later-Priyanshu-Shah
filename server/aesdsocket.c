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

int main() {
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting AESD Socket Server");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        return -1;
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