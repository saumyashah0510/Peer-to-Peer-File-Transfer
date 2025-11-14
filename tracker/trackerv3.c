// Make Tracker REMEMBER Registered Files

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

// Structure to store information about ONE file
typedef struct {
    char filename[MAX_FILENAME];
    char peer_ip[16];
    int peer_port;
} FileRecord;

// Array to store all registered files (max 10 files)
FileRecord registered_files[10];
int file_count = 0;

// Function to add a file to our memory
void add_file(char *filename, char *ip, int port) {
    if (file_count >= 10) {
        printf("✗ Storage full! Cannot register more files.\n");
        return;
    }
    
    strcpy(registered_files[file_count].filename, filename);
    strcpy(registered_files[file_count].peer_ip, ip);
    registered_files[file_count].peer_port = port;
    file_count++;
    
    printf("✓ Registered: %s from %s:%d\n", filename, ip, port);
}

// Function to print all registered files
void print_all_files() {
    printf("\n=== Registered Files ===\n");
    for (int i = 0; i < file_count; i++) {
        printf("%d. %s (from %s:%d)\n", 
               i+1,
               registered_files[i].filename,
               registered_files[i].peer_ip,
               registered_files[i].peer_port);
    }
    printf("========================\n\n");
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address, client_addr;
    int addrlen = sizeof(address);
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024] = {0};
    
    printf("Starting tracker on port %d...\n", TRACKER_PORT);
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf("Socket creation failed\n");
        exit(1);
    }
    
    // Reuse address
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TRACKER_PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Bind failed\n");
        exit(1);
    }
    printf("✓ Bound to port %d\n", TRACKER_PORT);
    
    // Listen
    if (listen(server_fd, 3) < 0) {
        printf("Listen failed\n");
        exit(1);
    }
    printf("✓ Listening for connections...\n");
    
    // MAIN LOOP: Handle multiple clients one by one
    while (1) {
        printf("\nWaiting for next client...\n");
        
        // Accept connection
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("Accept failed\n");
            continue;
        }
        
        // Get client's IP address
        char client_ip[16];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("✓ Client connected from %s\n", client_ip);
        
        // Read message
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(client_fd, buffer, 1024);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("✓ Received: %s\n", buffer);
            
            // Handle REGISTER command
            if (strncmp(buffer, "REGISTER", 8) == 0) {
                char filename[MAX_FILENAME];
                int peer_port;
                
                sscanf(buffer, "REGISTER %s %d", filename, &peer_port);
                
                // Add to our memory
                add_file(filename, client_ip, peer_port);
                
                // Show all registered files
                print_all_files();
                
                // Send OK response
                send(client_fd, "OK\n", 3, 0);
            } 
            else {
                // Unknown command
                send(client_fd, "ERROR Unknown command\n", 22, 0);
            }
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}