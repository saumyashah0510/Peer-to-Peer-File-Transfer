#include <stdio.h>
#include "protocol.h"
#include "file_ops.h"

int main() {
    char *filename = "bigfile.dat";
    
    printf("========================================\n");
    printf("      Testing File Operations\n");
    printf("========================================\n\n");
    
    // Test 1: Get file size
    long size = get_file_size(filename);
    printf("File size: %ld bytes\n", size);
    
    // Test 2: Calculate pieces
    int num_pieces = calculate_num_pieces(size);
    printf("Number of pieces: %d\n\n", num_pieces);
    
    // Test 3: Split file
    printf("--- Splitting file ---\n");
    split_file(filename, "pieces");
    
    printf("\n--- Assembling file ---\n");
    // Test 4: Assemble file
    assemble_file(filename, "pieces", num_pieces, "testfile_copy.txt");
    
    printf("\n========================================\n");
    printf("Done! Check if files are identical:\n");
    printf("  diff testfile.txt testfile_copy.txt\n");
    printf("========================================\n");
    
    return 0;
}