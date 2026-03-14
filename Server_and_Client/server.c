#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 55555
#define BUFFER_SIZE 1024

int main() { //SERVER
     // Create server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(1);
    }
    // Bind the socket to the port
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_address.sin_zero), 0, 8);
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("bind");
        close(server_socket); // DONT FORGET TO CLOSE HERE
        exit(1);
    }
    // Start listening
    int max_number_of_connections_at_once = 5;
    if (listen(server_socket, max_number_of_connections_at_once) < 0) {
        perror("listen");
        close(server_socket); // DONT FORGET TO CLOSE HERE
        exit(1);
    }
    printf("Server listening on port %d...\n", PORT);
    struct sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    unsigned int client_address_length = sizeof(client_address);
    // Accept incoming connection
    int new_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
    if (new_socket < 0) {
        perror("accept");
        exit(1);
    }
    // Communicate with client
    char buffer[BUFFER_SIZE] = {0};
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(new_socket, buffer, BUFFER_SIZE);
        if (valread <= 0) break;
        printf("Client: %s\n", buffer);
        send(new_socket, buffer, strlen(buffer), 0);
    }
    close(new_socket);
    close(server_socket);
    return 0;
}
