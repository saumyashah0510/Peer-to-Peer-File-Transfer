#ifndef PROTOCOL_H
#define PROTOCOL_H

// Every file will be split into chunks of 256,000 bytes (256 KB)
#define PIECE_SIZE 256000

//Tracker server will listen on port 8080 (common for testing)
#define TRACKER_PORT 8080

// Filename can be maximum 100 characters
#define MAX_FILENAME 100

#endif