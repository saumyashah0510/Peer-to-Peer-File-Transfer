# P2P File Transfer System - Complete Function Summary

## 1️⃣ protocol.h - Constants & Definitions

### Constants
- **`PIECE_SIZE`** = 256,000 bytes - Size of each file chunk
- **`TRACKER_PORT`** = 8080 - Port where tracker listens
- **`MAX_FILENAME`** = 100 - Maximum filename length
- **`MSG_FILE_INFO`** = "FILE_INFO" - Protocol message

**Purpose**: Central location for all shared constants across the project.

---

## 2️⃣ tracker.c - Tracker Server

### Data Structures
- **`FileRecord`** - Stores info about one registered file
  - `filename` - Name of the file
  - `peer_ip` - IP address of peer who has it
  - `peer_port` - Port of that peer

### Global Variables
- **`registered_files[10]`** - Array storing up to 10 file records
- **`file_count`** - Number of currently registered files

### Functions

| Function | Purpose |
|----------|---------|
| **`add_file()`** | Add a file record to memory database |
| **`find_peers()`** | Search for peers who have a specific file |
| **`print_all_files()`** | Display all registered files (debugging) |
| **`main()`** | Server loop - handles REGISTER and QUERY commands |

**Key Operations**:
- **REGISTER**: Stores filename with peer's REAL IP (from socket, not message)
- **QUERY**: Returns list of peers who have the file

---

## 3️⃣ file_ops.c/h - File Manipulation

### Functions

| Function | Parameters | Returns | Purpose |
|----------|-----------|---------|---------|
| **`get_file_size()`** | filename | file size (bytes) | Get size of a file using `stat()` |
| **`calculate_num_pieces()`** | file_size | number of pieces | Calculate how many pieces needed (ceiling division) |
| **`split_file()`** | filepath, output_dir | number of pieces | Split file into 256KB pieces |
| **`assemble_file()`** | filename, pieces_dir, num_pieces, output_path | 0=success, -1=error | Reassemble pieces into original file |
| **`read_piece()`** | filename, pieces_dir, piece_index, buffer, bytes_read | 0=success, -1=error | Read a specific piece from disk |
| **`save_piece()`** | filename, pieces_dir, piece_index, data, data_size | 0=success, -1=error | Save downloaded piece to disk |

**Key Algorithms**:
- **Ceiling Division**: `(file_size + PIECE_SIZE - 1) / PIECE_SIZE`
- **Piece Naming**: `filename.piece0`, `filename.piece1`, etc.

---

## 4️⃣ progress_bar.c/h - Progress Display

### Data Structure
- **`ProgressTracker`**
  - `total_pieces` - Total pieces to download
  - `downloaded_pieces` - Pieces completed
  - `total_bytes` - Total file size
  - `downloaded_bytes` - Bytes downloaded so far
  - `start_time` - When download started
  - `last_update` - Last activity timestamp

### Functions

| Function | Purpose |
|----------|---------|
| **`init_progress()`** | Initialize progress tracker with totals |
| **`update_progress()`** | Increment counters after each piece |
| **`get_speed_mbps()`** | Calculate download speed (MB/s) |
| **`get_eta_seconds()`** | Estimate time remaining |
| **`display_progress()`** | Show animated progress bar with stats |

**Visual Output**:
```
[████████████████████░░░░░░░░░░░░░░░░░░░░] 50.0% (5/10) 2.50 MB/s ETA: 2m 10s
```

---

## 5️⃣ network_utils.c/h - Network Utilities

### Functions

| Function | Purpose |
|----------|---------|
| **`get_my_ip()`** | Auto-detect machine's real IP address (not 127.0.0.1) |

**Algorithm**:
1. Get list of all network interfaces using `getifaddrs()`
2. Skip localhost (127.0.0.1)
3. Skip virtual interfaces (docker, veth)
4. Return first real IPv4 address found

---

## 6️⃣ multi_source.c/h - Multi-Peer Download Coordination

### Data Structures

- **`PeerConnection`** - Info about one peer
  - `ip`, `port` - Connection info
  - `pieces_downloaded` - Statistics
  - `bytes_downloaded` - Total data from this peer
  - `start_time`, `last_download_time` - Timestamps

- **`DownloadContext`** - Manages entire download
  - `filename`, `num_pieces`, `file_size` - File info
  - `peers[]` - Array of available peers
  - `piece_status[]` - Status of each piece (0=not started, 1=downloading, 2=complete)
  - `piece_source[]` - Which peer downloaded each piece
  - `status_mutex` - Thread synchronization
  - `progress` - Progress tracker

### Functions

| Function | Purpose | Thread-Safe? |
|----------|---------|--------------|
| **`init_download_context()`** | Initialize all download state | N/A |
| **`add_peer_to_context()`** | Add a peer to the download pool | No |
| **`get_next_piece()`** | Get next available piece to download | ✅ Yes (mutex) |
| **`mark_piece_completed()`** | Mark piece as done, update stats | ✅ Yes (mutex) |
| **`mark_piece_failed()`** | Reset piece for retry | ✅ Yes (mutex) |
| **`is_download_complete()`** | Check if all pieces downloaded | ✅ Yes (mutex) |
| **`display_peer_stats()`** | Show per-peer contribution statistics | No |
| **`cleanup_download_context()`** | Free memory and destroy mutex | N/A |

**Key Concept**: Mutex ensures only ONE thread can modify `piece_status[]` at a time, preventing duplicate downloads.

---

## 7️⃣ peerv5.c - Main Peer Application

### Global Variables
- **`my_port`** - This peer's listening port
- **`my_ip`** - This peer's IP address
- **`tracker_ip`** - Tracker server's IP
- **`base_dir`** - Base directory ("p2p_data")

### Core Functions

#### **Tracker Communication**
| Function | Purpose |
|----------|---------|
| **`connect_to_tracker()`** | Send message to tracker, get response |

#### **Peer-to-Peer Communication**
| Function | Purpose |
|----------|---------|
| **`get_file_info_from_peer()`** | Ask peer for file metadata (size, pieces) |
| **`request_piece_from_peer()`** | Download one specific piece from a peer |

#### **Download System**
| Function | Purpose |
|----------|---------|
| **`download_worker()`** | Thread function - downloads pieces in loop |
| **`download_file()`** | Main download orchestrator - creates threads, manages download |

**Download Flow**:
1. Query tracker for peers
2. Get file info from first peer
3. Initialize download context
4. Create 3 worker threads (or fewer if less peers)
5. Each thread repeatedly calls `get_next_piece()` and downloads
6. Threads update progress bar
7. Wait for all threads to finish
8. Assemble pieces into final file

#### **Upload System (Serving Files)**
| Function | Purpose |
|----------|---------|
| **`handle_peer_upload()`** | Thread function - serves FILE_INFO or REQUEST_PIECE requests |
| **`listener_thread()`** | Background thread - accepts incoming peer connections |

#### **User Interface**
| Function | Purpose |
|----------|---------|
| **`list_shared_files()`** | Display files in shared directory |
| **`add_file_to_share()`** | Copy file to shared dir and split into pieces |
| **`register_file()`** | Tell tracker we have a file |
| **`query_file()`** | Ask tracker who has a file |
| **`show_menu()`** | Display interactive menu |
| **`clear_screen()`** | Clear terminal screen |

#### **Main Entry Point**
| Function | Purpose |
|----------|---------|
| **`main()`** | Parse arguments, start listener thread, run menu loop |

---

## 🔄 Complete System Flow

### **Scenario: Peer B downloads file from Peer A**

```
┌─────────────────────────────────────────────────────────────────────┐
│ STEP 1: Setup Phase                                                 │
└─────────────────────────────────────────────────────────────────────┘

[TRACKER] (tracker.c)
  ├─ Start server on port 8080
  └─ Wait for connections...

[PEER A] (peerv5.c)
  ├─ Start on port 9000
  ├─ Create directory structure (shared/, pieces/, downloads/, temp_download/)
  ├─ Start listener_thread() in background
  └─ Show menu

[PEER B] (peerv5.c)
  ├─ Start on port 9001
  ├─ Create directory structure
  ├─ Start listener_thread() in background
  └─ Show menu


┌─────────────────────────────────────────────────────────────────────┐
│ STEP 2: Peer A Shares a File                                        │
└─────────────────────────────────────────────────────────────────────┘

[PEER A - User selects "1. Add file to share"]
  ├─ User enters: /home/user/movie.mp4
  ├─ add_file_to_share()
  │   ├─ Copy file to p2p_data/shared/movie.mp4
  │   └─ split_file() [file_ops.c]
  │       ├─ Calculate: 10,000,000 bytes ÷ 256,000 = 40 pieces
  │       ├─ Create: movie.mp4.piece0, movie.mp4.piece1, ... piece39
  │       └─ Save to: p2p_data/pieces/
  └─ File ready!

[PEER A - User selects "3. Register file with tracker"]
  ├─ User enters: movie.mp4
  ├─ register_file()
  │   └─ connect_to_tracker()
  │       └─ Send: "REGISTER movie.mp4 9000\n"
  │
  └─ [TRACKER receives]
      ├─ Extracts real IP from socket: 192.168.1.5
      ├─ add_file("movie.mp4", "192.168.1.5", 9000)
      ├─ Stores in registered_files[0]
      └─ Reply: "OK\n"


┌─────────────────────────────────────────────────────────────────────┐
│ STEP 3: Peer B Downloads the File                                   │
└─────────────────────────────────────────────────────────────────────┘

[PEER B - User selects "5. Download a file"]
  ├─ User enters: movie.mp4
  │
  ├─ download_file()
  │   │
  │   ├─ Query tracker for peers
  │   │   ├─ connect_to_tracker()
  │   │   ├─ Send: "QUERY movie.mp4\n"
  │   │   └─ [TRACKER]
  │   │       ├─ find_peers("movie.mp4")
  │   │       ├─ Found 1 match!
  │   │       └─ Reply: "PEERS 1\n192.168.1.5:9000\n"
  │   │
  │   ├─ Parse response: Found Peer A at 192.168.1.5:9000
  │   │
  │   ├─ Get file info from Peer A
  │   │   ├─ get_file_info_from_peer()
  │   │   ├─ Connect to 192.168.1.5:9000
  │   │   ├─ Send: "FILE_INFO movie.mp4\n"
  │   │   └─ [PEER A - listener_thread accepts, creates upload thread]
  │   │       ├─ handle_peer_upload()
  │   │       ├─ Read request: "FILE_INFO movie.mp4"
  │   │       ├─ get_file_size("p2p_data/shared/movie.mp4") = 10,000,000
  │   │       ├─ calculate_num_pieces(10,000,000) = 40
  │   │       └─ Reply: "INFO 40 10000000\n"
  │   │
  │   ├─ Display: File is 9.54 MB, 40 pieces, 1 peer
  │   │
  │   ├─ Initialize download
  │   │   ├─ init_download_context() [multi_source.c]
  │   │   │   ├─ Allocate piece_status[40] = [0,0,0,0,...] (all not downloaded)
  │   │   │   ├─ Allocate piece_source[40] = [0,0,0,0,...] (track sources)
  │   │   │   ├─ Initialize mutex
  │   │   │   └─ init_progress() [progress_bar.c]
  │   │   └─ add_peer_to_context("192.168.1.5", 9000)
  │   │
  │   ├─ Create 3 worker threads
  │   │   ├─ pthread_create(&workers[0], download_worker, peer=Peer A)
  │   │   ├─ pthread_create(&workers[1], download_worker, peer=Peer A)
  │   │   └─ pthread_create(&workers[2], download_worker, peer=Peer A)
  │   │
  │   ├─ [THREAD 1 executes download_worker()]
  │   │   ├─ Loop until download complete
  │   │   ├─ get_next_piece() → returns 0 (locks mutex, marks piece 0 as downloading, unlocks)
  │   │   ├─ request_piece_from_peer(Peer A, "movie.mp4", piece=0)
  │   │   │   ├─ Connect to 192.168.1.5:9000
  │   │   │   ├─ Send: "REQUEST_PIECE movie.mp4 0\n"
  │   │   │   └─ [PEER A - handle_peer_upload()]
  │   │   │       ├─ read_piece("movie.mp4", "pieces", 0, buffer) [file_ops.c]
  │   │   │       ├─ Reply: "SEND_PIECE 0 256000\n"
  │   │   │       └─ Send: [256,000 bytes of data]
  │   │   ├─ Receive 256,000 bytes into buffer
  │   │   ├─ save_piece("movie.mp4", "temp_download", 0, buffer, 256000) [file_ops.c]
  │   │   ├─ mark_piece_completed(ctx, piece=0, peer=0, bytes=256000) [multi_source.c]
  │   │   │   └─ piece_status[0] = 2 (completed), update stats
  │   │   ├─ update_progress() → downloaded_pieces++, downloaded_bytes += 256000
  │   │   ├─ display_progress() → [██░░░░░░] 2.5% (1/40) 2.10 MB/s
  │   │   ├─ printf(" [P1]") → Shows Peer 1 contributed
  │   │   ├─ get_next_piece() → returns 3
  │   │   └─ Download piece 3... (loop continues)
  │   │
  │   ├─ [THREAD 2 executes download_worker()] (simultaneously!)
  │   │   ├─ get_next_piece() → returns 1 (Thread 1 already took 0)
  │   │   ├─ request_piece_from_peer(Peer A, "movie.mp4", piece=1)
  │   │   ├─ Receive and save piece 1
  │   │   ├─ printf(" [P1]")
  │   │   ├─ get_next_piece() → returns 4
  │   │   └─ Download piece 4... (loop continues)
  │   │
  │   ├─ [THREAD 3 executes download_worker()] (simultaneously!)
  │   │   ├─ get_next_piece() → returns 2
  │   │   ├─ Download piece 2... [P1]
  │   │   ├─ get_next_piece() → returns 5
  │   │   └─ Download piece 5... (loop continues)
  │   │
  │   ├─ [All 3 threads working in parallel!]
  │   │   Progress bar updates: [████████████░░░░░░░] 60% (24/40) [P1] [P1] [P1]
  │   │
  │   ├─ [Eventually all pieces downloaded]
  │   │   └─ piece_status = [2,2,2,2,2,...,2] (all completed)
  │   │
  │   ├─ Wait for threads: pthread_join(workers[0]), join(workers[1]), join(workers[2])
  │   │
  │   ├─ display_peer_stats() [multi_source.c]
  │   │   └─ Shows: Peer 1 downloaded 40/40 pieces (100%), 9.54 MB, 2.5 MB/s avg
  │   │
  │   ├─ assemble_file() [file_ops.c]
  │   │   ├─ Open output file: p2p_data/downloads/movie.mp4
  │   │   ├─ For i = 0 to 39:
  │   │   │   ├─ Read temp_download/movie.mp4.piece[i]
  │   │   │   └─ Append to output file
  │   │   └─ Complete file assembled!
  │   │
  │   ├─ Cleanup: rm -f temp_download/movie.mp4.piece*
  │   └─ Display: ✓ Download Complete! (took 4 seconds, 2.38 MB/s avg)
  │
  └─ Return to menu


┌─────────────────────────────────────────────────────────────────────┐
│ STEP 4: Multi-Source Download (Peer C from A + B)                   │
└─────────────────────────────────────────────────────────────────────┘

[PEER B registers the file]
  └─ Tracker now has: movie.mp4 from Peer A (192.168.1.5:9000) 
                                and Peer B (192.168.1.6:9001)

[PEER C downloads]
  ├─ Query tracker → "PEERS 2\n192.168.1.5:9000\n192.168.1.6:9001\n"
  ├─ Create 3 worker threads
  │   ├─ Thread 1 → downloads from Peer A (round-robin: 0 % 2 = 0)
  │   ├─ Thread 2 → downloads from Peer B (round-robin: 1 % 2 = 1)
  │   └─ Thread 3 → downloads from Peer A (round-robin: 2 % 2 = 0)
  │
  ├─ Threads work simultaneously:
  │   ├─ T1: piece 0 from A [P1], piece 3 from A [P1], piece 6 from A [P1]...
  │   ├─ T2: piece 1 from B [P2], piece 4 from B [P2], piece 7 from B [P2]...
  │   └─ T3: piece 2 from A [P1], piece 5 from A [P1], piece 8 from A [P1]...
  │
  └─ Final stats:
      ├─ Peer 1 (A): 20 pieces (50%), 5.0 MB
      └─ Peer 2 (B): 20 pieces (50%), 5.0 MB
      └─ 2x faster than single source!
```

---

## 🔐 Thread Safety Summary

### **Critical Sections (Protected by Mutex)**
- `piece_status[]` array - Only one thread can access at a time
- `piece_source[]` array - Track which peer downloaded each piece
- Peer statistics (pieces_downloaded, bytes_downloaded)
- Progress display updates

### **How Mutex Works**
```
Thread 1                  Thread 2                  Thread 3
   |                         |                         |
   ├─ get_next_piece()       ├─ get_next_piece()       ├─ get_next_piece()
   ├─ LOCK mutex ✓           ├─ LOCK mutex ✗ WAIT     ├─ LOCK mutex ✗ WAIT
   ├─ Read piece_status[0]   |   (blocked)             |   (blocked)
   ├─ Mark piece 0 = 1       |                         |
   ├─ UNLOCK mutex           |                         |
   ├─ Download piece 0       ├─ LOCK mutex ✓           |   (still blocked)
   |   (slow network I/O)    ├─ Read piece_status[1]   |
   |                         ├─ Mark piece 1 = 1       |
   |                         ├─ UNLOCK mutex           |
   |                         ├─ Download piece 1       ├─ LOCK mutex ✓
   |                         |                         ├─ Mark piece 2 = 1
   |                         |                         ├─ UNLOCK mutex
   |                         |                         └─ Download piece 2
```

**Result**: No duplicate downloads, maximum concurrency!

---

## 📊 Performance Characteristics

### **Single-Source Download**
- 3 threads downloading from 1 peer
- Bottleneck: Peer's upload speed
- Advantage: Still 3x faster if peer has good bandwidth

### **Multi-Source Download**
- 3 threads downloading from 2+ peers
- Load distributed across peers
- Advantage: 2-3x faster, resilient to peer failures

### **File Size vs Pieces**
| File Size | Pieces | Memory (piece_status) |
|-----------|--------|----------------------|
| 1 MB | 4 | 16 bytes |
| 10 MB | 40 | 160 bytes |
| 100 MB | 391 | 1.5 KB |
| 1 GB | 3,907 | 15.6 KB |

---

## 🛠️ Compilation & Running

### **Compile**
```bash
# Tracker
gcc -o tracker tracker.c -pthread

# Peer
gcc -o peer peerv5.c file_ops.c progress_bar.c network_utils.c multi_source.c -pthread
```

### **Run**
```bash
# Terminal 1: Start tracker
./tracker

# Terminal 2: Start Peer A on port 9000
./peer 9000 192.168.1.100

# Terminal 3: Start Peer B on port 9001  
./peer 9001 192.168.1.100
```

### **Usage Flow**
1. **Peer A**: Add file → Register with tracker
2. **Peer B**: Download file (gets from Peer A)
3. **Peer B**: Register downloaded file
4. **Peer C**: Download file (gets from both A and B simultaneously!)

---

## 🎯 Key Takeaways

1. **Modular Design**: Each component has a clear responsibility
2. **Thread Safety**: Mutexes protect shared data structures
3. **Scalability**: Easily add more peers for faster downloads
4. **Resilience**: Failed pieces can be retried from other peers
5. **Visibility**: Progress bars and statistics show real-time status
6. **Protocol**: Simple text-based protocol for peer communication

**This is a fully functional P2P file transfer system!** 🚀