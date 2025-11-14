#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "protocol.h"

// Get file size in bytes
long get_file_size(char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// Calculate how many pieces a file needs
int calculate_num_pieces(long file_size) {
    return (file_size + PIECE_SIZE - 1) / PIECE_SIZE;
}

// Split a file into pieces and save them
// Split a file into pieces and save them
int split_file(char *filepath, char *output_dir) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        printf("✗ Cannot open file: %s\n", filepath);
        return -1;
    }
    
    // Get file size
    long file_size = get_file_size(filepath);
    if (file_size < 0) {
        printf("✗ Cannot get file size\n");
        fclose(file);
        return -1;
    }
    
    // Extract just the filename from full path
    char *filename = strrchr(filepath, '/');
    if (filename) {
        filename++;  // Skip the '/'
    } else {
        filename = filepath;  // No '/' found, use whole string
    }
    
    // Calculate number of pieces
    int num_pieces = calculate_num_pieces(file_size);
    
    printf("File size: %ld bytes\n", file_size);
    printf("Number of pieces: %d\n", num_pieces);
    printf("Piece size: %d bytes\n", PIECE_SIZE);
    
    // Create output directory if it doesn't exist
    char mkdir_cmd[512];
    sprintf(mkdir_cmd, "mkdir -p %s", output_dir);
    system(mkdir_cmd);
    
    // Buffer to hold one piece
    char *buffer = (char*)malloc(PIECE_SIZE);
    if (!buffer) {
        printf("✗ Memory allocation failed\n");
        fclose(file);
        return -1;
    }
    
    // Split file into pieces
    for (int i = 0; i < num_pieces; i++) {
        // Read one piece from file
        size_t bytes_read = fread(buffer, 1, PIECE_SIZE, file);
        
        if (bytes_read == 0) {
            break;
        }
        
        // Create piece filename: output_dir/filename.piece0, filename.piece1, etc.
        char piece_filename[512];
        sprintf(piece_filename, "%s/%s.piece%d", output_dir, filename, i);
        
        // Write piece to file
        FILE *piece_file = fopen(piece_filename, "wb");
        if (!piece_file) {
            printf("✗ Cannot create piece file: %s\n", piece_filename);
            continue;
        }
        
        fwrite(buffer, 1, bytes_read, piece_file);
        fclose(piece_file);
        
        printf("✓ Created piece %d (%zu bytes)\n", i, bytes_read);
    }
    
    free(buffer);
    fclose(file);
    
    printf("✓ File split complete!\n");
    return num_pieces;
}

// Assemble pieces back into original file
int assemble_file(char *filename, char *pieces_dir, int num_pieces, char *output_filename) {
    FILE *output_file = fopen(output_filename, "wb");
    if (!output_file) {
        printf("✗ Cannot create output file: %s\n", output_filename);
        return -1;
    }
    
    char *buffer = (char*)malloc(PIECE_SIZE);
    if (!buffer) {
        printf("✗ Memory allocation failed\n");
        fclose(output_file);
        return -1;
    }
    
    printf("Assembling %d pieces...\n", num_pieces);
    
    // Read each piece and write to output file
    for (int i = 0; i < num_pieces; i++) {
        // Build piece filename
        char piece_filename[512];
        sprintf(piece_filename, "%s/%s.piece%d", pieces_dir, filename, i);
        
        // Open piece file
        FILE *piece_file = fopen(piece_filename, "rb");
        if (!piece_file) {
            printf("✗ Missing piece %d\n", i);
            free(buffer);
            fclose(output_file);
            return -1;
        }
        
        // Read piece
        size_t bytes_read = fread(buffer, 1, PIECE_SIZE, piece_file);
        
        // Write to output file
        fwrite(buffer, 1, bytes_read, output_file);
        fclose(piece_file);
        
        printf("✓ Assembled piece %d (%zu bytes)\n", i, bytes_read);
    }
    
    free(buffer);
    fclose(output_file);
    
    printf("✓ File assembled: %s\n", output_filename);
    return 0;
}

// Read a specific piece from disk
int read_piece(char *filename, char *pieces_dir, int piece_index, char *buffer, int *bytes_read) {
    char piece_filename[512];
    sprintf(piece_filename, "%s/%s.piece%d", pieces_dir, filename, piece_index);
    
    FILE *piece_file = fopen(piece_filename, "rb");
    if (!piece_file) {
        printf("✗ Cannot open piece %d\n", piece_index);
        return -1;
    }
    
    *bytes_read = fread(buffer, 1, PIECE_SIZE, piece_file);
    fclose(piece_file);
    
    return 0;
}

// Save a piece to disk
int save_piece(char *filename, char *pieces_dir, int piece_index, char *data, int data_size) {
    // Create directory if doesn't exist
    char mkdir_cmd[512];
    sprintf(mkdir_cmd, "mkdir -p %s", pieces_dir);
    system(mkdir_cmd);
    
    char piece_filename[512];
    sprintf(piece_filename, "%s/%s.piece%d", pieces_dir, filename, piece_index);
    
    FILE *piece_file = fopen(piece_filename, "wb");
    if (!piece_file) {
        printf("✗ Cannot save piece %d\n", piece_index);
        return -1;
    }
    
    fwrite(data, 1, data_size, piece_file);
    fclose(piece_file);
    
    return 0;
}