#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 55555
#define BUFFER_SIZE 1024

int main() {// CLIENT_SELECT
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported"); // 0 if invalid, -1 if unsupported
        close(sock);
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
        perror("connect");
        close(sock);
        return -1;
    }
    char buf[BUFFER_SIZE];
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset);
    FD_SET(sock, &fdset); // read from server socket so you can display it for the client.
    fd_set basket = fdset;
    while (1) {
        fdset = basket;
        if (select(sock + 1, &fdset, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }
        if (FD_ISSET(STDIN_FILENO, &fdset)) {
            int bytes_read = read(STDIN_FILENO, buf, BUFFER_SIZE);
            if (bytes_read <= 0) break;
            write(STDOUT_FILENO, "You: ", strlen("You: "));
            write(sock, buf, bytes_read);
        }
        if (FD_ISSET(sock, &fdset)) {
            int bytes_read = read(sock, buf, BUFFER_SIZE);
            if (bytes_read <= 0) break;
            write(STDOUT_FILENO, "Server: ", strlen("Server: "));
            write(STDOUT_FILENO, buf, bytes_read);
        }
    }
    close(sock);
    return 0;
}
