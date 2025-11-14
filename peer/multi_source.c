#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "multi_source.h"

void init_download_context(DownloadContext *ctx, char *filename, int num_pieces, 
                           long file_size, char *downloads_dir) {
    strcpy(ctx->filename, filename);
    ctx->num_pieces = num_pieces;
    ctx->file_size = file_size;
    ctx->peer_count = 0;
    ctx->failed = 0;
    
    strcpy(ctx->downloads_dir, downloads_dir);
    
    // Allocate piece status array
    ctx->piece_status = (int*)calloc(num_pieces, sizeof(int));
    ctx->piece_source = (int*)calloc(num_pieces, sizeof(int));
    
    // Initialize mutex
    pthread_mutex_init(&ctx->status_mutex, NULL);
    
    // Initialize progress tracker
    ctx->progress = (ProgressTracker*)malloc(sizeof(ProgressTracker));
    init_progress(ctx->progress, num_pieces, file_size);
}

void add_peer_to_context(DownloadContext *ctx, char *ip, int port) {
    if (ctx->peer_count >= MAX_PEERS) return;
    
    strcpy(ctx->peers[ctx->peer_count].ip, ip);
    ctx->peers[ctx->peer_count].port = port;
    ctx->peers[ctx->peer_count].active_downloads = 0;
    ctx->peers[ctx->peer_count].pieces_downloaded = 0;
    ctx->peers[ctx->peer_count].bytes_downloaded = 0;
    ctx->peers[ctx->peer_count].start_time = time(NULL);
    ctx->peers[ctx->peer_count].last_download_time = 0;
    ctx->peer_count++;
}

int get_next_piece(DownloadContext *ctx) {
    pthread_mutex_lock(&ctx->status_mutex);
    
    int piece = -1;
    
    // Find first piece that's not downloaded or downloading
    for (int i = 0; i < ctx->num_pieces; i++) {
        if (ctx->piece_status[i] == 0) {
            ctx->piece_status[i] = 1;  // Mark as downloading
            piece = i;
            break;
        }
    }
    
    pthread_mutex_unlock(&ctx->status_mutex);
    return piece;
}

void mark_piece_completed(DownloadContext *ctx, int piece_index, int peer_index, int bytes) {
    pthread_mutex_lock(&ctx->status_mutex);
    
    ctx->piece_status[piece_index] = 2;  // Mark as completed
    ctx->piece_source[piece_index] = peer_index;  // Record which peer
    
    // Update peer statistics
    ctx->peers[peer_index].pieces_downloaded++;
    ctx->peers[peer_index].bytes_downloaded += bytes;
    ctx->peers[peer_index].last_download_time = time(NULL);
    
    pthread_mutex_unlock(&ctx->status_mutex);
}

void mark_piece_failed(DownloadContext *ctx, int piece_index) {
    pthread_mutex_lock(&ctx->status_mutex);
    
    ctx->piece_status[piece_index] = 0;  // Mark as not downloaded (retry later)
    
    pthread_mutex_unlock(&ctx->status_mutex);
}

int is_download_complete(DownloadContext *ctx) {
    pthread_mutex_lock(&ctx->status_mutex);
    
    int complete = 1;
    for (int i = 0; i < ctx->num_pieces; i++) {
        if (ctx->piece_status[i] != 2) {
            complete = 0;
            break;
        }
    }
    
    pthread_mutex_unlock(&ctx->status_mutex);
    return complete;
}

void display_peer_stats(DownloadContext *ctx) {
    printf("\n");
    printf("========================================\n");
    printf("      Per-Peer Download Statistics\n");
    printf("========================================\n");
    
    for (int i = 0; i < ctx->peer_count; i++) {
        PeerConnection *peer = &ctx->peers[i];
        
        double mb_downloaded = peer->bytes_downloaded / (1024.0 * 1024.0);
        double percentage = (peer->bytes_downloaded * 100.0) / ctx->file_size;
        
        time_t elapsed = time(NULL) - peer->start_time;
        double speed_mbps = 0.0;
        if (elapsed > 0) {
            speed_mbps = mb_downloaded / elapsed;
        }
        
        printf("\nPeer %d: %s:%d\n", i+1, peer->ip, peer->port);
        printf("├─ Pieces: %d/%d (%.1f%%)\n", 
               peer->pieces_downloaded, ctx->num_pieces,
               (peer->pieces_downloaded * 100.0) / ctx->num_pieces);
        printf("├─ Data: %.2f MB (%.1f%% of total)\n", mb_downloaded, percentage);
        printf("├─ Avg Speed: %.2f MB/s\n", speed_mbps);
        
        // Visual bar for this peer's contribution
        int bar_width = 30;
        int filled = (int)(percentage * bar_width / 100.0);
        printf("└─ [");
        for (int j = 0; j < bar_width; j++) {
            if (j < filled) printf("█");
            else printf("░");
        }
        printf("]\n");
    }
    
    printf("\n========================================\n");
}

void cleanup_download_context(DownloadContext *ctx) {
    free(ctx->piece_status);
    free(ctx->piece_source);
    pthread_mutex_destroy(&ctx->status_mutex);
    free(ctx->progress);
}