#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

// Common configuration
// #define SERVER_IP "192.168.1.246"         // Change to your server's IP address
#define SERVER_IP "127.0.0.1"         // Change to your server's IP address
#define VIDEO_PORT 5555               // Port for video streaming
#define CONTROL_PORT 5556             // Port for control messages

#define FRAME_WIDTH 640               // Frame width
#define FRAME_HEIGHT 480              // Frame height
#define MAX_PACKET_SIZE 1400          // Maximum UDP packet size (to avoid fragmentation)
#define MAX_FRAME_SIZE (FRAME_WIDTH * FRAME_HEIGHT * 3) // RGB frame size

// Protocol message types
#define MSG_TYPE_FRAME_CHUNK 1        // Frame chunk message
#define MSG_TYPE_CONTROL 2            // Control message

// Frame chunk header
typedef struct {
    uint8_t msg_type;                 // Message type (MSG_TYPE_FRAME_CHUNK)
    uint32_t frame_id;                // Frame identifier
    uint32_t chunk_index;             // Chunk index
    uint32_t total_chunks;            // Total chunks in frame
    uint32_t width;                   // Frame width
    uint32_t height;                  // Frame height
    uint32_t chunk_size;              // Size of this chunk's data
    uint32_t chunk_offset;            // Offset in the frame
} FrameChunkHeader;

// Control message
typedef struct {
    uint8_t msg_type;                 // Message type (MSG_TYPE_CONTROL)
    float x_axis;                     // X-axis value (-1.0 to 1.0)
    float y_axis;                     // Y-axis value (-1.0 to 1.0)
    uint8_t buttons[8];               // Button states
} ControlMessage;

// Calculate number of chunks needed for a frame
#define CALC_NUM_CHUNKS(frame_size, chunk_size) \
    (((frame_size) + (chunk_size) - 1) / (chunk_size))

#endif /* COMMON_H */
