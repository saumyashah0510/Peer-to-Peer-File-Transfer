#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "protocol.h"

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    
    printf("Starting tracker on port %d...\n", TRACKER_PORT);
    
    // Step 1: Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf("Socket creation failed\n");
        exit(1);
    }
    printf("✓ Socket created\n");
    
    // Step 2: Set socket option to reuse address
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Step 3: Bind to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TRACKER_PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Bind failed\n");
        exit(1);
    }
    printf("✓ Bound to port %d\n", TRACKER_PORT);
    
    // Step 4: Listen for connections
    if (listen(server_fd, 3) < 0) {
        printf("Listen failed\n");
        exit(1);
    }
    printf("✓ Listening for connections...\n");
    
    // Step 5: Accept ONE connection
    client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (client_fd < 0) {
        printf("Accept failed\n");
        exit(1);
    }
    printf("✓ Client connected!\n");
    
    // Step 6: Read message from client
    int bytes_read = read(client_fd, buffer, 1024);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // Null-terminate the string
        printf("✓ Received: %s\n", buffer);
        
        // Step 7: Check if it's a REGISTER command
        if (strncmp(buffer, "REGISTER", 8) == 0) {
            // Parse: REGISTER filename port
            char filename[MAX_FILENAME];
            int peer_port;
            
            sscanf(buffer, "REGISTER %s %d", filename, &peer_port);
            
            printf("✓ Client wants to register file: %s on port %d\n", 
                   filename, peer_port);
            
            // Send OK response
            char *response = "OK\n";
            send(client_fd, response, strlen(response), 0);
            printf("✓ Sent OK response\n");
        } else {
            // Unknown command
            char *response = "ERROR Unknown command\n";
            send(client_fd, response, strlen(response), 0);
            printf("✗ Unknown command received\n");
        }
    }
    
    // Step 8: Close connections
    close(client_fd);
    close(server_fd);
    
    printf("✓ Tracker shutting down\n");
    return 0;
}