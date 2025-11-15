#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"
#include "../file_ops.h"
#include "network_utils.h"
#include "progress_bar.h"
#include "multi_source.h"

// Global variables
int my_port;
char my_ip[16];
char tracker_ip[16];
char base_dir[256] = "p2p_data";

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
    tracker_addr.sin_addr.s_addr = inet_addr(tracker_ip);
    
    if (connect(sock, (struct sockaddr *)&tracker_addr, sizeof(tracker_addr)) < 0) {
        printf("✗ Cannot connect to tracker at %s:%d\n", tracker_ip, TRACKER_PORT);
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

// Get file info from peer
int get_file_info_from_peer(char *peer_ip, int peer_port, char *filename, int *num_pieces, long *file_size) {
    int sock;
    struct sockaddr_in peer_addr;
    char request[512];
    char response[1024];
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip);
    
    if (connect(sock, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
        close(sock);
        return -1;
    }
    
    sprintf(request, "FILE_INFO %s\n", filename);
    send(sock, request, strlen(request), 0);
    
    int bytes = read(sock, response, sizeof(response));
    if (bytes > 0) {
        response[bytes] = '\0';
        sscanf(response, "INFO %d %ld", num_pieces, file_size);
    }
    
    close(sock);
    return 0;
}

// Request piece from peer
int request_piece_from_peer(char *peer_ip, int peer_port, char *filename, int piece_index, char *buffer, int *bytes_received) {
    int sock;
    struct sockaddr_in peer_addr;
    char request[512];
    char response_line[256];
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    peer_addr.sin_addr.s_addr = inet_addr(peer_ip);
    
    if (connect(sock, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
        close(sock);
        return -1;
    }
    
    sprintf(request, "REQUEST_PIECE %s %d\n", filename, piece_index);
    send(sock, request, strlen(request), 0);
    
    // Read header line by line
    int pos = 0;
    char ch;
    while (pos < 255) {
        int n = read(sock, &ch, 1);
        if (n <= 0) break;
        response_line[pos++] = ch;
        if (ch == '\n') break;
    }
    response_line[pos] = '\0';
    
    int received_index, data_size;
    if (sscanf(response_line, "SEND_PIECE %d %d", &received_index, &data_size) != 2) {
        close(sock);
        return -1;
    }
    
    if (received_index != piece_index) {
        close(sock);
        return -1;
    }
    
    // Read piece data
    int total_received = 0;
    while (total_received < data_size) {
        int bytes = read(sock, buffer + total_received, data_size - total_received);
        if (bytes <= 0) break;
        total_received += bytes;
    }
    
    *bytes_received = total_received;
    close(sock);
    
    return (total_received == data_size) ? 0 : -1;
}

// Download worker thread
typedef struct {
    DownloadContext *ctx;
    PeerConnection *peer;
    int thread_id;
} DownloadWorkerArgs;

void* download_worker(void *arg) {
    /*✔ A small helper
✔ Assigned to one peer
✔ Downloads pieces one-by-one
✔ Saves them
✔ Updates progress
✔ Retries failed pieces
✔ Stops when everything is done*/
    DownloadWorkerArgs *args = (DownloadWorkerArgs*)arg;
    DownloadContext *ctx = args->ctx;
    PeerConnection *peer = args->peer;
    int thread_id = args->thread_id;
    
    // Find 2hich peer number is this
    int peer_index = -1;
    for (int i = 0; i < ctx->peer_count; i++) {
        if (strcmp(ctx->peers[i].ip, peer->ip) == 0 && 
            ctx->peers[i].port == peer->port) {//compare ip and port
            peer_index = i;
            break;
        }
    }
    
    char *piece_buffer = (char*)malloc(PIECE_SIZE);//allocate memeory to store one piece
    if (!piece_buffer) {
        free(arg);
        return NULL;
    }
    /*he thread keeps downloading while:

The full file is NOT completed

The download has NOT been marked as failed*/
    while (!is_download_complete(ctx) && !ctx->failed) {
        // Get next piece to download
        int piece_index = get_next_piece(ctx);//this chooses a piece which  is not downloaded yet
        
        if (piece_index == -1) {
            // No more pieces to download
            break;
        }
        
        // Download piece from peer
        int bytes_received;
        if (request_piece_from_peer(peer->ip, peer->port, ctx->filename, 
                                     piece_index, piece_buffer, &bytes_received) == 0) {
            // Success - save piece
            save_piece(ctx->filename, ctx->downloads_dir, piece_index, 
                      piece_buffer, bytes_received);
            
            mark_piece_completed(ctx, piece_index, peer_index, bytes_received);
            
            // Update progress
            pthread_mutex_lock(&ctx->status_mutex);
            update_progress(ctx->progress, bytes_received);
            display_progress(ctx->progress);
            
            // Show which peer just contributed (color coded!)
            printf(" [P%d]", peer_index + 1);
            fflush(stdout);
            
            pthread_mutex_unlock(&ctx->status_mutex);
            
        } else {
            // Failed - mark for retry
            mark_piece_failed(ctx, piece_index);
        }
    }
    
    free(piece_buffer);
    free(arg);
    return NULL;
}

// Download file with multi-source support and per-peer stats
void download_file() {
    char filename[MAX_FILENAME];
    char message[1024];
    char response[1024];
    
    printf("\n--- Download File (Multi-Source) ---\n");
    printf("Enter filename to download: ");
    scanf("%s", filename);
    getchar();
    
    // Query tracker for peers
    sprintf(message, "QUERY %s\n", filename);
    
    printf("Searching for peers...\n");
    if (connect_to_tracker(message, response) != 0) {
        printf("✗ Cannot contact tracker\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    if (strncmp(response, "ERROR", 5) == 0) {
        printf("✗ File not found\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    // Parse peer list
    int peer_count;
    char peer_list[2048];
    strcpy(peer_list, response);
    
    char *line = strtok(peer_list, "\n");
    sscanf(line, "PEERS %d", &peer_count);
    
    printf("✓ Found %d peer(s)\n", peer_count);
    
    if (peer_count == 0) {
        printf("✗ No peers available\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    // Get file info from first peer
    line = strtok(NULL, "\n");
    char first_peer_ip[16];
    int first_peer_port;
    sscanf(line, "%[^:]:%d", first_peer_ip, &first_peer_port);
    
    int num_pieces;
    long file_size;
    
    printf("Getting file information from %s:%d...\n", first_peer_ip, first_peer_port);
    if (get_file_info_from_peer(first_peer_ip, first_peer_port, filename, &num_pieces, &file_size) != 0) {
        printf("✗ Cannot get file info\n");
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }
    
    printf("\n");
    printf("========================================\n");
    printf("File: %s\n", filename);
    printf("Size: %.2f MB (%ld bytes)\n", file_size / (1024.0 * 1024.0), file_size);
    printf("Pieces: %d\n", num_pieces);
    printf("Peers: %d\n", peer_count);
    printf("========================================\n\n");
    
    // Initialize download context
    char temp_dir[512];
    sprintf(temp_dir, "%s/temp_download", base_dir);
    
    DownloadContext ctx;
    init_download_context(&ctx, filename, num_pieces, file_size, temp_dir);
    
    // Add all peers to context
    strcpy(peer_list, response);  // Reset peer_list
    line = strtok(peer_list, "\n");  // Skip "PEERS X"
    line = strtok(NULL, "\n");  // First peer
    
    while (line != NULL && ctx.peer_count < MAX_PEERS) {
        char ip[16];
        int port;
        if (sscanf(line, "%[^:]:%d", ip, &port) == 2) {
            add_peer_to_context(&ctx, ip, port);
            printf("Added peer %d: %s:%d\n", ctx.peer_count, ip, port);
        }
        line = strtok(NULL, "\n");
    }
    
    printf("\nStarting multi-source download from %d peer(s)...\n", ctx.peer_count);
    printf("Watch for [P1], [P2], [P3]... indicators showing which peer is contributing!\n\n");
    sleep(1);
    
    // Create worker threads
    pthread_t workers[MAX_CONCURRENT_DOWNLOADS];
    int num_workers = (ctx.peer_count < MAX_CONCURRENT_DOWNLOADS) ? 
                      ctx.peer_count : MAX_CONCURRENT_DOWNLOADS;
    
    printf("Spawning %d download threads...\n\n", num_workers);
    
    for (int i = 0; i < num_workers; i++) {
        DownloadWorkerArgs *args = (DownloadWorkerArgs*)malloc(sizeof(DownloadWorkerArgs));
        args->ctx = &ctx;
        args->peer = &ctx.peers[i % ctx.peer_count];
        args->thread_id = i;
        
        pthread_create(&workers[i], NULL, download_worker, args);
    }
    
    // Wait for all workers to finish
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }
    
    printf("\n\n");
    
    // Display per-peer statistics
    display_peer_stats(&ctx);
    
    // Assemble file
    if (is_download_complete(&ctx)) {
        printf("\nAssembling file...\n");
        char output_path[512];
        sprintf(output_path, "%s/downloads/%s", base_dir, filename);
        
        if (assemble_file(filename, temp_dir, num_pieces, output_path) == 0) {
            printf("\n========================================\n");
            printf("✓ Download Complete!\n");
            printf("========================================\n");
            printf("File: %s\n", output_path);
            printf("Size: %.2f MB\n", file_size / (1024.0 * 1024.0));
            printf("Average Speed: %.2f MB/s\n", get_speed_mbps(ctx.progress));
            printf("Time Taken: %ld seconds\n", time(NULL) - ctx.progress->start_time);
            printf("========================================\n");
            
            // Cleanup temp pieces
            char cleanup_cmd[1024];
            sprintf(cleanup_cmd, "rm -f %s/%s.piece*", temp_dir, filename);
            system(cleanup_cmd);
        }
    } else {
        printf("\n✗ Download incomplete or failed\n");
    }
    
    cleanup_download_context(&ctx);
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Handle peer upload
void* handle_peer_upload(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    char buffer[1024];
    int bytes_read = read(client_fd, buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        if (strncmp(buffer, "FILE_INFO", 9) == 0) {
            char filename[MAX_FILENAME];
            sscanf(buffer, "FILE_INFO %s", filename);
            
            printf("[Info] Request for file info: %s\n", filename);
            
            char filepath[512];
            sprintf(filepath, "%s/shared/%s", base_dir, filename);
            long file_size = get_file_size(filepath);
            int num_pieces = calculate_num_pieces(file_size);
            
            char response[256];
            sprintf(response, "INFO %d %ld\n", num_pieces, file_size);
            send(client_fd, response, strlen(response), 0);
            
            printf("[Info] Sent: %d pieces, %ld bytes\n", num_pieces, file_size);
        }
        else if (strncmp(buffer, "REQUEST_PIECE", 13) == 0) {
            char filename[MAX_FILENAME];
            int piece_index;
            
            if (sscanf(buffer, "REQUEST_PIECE %s %d", filename, &piece_index) == 2) {
                printf("[Upload] Request for %s piece %d\n", filename, piece_index);
                
                char *piece_data = (char*)malloc(PIECE_SIZE);
                int piece_size;
                
                char pieces_dir[512];
                sprintf(pieces_dir, "%s/pieces", base_dir);
                
                if (read_piece(filename, pieces_dir, piece_index, piece_data, &piece_size) == 0) {
                    char response_header[256];
                    sprintf(response_header, "SEND_PIECE %d %d\n", piece_index, piece_size);
                    send(client_fd, response_header, strlen(response_header), 0);
                    send(client_fd, piece_data, piece_size, 0);
                    
                    printf("[Upload] Sent piece %d (%d bytes)\n", piece_index, piece_size);
                } else {
                    printf("[Upload] ✗ Piece not found\n");
                }
                
                free(piece_data);
            }
        }
    }
    
    close(client_fd);
    return NULL;
}

// List shared files
void list_shared_files() {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    printf("\n--- Shared Files ---\n");
    
    char shared_dir[512];
    sprintf(shared_dir, "%s/shared", base_dir);
    
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
        char filepath[1024];
        sprintf(filepath, "%s/%s", shared_dir, entry->d_name);
        
        long size = get_file_size(filepath);
        int pieces = calculate_num_pieces(size);
        
        printf("%d. %s (%.2f MB, %d pieces)\n", count, entry->d_name, 
               size / (1024.0 * 1024.0), pieces);
    }
    
    closedir(dir);
    
    if (count == 0) {
        printf("(No files in shared directory)\n");
    }
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Add file to share
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
    sprintf(dest_path, "%s/shared/%s", base_dir, filename);
    
    char copy_cmd[2048];
    sprintf(copy_cmd, "cp %s %s", source_path, dest_path);
    system(copy_cmd);
    
    printf("✓ File copied to shared directory\n");
    
    printf("\nSplitting file into pieces...\n");
    
    char pieces_dir[512];
    sprintf(pieces_dir, "%s/pieces", base_dir);
    
    int num_pieces = split_file(dest_path, pieces_dir);
    
    if (num_pieces > 0) {
        printf("\n✓ File ready to share: %s (%d pieces)\n", filename, num_pieces);
    }
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Register file
void register_file() {
    char filename[MAX_FILENAME];
    char message[1024];
    char response[1024];
    
    printf("\n--- Register File ---\n");
    printf("Enter filename to register: ");
    scanf("%s", filename);
    getchar();
    
    char filepath[512];
    sprintf(filepath, "%s/shared/%s", base_dir, filename);
    
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

// Query file
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
    
    printf("✓ Listener started on %s:%d\n", my_ip, my_port);
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            continue;
        }
        
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
    printf("   P2P File Transfer - Peer v5.0\n");
    printf("   (Multi-Source + Per-Peer Stats)\n");
    printf("========================================\n");
    printf("Your IP: %s\n", my_ip);
    printf("Your Port: %d\n", my_port);
    printf("Tracker: %s:%d\n", tracker_ip, TRACKER_PORT);
    printf("Data Directory: %s/\n", base_dir);
    printf("========================================\n\n");
    
    printf("MENU:\n");
    printf("1. Add file to share\n");
    printf("2. List shared files\n");
    printf("3. Register file with tracker\n");
    printf("4. Query for a file\n");
    printf("5. Download a file (Multi-Source + Stats)\n");
    printf("6. Exit\n");
    printf("\nEnter choice: ");
}

int main(int argc, char *argv[]) {
    pthread_t listener_tid;
    int choice;
    
    if (argc != 3) {
        printf("Usage: %s <my_port> <tracker_ip>\n", argv[0]);
        printf("Example: %s 9000 192.168.1.100\n", argv[0]);
        exit(1);
    }
    
    my_port = atoi(argv[1]);
    strcpy(tracker_ip, argv[2]);
    
    // Auto-detect my real IP
    if (get_my_ip(my_ip) != 0) {
        printf("⚠ Warning: Could not detect real IP, using 127.0.0.1\n");
    }
    
    // Create directory structure
    char mkdir_cmd[2048];
    sprintf(mkdir_cmd, "mkdir -p %s/shared %s/pieces %s/downloads %s/temp_download", 
            base_dir, base_dir, base_dir, base_dir);
    system(mkdir_cmd);
    
    printf("========================================\n");
    printf("   Starting P2P Peer v5.0\n");
    printf("========================================\n");
    printf("My IP: %s\n", my_ip);
    printf("My Port: %d\n", my_port);
    printf("Tracker: %s:%d\n", tracker_ip, TRACKER_PORT);
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
