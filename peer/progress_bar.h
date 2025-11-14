#ifndef PROGRESS_BAR_H
#define PROGRESS_BAR_H

#include <time.h>

typedef struct {
    int total_pieces;
    int downloaded_pieces;
    long total_bytes;
    long downloaded_bytes;
    time_t start_time;
    time_t last_update;
} ProgressTracker;

// Initialize progress tracker
void init_progress(ProgressTracker *tracker, int total_pieces, long total_bytes);

// Update progress (call after each piece)
void update_progress(ProgressTracker *tracker, int piece_size);

// Display progress bar
void display_progress(ProgressTracker *tracker);

// Calculate download speed
double get_speed_mbps(ProgressTracker *tracker);

// Calculate ETA
int get_eta_seconds(ProgressTracker *tracker);

#endif