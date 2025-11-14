#include <stdio.h>
#include <string.h>
#include <time.h>
#include "progress_bar.h"

void init_progress(ProgressTracker *tracker, int total_pieces, long total_bytes) {
    tracker->total_pieces = total_pieces;
    tracker->downloaded_pieces = 0;
    tracker->total_bytes = total_bytes;
    tracker->downloaded_bytes = 0;
    tracker->start_time = time(NULL);
    tracker->last_update = time(NULL);
}

void update_progress(ProgressTracker *tracker, int piece_size) {
    tracker->downloaded_pieces++;
    tracker->downloaded_bytes += piece_size;
    tracker->last_update = time(NULL);
}

double get_speed_mbps(ProgressTracker *tracker) {
    time_t elapsed = time(NULL) - tracker->start_time;
    if (elapsed == 0) return 0.0;
    
    double bytes_per_sec = (double)tracker->downloaded_bytes / elapsed;
    return bytes_per_sec / (1024.0 * 1024.0); // Convert to MB/s
}

int get_eta_seconds(ProgressTracker *tracker) {
    double speed = get_speed_mbps(tracker);
    if (speed == 0) return 0;
    
    long remaining_bytes = tracker->total_bytes - tracker->downloaded_bytes;
    double remaining_mb = remaining_bytes / (1024.0 * 1024.0);
    
    return (int)(remaining_mb / speed);
}

void display_progress(ProgressTracker *tracker) {
    int bar_width = 40;
    float percentage = (float)tracker->downloaded_pieces / tracker->total_pieces;
    int filled = (int)(percentage * bar_width);
    
    // Clear line and move cursor to beginning
    printf("\r");
    
    // Progress bar
    printf("[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("█");
        } else {
            printf("░");
        }
    }
    printf("] ");
    
    // Percentage
    printf("%.1f%% ", percentage * 100);
    
    // Pieces
    printf("(%d/%d) ", tracker->downloaded_pieces, tracker->total_pieces);
    
    // Speed
    double speed = get_speed_mbps(tracker);
    printf("%.2f MB/s ", speed);
    
    // ETA
    int eta = get_eta_seconds(tracker);
    if (eta > 0) {
        int minutes = eta / 60;
        int seconds = eta % 60;
        printf("ETA: %dm %ds", minutes, seconds);
    }
    
    fflush(stdout);
}