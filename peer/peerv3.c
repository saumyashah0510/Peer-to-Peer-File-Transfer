#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "file_ops.h"

// Global variables
int my_port;
char my_ip[16] = "127.0.0.1";
char shared_dir[256];
char pieces_dir[256];
char downloads_dir[256];

// Clear screen
void clear_screen() {
    printf("\033[2J\033[H");
}

// Connect to tracker
int connect_to_tracker(char *message, char *response) {
    int sock;
    struct sockaddr_in tracker_addr;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("✗ Socket creation failed\n");
        return -1;
    }
    
    tracker_addr.sin_family = AF_INET;
    tracker_addr.sin_port = htons(TRACKER_PORT);
    tracker_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr *)&tracker_addr, sizeof(tracker_addr)) < 0) {
        printf("✗ Cannot connect to tracker\n");
        close(sock);
        return -1;
    }
    
    send(sock, message, strlen(message), 0);
    
    memset(response, 0, 1024);
    int bytes = read(sock, response, 1024);
    if (bytes > 0) {
        response[bytes] = '\0';
    }
    
    close(sock);
    return 0;
}

// Connect to a peer and request a piece
int request_piece_from_peer(char *peer_ip, int peer_port, char *filename, int piece_index, char *buffer, int *bytes_received) {
    int sock;
    struct sockaddr_in peer_addr;
    char request[512];
    char response[1024];
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("✗ Socket creation failed\n");
        return -1;
    }
    
    // Set peer address
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip);
    
    // Connect to peer
    if (connect(sock, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
        printf("✗ Cannot connect to peer %s:%d\n", peer_ip, peer_port);
        close(sock);
        return -1;
    }
    
    // Send request: REQUEST_PIECE filename piece_index
    sprintf(request, "REQUEST_PIECE %s %d\n", filename, piece_index);
    send(sock, request, strlen(request), 0);
    
    // Read response header: SEND_PIECE piece_index size
    int header_bytes = read(sock, response, 1024);
    if (header_bytes <= 0) {
        printf("✗ No response from peer\n");
        close(sock);
        return -1;
    }
    response[header_bytes] = '\0';
    
    // Parse response
    int received_index, data_size;
    if (sscanf(response, "SEND_PIECE %d %d\n", &received_index, &data_size) != 2) {
        printf("✗ Invalid response format\n");
        close(sock);
        return -1;
    }
    
    if (received_index != piece_index) {
        printf("✗ Wrong piece received\n");
        close(sock);
        return -1;
    }
    
    // Read the actual piece data
    int total_received = 0;
    while (total_received < data_size) {
        int bytes = read(sock, buffer + total_received, data_size - total_received);
        if (bytes <= 0) break;
        total_received += bytes;
    }
    
    *bytes_received = total_received;
    close(sock);
    
    return 0;
}

// Download file from peers
void download_file() {
    char filename[MAX_FILENAME];
    char message[1024];
    char response[1024];
    
    printf("\n--- Download File ---\n");
    printf("Enter filename to download: ");
    scanf("%s", filename);
    getchar();
    
    // Step 1: Query tracker for peers
    sprintf(message, "QUERY %s\n", filename);
    
    printf("Searching for peers...\n");
    if (connect_to_tracker(message, response) != 0) {
        printf("✗ Cannot contact tracker\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    // Check if file found
    if (strncmp(response, "ERROR", 5) == 0) {
        printf("✗ File not found on network\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    // Parse peer list: PEERS count\nip:port\nip:port\n...
    int peer_count;
    char peer_list[1024];
    strcpy(peer_list, response);
    
    char *line = strtok(peer_list, "\n");
    if (sscanf(line, "PEERS %d", &peer_count) != 1) {
        printf("✗ Invalid tracker response\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    if (peer_count == 0) {
        printf("✗ No peers have this file\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    printf("✓ Found %d peer(s)\n", peer_count);
    
    // Get first peer's IP and port
    line = strtok(NULL, "\n");
    char peer_ip[16];
    int peer_port;
    
    if (sscanf(line, "%[^:]:%d", peer_ip, &peer_port) != 2) {
        printf("✗ Cannot parse peer address\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    printf("Downloading from peer: %s:%d\n", peer_ip, peer_port);
    
    // Step 2: Get file info from the peer (we need to know how many pieces)
    // For simplicity, let's ask user how many pieces (in real implementation, peer would tell us)
    int num_pieces;
    printf("Enter number of pieces (or 1 if unknown): ");
    scanf("%d", &num_pieces);
    getchar();
    
    // Step 3: Download each piece
    printf("\nDownloading %d piece(s)...\n", num_pieces);
    
    char *piece_buffer = (char*)malloc(PIECE_SIZE);
    if (!piece_buffer) {
        printf("✗ Memory allocation failed\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    for (int i = 0; i < num_pieces; i++) {
        printf("Downloading piece %d/%d... ", i+1, num_pieces);
        fflush(stdout);
        
        int bytes_received;
        if (request_piece_from_peer(peer_ip, peer_port, filename, i, piece_buffer, &bytes_received) == 0) {
            // Save piece to downloads directory
            save_piece(filename, downloads_dir, i, piece_buffer, bytes_received);
            printf("✓ (%d bytes)\n", bytes_received);
        } else {
            printf("✗ Failed\n");
            free(piece_buffer);
            printf("\nPress Enter to continue...");
            getchar();
            return;
        }
    }
    
    free(piece_buffer);
    
    // Step 4: Assemble pieces into complete file
    printf("\nAssembling file...\n");
    char output_path[512];
    sprintf(output_path, "%s/%s", downloads_dir, filename);
    
    if (assemble_file(filename, downloads_dir, num_pieces, output_path) == 0) {
        printf("\n✓ Download complete: %s\n", output_path);
    } else {
        printf("\n✗ Failed to assemble file\n");
    }
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Handle incoming peer connection (upload pieces)
void* handle_peer_upload(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    char buffer[1024];
    int bytes_read = read(client_fd, buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        printf("[Upload] Received request: %s", buffer);
        
        // Parse: REQUEST_PIECE filename piece_index
        char filename[MAX_FILENAME];
        int piece_index;
        
        if (sscanf(buffer, "REQUEST_PIECE %s %d", filename, &piece_index) == 2) {
            printf("[Upload] Request for %s piece %d\n", filename, piece_index);
            
            // Read piece from disk
            char *piece_data = (char*)malloc(PIECE_SIZE);
            int piece_size;
            
            // Try to read the piece
            if (read_piece(filename, pieces_dir, piece_index, piece_data, &piece_size) == 0) {
                printf("[Upload] Read piece: %d bytes\n", piece_size);
                
                // Send response header: SEND_PIECE piece_index size
                char response_header[256];
                sprintf(response_header, "SEND_PIECE %d %d\n", piece_index, piece_size);
                send(client_fd, response_header, strlen(response_header), 0);
                
                printf("[Upload] Sent header: %s", response_header);
                
                // Send piece data
                int sent = send(client_fd, piece_data, piece_size, 0);
                printf("[Upload] Sent piece data: %d bytes\n", sent);
                
            } else {
                printf("[Upload] ✗ Piece not found in %s\n", pieces_dir);
                
                // Debug: List what's in pieces_dir
                char ls_cmd[512];
                sprintf(ls_cmd, "ls -la %s/", pieces_dir);
                printf("[Upload] Directory contents:\n");
                system(ls_cmd);
            }
            
            free(piece_data);
        } else {
            printf("[Upload] ✗ Cannot parse request\n");
        }
    }
    
    close(client_fd);
    return NULL;
}

// List files in shared directory
void list_shared_files() {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    printf("\n--- Shared Files ---\n");
    
    dir = opendir(shared_dir);
    if (!dir) {
        printf("✗ Cannot open shared directory\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (strstr(entry->d_name, ".piece") != NULL) {
            continue;
        }
        
        count++;
        char filepath[512];
        sprintf(filepath, "%s/%s", shared_dir, entry->d_name);
        
        long size = get_file_size(filepath);
        int pieces = calculate_num_pieces(size);
        
        printf("%d. %s (%ld bytes, %d pieces)\n", count, entry->d_name, size, pieces);
    }
    
    closedir(dir);
    
    if (count == 0) {
        printf("(No files in shared directory)\n");
    }
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Add file to shared directory and split it
void add_file_to_share() {
    char source_path[512];
    char filename[MAX_FILENAME];
    
    printf("\n--- Add File to Share ---\n");
    printf("Enter full path of file: ");
    scanf("%s", source_path);
    getchar();
    
    if (get_file_size(source_path) < 0) {
        printf("✗ File not found: %s\n", source_path);
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    char *basename_ptr = strrchr(source_path, '/');
    if (basename_ptr) {
        strcpy(filename, basename_ptr + 1);
    } else {
        strcpy(filename, source_path);
    }
    
    char dest_path[512];
    sprintf(dest_path, "%s/%s", shared_dir, filename);
    
    char copy_cmd[2048];
    sprintf(copy_cmd, "cp %s %s", source_path, dest_path);
    system(copy_cmd);
    
    printf("✓ File copied to shared directory\n");
    
    printf("\nSplitting file into pieces...\n");
    int num_pieces = split_file(dest_path, pieces_dir);
    
    if (num_pieces > 0) {
        printf("\n✓ File ready to share: %s (%d pieces)\n", filename, num_pieces);
    }
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Register file with tracker
void register_file() {
    char filename[MAX_FILENAME];
    char message[1024];
    char response[1024];
    
    printf("\n--- Register File ---\n");
    printf("Enter filename to register: ");
    scanf("%s", filename);
    getchar();
    
    char filepath[512];
    sprintf(filepath, "%s/%s", shared_dir, filename);
    
    if (get_file_size(filepath) < 0) {
        printf("✗ File not found in shared directory\n");
        printf("Tip: Use 'Add file' option first\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    sprintf(message, "REGISTER %s %d\n", filename, my_port);
    
    printf("Registering with tracker...\n");
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

// Query for a file
void query_file() {
    char filename[MAX_FILENAME];
    char message[1024];
    char response[1024];
    
    printf("\n--- Query File ---\n");
    printf("Enter filename to search: ");
    scanf("%s", filename);
    getchar();
    
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

// Listener thread
void* listener_thread(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in address, client_addr;
    int addrlen = sizeof(address);
    socklen_t client_len = sizeof(client_addr);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        printf("✗ Listener socket creation failed\n");
        return NULL;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(my_port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("✗ Listener bind failed on port %d\n", my_port);
        close(server_fd);
        return NULL;
    }
    
    if (listen(server_fd, 3) < 0) {
        printf("✗ Listener listen failed\n");
        close(server_fd);
        return NULL;
    }
    
    printf("✓ Listener started on port %d\n", my_port);
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            continue;
        }
        
        // Create new thread to handle upload
        pthread_t upload_tid;
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        pthread_create(&upload_tid, NULL, handle_peer_upload, client_fd_ptr);
        pthread_detach(upload_tid);
    }
    
    close(server_fd);
    return NULL;
}

// Show menu
void show_menu() {
    clear_screen();
    printf("========================================\n");
    printf("       P2P File Transfer - Peer\n");
    printf("========================================\n");
    printf("Your IP: %s\n", my_ip);
    printf("Your Port: %d\n", my_port);
    printf("Shared Directory: %s\n", shared_dir);
    printf("========================================\n\n");
    
    printf("MENU:\n");
    printf("1. Add file to share\n");
    printf("2. List shared files\n");
    printf("3. Register file with tracker\n");
    printf("4. Query for a file\n");
    printf("5. Download a file\n");
    printf("6. Exit\n");
    printf("\nEnter choice: ");
}

int main(int argc, char *argv[]) {
    pthread_t listener_tid;
    int choice;
    
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        printf("Example: %s 9000\n", argv[0]);
        exit(1);
    }
    
    my_port = atoi(argv[1]);
    
    // Create directories
    sprintf(shared_dir, "peer_%d_shared", my_port);
    sprintf(pieces_dir, "peer_%d_pieces", my_port);
    sprintf(downloads_dir, "peer_%d_downloads", my_port);
    
    char mkdir_cmd[1024];
    sprintf(mkdir_cmd, "mkdir -p %s %s %s", shared_dir, pieces_dir, downloads_dir);
    system(mkdir_cmd);
    
    printf("========================================\n");
    printf("   Starting P2P Peer on port %d\n", my_port);
    printf("========================================\n\n");
    
    printf("Starting listener thread...\n");
    if (pthread_create(&listener_tid, NULL, listener_thread, NULL) != 0) {
        printf("✗ Failed to create listener thread\n");
        exit(1);
    }
    
    sleep(1);
    
    while (1) {
        show_menu();
        
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }
        getchar();
        
        switch(choice) {
            case 1:
                add_file_to_share();
                break;
            case 2:
                list_shared_files();
                break;
            case 3:
                register_file();
                break;
            case 4:
                query_file();
                break;
            case 5:
                download_file();
                break;
            case 6:
                printf("\n✓ Exiting...\n");
                exit(0);
            default:
                printf("\n✗ Invalid choice!\n");
                sleep(1);
        }
    }
    
    return 0;
}