#include <stdio.h>        
#include <stdlib.h>     
#include <string.h>         
#include <unistd.h>         
#include <sys/socket.h>     
#include <netinet/in.h>     
#include "protocol.h"       

int main(){

    /* 
    server_fd: The listening socket (like a receptionist)
    client_fd: Connection to one specific client (like a phone line)
    address: Stores IP address and port information
    */

    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    printf("Starting tracker on port %d...\n", TRACKER_PORT);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf("Socket creation failed\n");
        exit(1);
    }
    printf("✓ Socket created\n");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TRACKER_PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Bind failed\n");
        exit(1);
    }
    printf("✓ Bound to port %d\n", TRACKER_PORT);
    
    // Step 3: Listen for connections
    if (listen(server_fd, 3) < 0) {
        printf("Listen failed\n");
        exit(1);
    }
    printf("✓ Listening for connections...\n");
    
    // Step 4: Accept ONE connection
    client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (client_fd < 0) {
        printf("Accept failed\n");
        exit(1);
    }
    printf("✓ Client connected!\n");
    
    // Step 5: Send a simple message
    char *message = "Hello from tracker!\n";
    send(client_fd, message, strlen(message), 0);
    printf("✓ Message sent to client\n");
    
    // Step 6: Close connections
    close(client_fd);
    close(server_fd);
    
    printf("✓ Tracker shutting down\n");

    return 0;
}


