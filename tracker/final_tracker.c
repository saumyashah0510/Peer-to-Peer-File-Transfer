#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"

// Stores filename, ip address and port number
typedef struct {
    char filename[MAX_FILENAME];
    char peer_ip[16];
    int peer_port;
} FileRecord;

// Array to store all registered files (max 10 files)
FileRecord registered_files[10]; 
int file_count = 0; // counter

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

// Function to find peers who have a specific file
// response is pointer to buffer where we will write response message
void find_peers(char *filename, char *response) {
    int found = 0;
    char temp[1024] = "";
    
    // Search through all registered files
    for (int i = 0; i < file_count; i++) {
        if (strcmp(registered_files[i].filename, filename) == 0) {
            // Found a match!
            found++;
            char peer_info[100]; // temporary buffer to store one peers information
            sprintf(peer_info, "%s:%d\n", 
                    registered_files[i].peer_ip, 
                    registered_files[i].peer_port); // Creates a string like "192.168.1.5:9000\n" and writes to peer_info buffer
            strcat(temp, peer_info); // concats peer_info to temp to get final buffer to write to response
        }
    }
    
    if (found > 0) {
        sprintf(response, "PEERS %d\n%s", found, temp);
        printf("✓ Found %d peer(s) with file: %s\n", found, filename);
    } else {
        sprintf(response, "ERROR File not found\n");
        printf("✗ No peers have file: %s\n", filename);
    }
}

// Function to print all registered files
void print_all_files() {
    printf("\n=== Registered Files ===\n");
    if (file_count == 0) {
        printf("(No files registered yet)\n");
    }
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
    char response[1024] = {0};
    
    printf("========================================\n");
    printf("   P2P File Transfer - Tracker Server  \n");
    printf("========================================\n");
    printf("Starting tracker on port %d...\n", TRACKER_PORT);
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf("✗ Socket creation failed\n");
        exit(1);
    }
    printf("✓ Socket created\n");
    
    
    int opt = 1;

    /*
    set options on a socket file descriptor : 
    server_fd : which socket
    SOL_SOCKET : Socket level option (general for all protocols)
    SO_REUSEADDR : Allow reuse of local addresses. Without this, if you stop the server and restart it immediately, you'll get "Address already in use" error
    */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    address.sin_port = htons(TRACKER_PORT); // htons : host to network server : for converting port number, TRACKER_PORT from protocols.h i.e. 8080
    

    // Bind associates the socket with an address (IP + port)
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("✗ Bind failed\n");
        close(server_fd);
        exit(1);
    }
    printf("✓ Bound to 0.0.0.0:%d (listening on all network interfaces)\n", TRACKER_PORT);
    

    // Listen marks socket to accept connections(Backlog : 3 i.e. maximum 3 waiting in queue)
    if (listen(server_fd, 3) < 0) {
        printf("✗ Listen failed\n");
        close(server_fd);
        exit(1);
    }
    printf("✓ Listening for connections...\n");
    printf("========================================\n");
    

    // MAIN LOOP: Handle multiple clients forever
    while (1) {

        printf("\n[Waiting for client...]\n");
        

        // Accept connection
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("✗ Accept failed\n");
            continue;
        }
        

        // Get client's IP address (REAL IP from network)
        char client_ip[16]; 
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)); // converts binary IP to human-readable string
        printf("✓ Client connected from %s\n", client_ip);
        
        // Read message from client
        memset(buffer, 0, sizeof(buffer)); // memset : Memory set - fills memory with a specific value
        int bytes_read = read(client_fd, buffer, 1024); 
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("✓ Received: %s", buffer);
            
            // ============ Handle REGISTER command ============
            if (strncmp(buffer, "REGISTER", 8) == 0) {
                char filename[MAX_FILENAME];
                int peer_port;
                
                sscanf(buffer, "REGISTER %s %d", filename, &peer_port);
                
                // Store with REAL IP (from socket, not from message)
                add_file(filename, client_ip, peer_port);
                print_all_files();
                
                send(client_fd, "OK\n", 3, 0);
                printf("✓ Sent: OK\n");
            }
            // ============ Handle QUERY command ============
            else if (strncmp(buffer, "QUERY", 5) == 0) {
                char filename[MAX_FILENAME];
                
                sscanf(buffer, "QUERY %s", filename);
                printf("✓ Searching for: %s\n", filename);
                
                memset(response, 0, sizeof(response));
                find_peers(filename, response);
                
                send(client_fd, response, strlen(response), 0);
                printf("✓ Sent response\n");
            }
            // ============ Handle unknown command ============
            else {
                printf("✗ Unknown command\n");
                send(client_fd, "ERROR Unknown command\n", 22, 0);
            }
        }
        
        close(client_fd);
        printf("[Connection closed]\n");
    }
    
    close(server_fd);
    return 0;
}