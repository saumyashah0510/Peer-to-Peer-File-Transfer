#ifndef FILE_OPS_H
#define FILE_OPS_H

// Get file size
long get_file_size(char *filename);

// Calculate number of pieces needed
int calculate_num_pieces(long file_size);

// Split file into pieces
int split_file(char *filename, char *output_dir);

// Assemble pieces into complete file
int assemble_file(char *filename, char *pieces_dir, int num_pieces, char *output_filename);

// Read a specific piece
int read_piece(char *filename, char *pieces_dir, int piece_index, char *buffer, int *bytes_read);

// Save a piece
int save_piece(char *filename, char *pieces_dir, int piece_index, char *data, int data_size);

#endif