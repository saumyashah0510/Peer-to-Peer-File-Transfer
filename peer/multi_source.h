#ifndef MULTI_SOURCE_H
#define MULTI_SOURCE_H

#include <pthread.h>
#include <time.h>
#include "progress_bar.h"

#define MAX_PEERS 10
#define MAX_CONCURRENT_DOWNLOADS 3

typedef struct {
    char ip[16];
    int port;
    int active_downloads;
    
    // Statistics
    int pieces_downloaded;
    long bytes_downloaded;
    time_t start_time;
    time_t last_download_time;
} PeerConnection;

typedef struct {
    char filename[256];
    int num_pieces;
    long file_size;
    
    PeerConnection peers[MAX_PEERS];
    int peer_count;
    
    int *piece_status;  // 0=not downloaded, 1=downloading, 2=completed
    int *piece_source;  // Which peer downloaded this piece (peer index)
    pthread_mutex_t status_mutex;
    
    ProgressTracker *progress;
    
    char downloads_dir[512];
    int failed;
} DownloadContext;

// Initialize download context
void init_download_context(DownloadContext *ctx, char *filename, int num_pieces, 
                           long file_size, char *downloads_dir);

// Add peer to download context
void add_peer_to_context(DownloadContext *ctx, char *ip, int port);

// Get next piece to download (thread-safe)
int get_next_piece(DownloadContext *ctx);

// Mark piece as completed (thread-safe) - UPDATED SIGNATURE
void mark_piece_completed(DownloadContext *ctx, int piece_index, int peer_index, int bytes);

// Mark piece as failed (thread-safe)
void mark_piece_failed(DownloadContext *ctx, int piece_index);

// Check if download is complete
int is_download_complete(DownloadContext *ctx);

// Display per-peer statistics
void display_peer_stats(DownloadContext *ctx);

// Cleanup
void cleanup_download_context(DownloadContext *ctx);

#endif