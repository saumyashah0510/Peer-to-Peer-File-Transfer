#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

// Global variables
int my_port;
char my_ip[16] = "127.0.0.1";  // localhost for now

// Function to clear screen
void clear_screen() {
    printf("\033[2J\033[H");  // ANSI escape code
}

// Function to connect to tracker and send message
int connect_to_tracker(char *message, char *response) {
    int sock;
    struct sockaddr_in tracker_addr;
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("✗ Socket creation failed\n");
        return -1;
    }
    
    // Set tracker address
    tracker_addr.sin_family = AF_INET;
    tracker_addr.sin_port = htons(TRACKER_PORT);
    tracker_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // Connect to tracker
    if (connect(sock, (struct sockaddr *)&tracker_addr, sizeof(tracker_addr)) < 0) {
        printf("✗ Cannot connect to tracker\n");
        close(sock);
        return -1;
    }
    
    // Send message
    send(sock, message, strlen(message), 0);
    
    // Receive response
    memset(response, 0, 1024);
    int bytes = read(sock, response, 1024);
    if (bytes > 0) {
        response[bytes] = '\0';
    }
    
    close(sock);
    return 0;
}

// Function to register a file
void register_file() {
    char filename[MAX_FILENAME];
    char message[1024];
    char response[1024];
    
    printf("\n--- Register File ---\n");
    printf("Enter filename to register: ");
    scanf("%s", filename);
    getchar();  // consume newline
    
    // Build REGISTER message
    sprintf(message, "REGISTER %s %d\n", filename, my_port);
    
    printf("Connecting to tracker...\n");
    if (connect_to_tracker(message, response) == 0) {
        if (strncmp(response, "OK", 2) == 0) {
            printf("✓ File '%s' registered successfully!\n", filename);
        } else {
            printf("✗ Registration failed: %s\n", response);
        }
    }
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Function to query for a file
void query_file() {
    char filename[MAX_FILENAME];
    char message[1024];
    char response[1024];
    
    printf("\n--- Query File ---\n");
    printf("Enter filename to search: ");
    scanf("%s", filename);
    getchar();  // consume newline
    
    // Build QUERY message
    sprintf(message, "QUERY %s\n", filename);
    
    printf("Searching...\n");
    if (connect_to_tracker(message, response) == 0) {
        if (strncmp(response, "PEERS", 5) == 0) {
            printf("\n✓ Found peers:\n");
            printf("-------------------\n");
            printf("%s", response);
            printf("-------------------\n");
        } else if (strncmp(response, "ERROR", 5) == 0) {
            printf("✗ File not found\n");
        }
    }
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Background listener thread (will handle incoming peer connections)
void* listener_thread(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in address, client_addr;
    int addrlen = sizeof(address);
    socklen_t client_len = sizeof(client_addr);
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf("✗ Listener socket creation failed\n");
        return NULL;
    }
    
    // Reuse address
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(my_port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("✗ Listener bind failed on port %d\n", my_port);
        close(server_fd);
        return NULL;
    }
    
    // Listen
    if (listen(server_fd, 3) < 0) {
        printf("✗ Listener listen failed\n");
        close(server_fd);
        return NULL;
    }
    
    printf("✓ Listener started on port %d\n", my_port);
    
    // Accept connections (for future use - file uploads)
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            continue;
        }
        
        // Get client IP
        char client_ip[16];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        
        printf("\n[Background] Peer connected from %s\n", client_ip);
        
        // For now, just close (we'll add file upload logic later)
        close(client_fd);
    }
    
    close(server_fd);
    return NULL;
}

// Function to show menu
void show_menu() {
    clear_screen();
    printf("========================================\n");
    printf("       P2P File Transfer - Peer\n");
    printf("========================================\n");
    printf("Your IP: %s\n", my_ip);
    printf("Your Port: %d\n", my_port);
    printf("========================================\n\n");
    
    printf("MENU:\n");
    printf("1. Register a file with tracker\n");
    printf("2. Query for a file\n");
    printf("3. Exit\n");
    printf("\nEnter choice: ");
}

int main(int argc, char *argv[]) {
    pthread_t listener_tid;
    int choice;
    
    // Check command line arguments
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        printf("Example: %s 9000\n", argv[0]);
        exit(1);
    }
    
    // Get port from command line
    my_port = atoi(argv[1]);
    
    printf("========================================\n");
    printf("   Starting P2P Peer on port %d\n", my_port);
    printf("========================================\n\n");
    
    // Start listener thread in background
    printf("Starting listener thread...\n");
    if (pthread_create(&listener_tid, NULL, listener_thread, NULL) != 0) {
        printf("✗ Failed to create listener thread\n");
        exit(1);
    }
    
    sleep(1);  // Give listener time to start
    
    // Main menu loop
    while (1) {
        show_menu();
        
        if (scanf("%d", &choice) != 1) {
            // Clear invalid input
            while(getchar() != '\n');
            continue;
        }
        getchar();  // consume newline
        
        switch(choice) {
            case 1:
                register_file();
                break;
            case 2:
                query_file();
                break;
            case 3:
                printf("\n✓ Exiting...\n");
                exit(0);
            default:
                printf("\n✗ Invalid choice!\n");
                sleep(1);
        }
    }
    
    return 0;
}