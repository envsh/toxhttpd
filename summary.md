# ToxHTTP - Tox HTTP REST API Server

## Overview
ToxHTTP exposes Tox core functionality via HTTP REST API, supporting push notifications via SSE and WebSocket.

## Architecture

### Threading Model (3 threads)
1. **Main HTTP thread**: Mongoose HTTP server
2. **Tox iterate thread**: Calls `tox_iterate()` in loop
3. **Event dispatcher thread**: Processes Tox events and dispatches to push handlers

### Components
- `main.c`: Entry point, signal handling
- `http_server.c`: HTTP handlers, routing
- `tox_core.c`: Tox core wrapper, callbacks (with logs)
- `event_queue.c`: Thread-safe event queue
- `push_sse.c`: Server-Sent Events handler
- `push_ws.c`: WebSocket handler
- `json_util.c`: JSON serialization
- `bootstrap.c`: Bootstrap nodes (3 TCP relay nodes)

## REST API Endpoints

### Self
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/api/self` | - | Get self address, name, status, connection_status |

### Friends
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| GET | `/api/friends` | - | List friend IDs |
| POST | `/api/friends` | `public_key` | Add friend (64-char pubkey or 76-char address) |
| POST | `/api/friend` | `friend_id` | Get friend info |

### Messages
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| POST | `/api/messages` | `friend_id`, `message` | Send message to friend |

### Bootstrap
| Method | Endpoint | Parameters | Description |
|--------|----------|------------|-------------|
| POST | `/api/bootstrap` | - | Bootstrap to network (uses 3 hardcoded nodes) |

## Push Endpoints

| Endpoint | Type | Description |
|----------|------|-------------|
| `/events/sse` | SSE | Server-Sent Events stream |

## Bootstrap Nodes

3 hardcoded bootstrap nodes with TCP relay:
- 104.225.141.59:33445
- 43.198.227.166:3389
- 3.0.24.15:33445

## Tox Callbacks (with logging)
- `callback_connection_status`: Connection status changes
- `callback_friend_request`: Incoming friend requests (auto-accepts)
- `callback_friend_message`: Incoming messages
- `callback_friend_name`: Friend name changes
- `callback_friend_status`: Friend status changes
- `callback_friend_status_message`: Friend status message
- `callback_friend_typing`: Typing status
- `callback_file_recv`: File transfer requests
- `callback_file_recv_control`: File transfer control

## Configuration
- Port: 8181 (default)
- Mode: TCP only (UDP disabled for stability)
- Log output: stderr

## Build
```bash
make
```

## Run
```bash
LD_LIBRARY_PATH=/home/yatseni/devsys/lib ./toxhttpd [port]
```

## Example Usage
```bash
# Start server
LD_LIBRARY_PATH=/home/yatseni/devsys/lib ./toxhttpd 8181

# After boot, check connection (should show "tcp")
curl http://localhost:8181/api/self

# Add friend (64-char pubkey or 76-char address)
curl -X POST -d "public_key=7FF9E857268C527647EB22C304DDA832B1CF578121FC1E5AD440C756ADDF6275" http://localhost:8181/api/friends

# Send message
curl -X POST -d "friend_id=0&message=Hello" http://localhost:8181/api/messages

# Get friends
curl http://localhost:8181/api/friends

# Get friend info
curl -X POST -d "friend_id=0" http://localhost:8181/api/friend
```

## Features Working
- [x] TCP connection (UDP disabled for stability)
- [x] Add friend with pubkey (64-char) or address (76-char)
- [x] Send messages
- [x] Receive messages (via callback logs)
- [x] Auto-accept friend requests
- [x] Connection status tracking
- [x] Friend typing status
- [x] Friend status updates

## Notes
- No authentication (single instance)
- TCP mode more stable than UDP
- Friends show "offline" when not connected - requires both sides online
- Messages require friend to be connected