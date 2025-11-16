# P2P File Transfer System

A high-performance, BitTorrent-inspired peer-to-peer file transfer system implemented in C, featuring multi-source downloads, real-time progress tracking, and per-peer statistics.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-green.svg)](https://www.linux.org/)


---

## ✨ Features

### Core Functionality
- **Multi-Source Downloads**: Download different file pieces from multiple peers simultaneously
- **Real-Time Progress Tracking**: Visual progress bar with speed and ETA calculations
- **Per-Peer Statistics**: Detailed breakdown of each peer's contribution
- **Automatic File Chunking**: Files automatically split into 256KB pieces
- **File Integrity**: Automatic verification through piece reassembly
- **Thread-Safe Operations**: Mutex-protected shared data structures

### Networking
- **Centralized Tracker**: Maintains registry of available files and peers
- **IPv4 Support**: Compatible with standard IP networks
- **Bridged Network Mode**: Direct peer-to-peer connections via WiFi/LAN
- **Cross-Platform**: Works on localhost and distributed networks

### User Experience
- **Interactive Menu System**: Clean, intuitive command-line interface
- **Live Download Indicators**: Real-time display of which peer is contributing
- **Detailed Statistics**: Comprehensive per-peer download analytics
- **Error Handling**: Graceful failure recovery and retry mechanisms

---

## 🏗️ Architecture

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                    P2P File Transfer System                  │
├─────────────────────┬───────────────────────────────────────┤
│   Tracker Server    │         Peer Clients                  │
│                     │                                       │
│  ┌──────────────┐   │   ┌────────────┐   ┌──────────────┐  │
│  │ File Registry│   │   │   Peer 1   │   │   Peer 2     │  │
│  │              │◄──┼───┤ (Seeder)   │   │ (Downloader) │  │
│  │ Peer Tracker │   │   │            │   │              │  │
│  │              │───┼──►│ Shares:    │◄──┤ Downloads:   │  │
│  │ Port: 8080   │   │   │ movie.mp4  │   │ movie.mp4    │  │
│  └──────────────┘   │   └────────────┘   └──────────────┘  │
└─────────────────────┴───────────────────────────────────────┘
```

### Communication Flow

```
1. Registration Phase
   Peer → Tracker: REGISTER movie.mp4 9000
   Tracker → Peer: OK

2. Discovery Phase
   Peer → Tracker: QUERY movie.mp4
   Tracker → Peer: PEERS 2\n192.168.1.5:9000\n192.168.1.8:9001

3. Download Phase
   Peer → Peer: FILE_INFO movie.mp4
   Peer → Peer: INFO 82 20971520
   
   Peer → Peer: REQUEST_PIECE movie.mp4 0
   Peer → Peer: SEND_PIECE 0 256000\n[binary data]
```

---



## 📦 Installation

### 1. Clone Repository

```bash
git clone https://github.com/saumyashah0510/Peer-to-Peer-File-Transfer.git
cd Peer-to-Peer-File-Transfer
```

### 2. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential gcc make
```



### 3. Compile

```bash
# Compile tracker
gcc tracker/final_tracker.c -I common -o tracker.out

# Compile peer (with all features)
gcc peer/peerv5.c peer/network_utils.c peer/progress_bar.c peer/multi_source.c file_ops.c \
    -I common -I peer -o peer.out -lpthread
```



---

## 🚀 Quick Start

### Single Machine Test (Localhost)

#### Terminal 1: Start Tracker
```bash
./tracker.out
```

**Expected Output:**
```
========================================
   P2P File Transfer - Tracker Server  
========================================
Starting tracker on port 8080...
✓ Socket created
✓ Bound to 0.0.0.0:8080
✓ Listening for connections...
========================================
```

#### Terminal 2: Start Peer 1 (Seeder)
```bash
# Create test file
dd if=/dev/zero of=testfile.dat bs=1M count=20

# Start peer
./peer.out 9000 127.0.0.1

# In menu:
# 1. Add file to share → testfile.dat
# 3. Register file → testfile.dat
```

#### Terminal 3: Start Peer 2 (Also Seeder)
```bash
./peer.out 9001 127.0.0.1

# In menu:
# 1. Add file to share → testfile.dat
# 3. Register file → testfile.dat
```

#### Terminal 4: Start Peer 3 (Downloader)
```bash
./peer.out 9002 127.0.0.1

# In menu:
# 5. Download a file → testfile.dat
```

**Expected Result:**
```
[████████████████████████████████████████] 100.0% (82/82) 18.45 MB/s [P1] [P2] [P1] [P2]

========================================
      Per-Peer Download Statistics
========================================

Peer 1: 127.0.0.1:9000
├─ Pieces: 41/82 (50.0%)
├─ Data: 10.00 MB (50.0% of total)
├─ Avg Speed: 9.22 MB/s
└─ [███████████████░░░░░░░░░░░░░░░]

Peer 2: 127.0.0.1:9001
├─ Pieces: 41/82 (50.0%)
├─ Data: 10.00 MB (50.0% of total)
├─ Avg Speed: 9.23 MB/s
└─ [███████████████░░░░░░░░░░░░░░░]

========================================

✓ Download complete!
```

---

## 📖 Usage Guide

### Starting the Tracker

```bash
./tracker.out
```

The tracker will:
- Listen on port **8080**
- Accept connections from any network interface (0.0.0.0)
- Display all registered files and peers
- Handle REGISTER, QUERY, and UNREGISTER requests

### Starting a Peer

```bash
./peer.out <port> <tracker_ip>

# Examples:
./peer.out 9000 127.0.0.1          # Localhost
./peer.out 9000 192.168.1.100      # LAN
./peer.out 9000 172.20.10.5        # Mobile hotspot
```

**Parameters:**
- `<port>`: Port number for this peer to listen on (9000-9999 recommended)
- `<tracker_ip>`: IP address of the tracker server

### Menu Options

#### 1. Add File to Share
Copies a file to the shared directory and splits it into pieces.

```
Enter choice: 1
Enter full path of file: /path/to/myfile.pdf

✓ File copied to shared directory

Splitting file into pieces...
File size: 2457600 bytes
Number of pieces: 10
✓ File ready to share: myfile.pdf (10 pieces)
```

#### 2. List Shared Files
Displays all files available for sharing.

```
Enter choice: 2

--- Shared Files ---
1. movie.mp4 (150.50 MB, 588 pieces)
2. document.pdf (2.30 MB, 9 pieces)
3. testfile.dat (20.00 MB, 82 pieces)
```

#### 3. Register File with Tracker
Announces file availability to the tracker.

```
Enter choice: 3
Enter filename to register: movie.mp4
Registering with tracker...
✓ File 'movie.mp4' registered successfully!
```

#### 4. Query for a File
Searches for peers who have a specific file.

```
Enter choice: 4
Enter filename to search: movie.mp4
Searching...

✓ Found peers:
-------------------
PEERS 2
192.168.1.5:9000
192.168.1.8:9001
-------------------
```

#### 5. Download a File (Multi-Source)
Downloads file from multiple peers simultaneously.

```
Enter choice: 5
Enter filename to download: movie.mp4

Searching for peers...
✓ Found 2 peer(s)
Getting file information from 192.168.1.5:9000...

========================================
File: movie.mp4
Size: 150.50 MB (157810688 bytes)
Pieces: 588
Peers: 2
========================================

Added peer 1: 192.168.1.5:9000
Added peer 2: 192.168.1.8:9001

Starting multi-source download from 2 peer(s)...
Watch for [P1], [P2], [P3]... indicators showing which peer is contributing!

Spawning 2 download threads...

[████████████████████░░░░░░░░░░░░] 55.2% (325/588) 12.34 MB/s ETA: 0m 8s [P1] [P2] [P1] [P2]

[████████████████████████████████████████] 100.0% (588/588) 14.67 MB/s ETA: 0m 0s


========================================
      Per-Peer Download Statistics
========================================

Peer 1: 192.168.1.5:9000
├─ Pieces: 294/588 (50.0%)
├─ Data: 75.25 MB (50.0% of total)
├─ Avg Speed: 7.33 MB/s
└─ [███████████████░░░░░░░░░░░░░░░]

Peer 2: 192.168.1.8:9001
├─ Pieces: 294/588 (50.0%)
├─ Data: 75.25 MB (50.0% of total)
├─ Avg Speed: 7.34 MB/s
└─ [███████████████░░░░░░░░░░░░░░░]

========================================

Assembling file...
✓ Assembled piece 0 (256000 bytes)
✓ Assembled piece 1 (256000 bytes)
...
✓ Assembled piece 587 (121688 bytes)
✓ File assembled: p2p_data/downloads/movie.mp4

========================================
✓ Download Complete!
========================================
File: p2p_data/downloads/movie.mp4
Size: 150.50 MB
Average Speed: 14.67 MB/s
Time Taken: 10 seconds
========================================
```

#### 6. Exit
Closes the peer application.

```
Enter choice: 6
✓ Exiting...
```

---

## 📁 Project Structure

```
p2p-file-transfer/
│
├── README.md                    # Project documentation
├── LICENSE                      # MIT License
│
├── common/
│   └── protocol.h              # Protocol definitions and constants
│                               # - TRACKER_PORT (8080)
│                               # - PIECE_SIZE (256000)
│                               # - MAX_FILENAME (256)
│
├── tracker/
│   └── final_tracker.c         # Tracker server implementation
│                               # - Maintains file registry
│                               # - Handles peer connections
│                               # - Routes QUERY/REGISTER requests
│
├── peer/
│   ├── peerv5.c                # Main peer client (v5.0)
│   │                           # - Menu system
│   │                           # - Multi-source downloads
│   │                           # - Upload handler
│   │
│   ├── network_utils.h         # Network utility headers
│   ├── network_utils.c         # IP detection utilities
│   │                           # - get_my_ip()
│   │
│   ├── progress_bar.h          # Progress bar headers
│   ├── progress_bar.c          # Progress tracking implementation
│   │                           # - Real-time progress display
│   │                           # - Speed calculation
│   │                           # - ETA estimation
│   │
│   ├── multi_source.h          # Multi-source download headers
│   └── multi_source.c          # Multi-source download logic
│                               # - DownloadContext management
│                               # - Per-peer statistics
│                               # - Thread-safe piece allocation
│
├── file_ops.h                  # File operations headers
└── file_ops.c                  # File splitting/assembly
                                # - split_file()
                                # - assemble_file()
                                # - save_piece()
                                # - read_piece()
```

### Runtime Directory Structure

When peers run, they automatically create:

```
p2p_data/
├── shared/           # Files you're sharing (original files)
│   ├── movie.mp4
│   └── document.pdf
│
├── pieces/           # Split pieces of shared files
│   ├── movie.mp4.piece0
│   ├── movie.mp4.piece1
│   └── ...
│
├── downloads/        # Completed downloaded files
│   └── movie.mp4
│
└── temp_download/    # Temporary pieces during active download
    ├── movie.mp4.piece0
    └── ...
```

---

## 🔌 Protocol Specification

### Message Types

#### Peer → Tracker Messages

| Command | Format | Description | Example |
|---------|--------|-------------|---------|
| REGISTER | `REGISTER <filename> <port>\n` | Register a file with tracker | `REGISTER movie.mp4 9000\n` |
| QUERY | `QUERY <filename>\n` | Find peers who have a file | `QUERY movie.mp4\n` |
| UNREGISTER | `UNREGISTER <filename> <port>\n` | Remove file from tracker | `UNREGISTER movie.mp4 9000\n` |

#### Tracker → Peer Responses

| Response | Format | Description | Example |
|----------|--------|-------------|---------|
| OK | `OK\n` | Command successful | `OK\n` |
| PEERS | `PEERS <count>\n<ip>:<port>\n...` | List of peers sharing file | `PEERS 2\n192.168.1.5:9000\n192.168.1.8:9001\n` |
| ERROR | `ERROR <message>\n` | Error occurred | `ERROR File not found\n` |

#### Peer → Peer Messages

| Command | Format | Description | Example |
|---------|--------|-------------|---------|
| FILE_INFO | `FILE_INFO <filename>\n` | Request file metadata | `FILE_INFO movie.mp4\n` |
| REQUEST_PIECE | `REQUEST_PIECE <filename> <index>\n` | Request specific piece | `REQUEST_PIECE movie.mp4 42\n` |

#### Peer → Peer Responses

| Response | Format | Description | Example |
|----------|--------|-------------|---------|
| INFO | `INFO <pieces> <size>\n` | File metadata | `INFO 588 157810688\n` |
| SEND_PIECE | `SEND_PIECE <index> <size>\n<data>` | Piece data (header + binary) | `SEND_PIECE 42 256000\n[256000 bytes]` |

### Example Complete Exchange

```
# 1. Peer registers file with tracker
Peer (192.168.1.5:9000) → Tracker: REGISTER movie.mp4 9000\n
Tracker → Peer: OK\n

# 2. Another peer queries for file
Peer (192.168.1.8:9001) → Tracker: QUERY movie.mp4\n
Tracker → Peer: PEERS 1\n192.168.1.5:9000\n

# 3. Downloading peer requests file info
Peer (192.168.1.8:9001) → Peer (192.168.1.5:9000): FILE_INFO movie.mp4\n
Peer (192.168.1.5:9000) → Peer (192.168.1.8:9001): INFO 588 157810688\n

# 4. Downloading peer requests first piece
Peer (192.168.1.8:9001) → Peer (192.168.1.5:9000): REQUEST_PIECE movie.mp4 0\n
Peer (192.168.1.5:9000) → Peer (192.168.1.8:9001): SEND_PIECE 0 256000\n[256000 bytes of binary data]

# 5. Process repeats for all 588 pieces
```

---











## 📄 License

This project is licensed under the **MIT License**.

```
MIT License

Copyright (c) 2024 P2P File Transfer Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---


## Thank You
**Happy file sharing!** 🚀
