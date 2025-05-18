#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include "common.h"

// Video source configuration
#define VIDEO_PATH "video.mp4"   // Path to video file (or device)
#define TARGET_FPS 30            // Target frames per second

// Server state
typedef struct {
    // UDP sockets
    int video_socket;
    int control_socket;

    // Client address for video
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;

    // FFmpeg components
    AVFormatContext *format_context;
    AVCodecContext *codec_context;
    struct SwsContext *sws_context;
    AVFrame *frame;
    AVPacket *packet;

    // Video stream info
    int video_stream_index;
    uint32_t frame_count;
    uint32_t chunk_count;

    // Frame buffer
    uint8_t *rgb_buffer;

    // Control state
    ControlMessage last_control;
    bool client_connected;

    // Timing
    struct timeval last_frame_time;
} ServerState;

// Initialize UDP sockets
bool init_network(ServerState *state) {
    printf("Initializing UDP sockets...\n");

    // Create video socket
    state->video_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (state->video_socket < 0) {
        perror("Failed to create video socket");
        return false;
    }

    // Create control socket
    state->control_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (state->control_socket < 0) {
        perror("Failed to create control socket");
        close(state->video_socket);
        return false;
    }

    // Set up control socket address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(CONTROL_PORT);

    // Bind control socket
    if (bind(state->control_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind control socket");
        close(state->video_socket);
        close(state->control_socket);
        return false;
    }

    // Set control socket to non-blocking mode
    int flags = fcntl(state->control_socket, F_GETFL, 0);
    fcntl(state->control_socket, F_SETFL, flags | O_NONBLOCK);

    // Initialize client address structure
    state->client_addr_len = sizeof(state->client_addr);
    memset(&state->client_addr, 0, state->client_addr_len);

    printf("UDP sockets initialized: Video port: %d, Control port: %d\n",
           VIDEO_PORT, CONTROL_PORT);
    return true;
}

// Initialize FFmpeg and open video
bool init_video(ServerState *state) {
    printf("Initializing FFmpeg and opening video: %s\n", VIDEO_PATH);

    // Open input file
    if (avformat_open_input(&state->format_context, VIDEO_PATH, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open input file '%s'\n", VIDEO_PATH);
        return false;
    }

    if (avformat_find_stream_info(state->format_context, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return false;
    }

    // Find video stream
    state->video_stream_index = -1;
    for (unsigned int i = 0; i < state->format_context->nb_streams; i++) {
        if (state->format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            state->video_stream_index = i;
            break;
        }
    }

    if (state->video_stream_index == -1) {
        fprintf(stderr, "Could not find a video stream\n");
        return false;
    }

    // Get codec parameters
    AVCodecParameters *codec_params = state->format_context->streams[state->video_stream_index]->codecpar;
    printf("Original video dimensions: %dx%d\n", codec_params->width, codec_params->height);

    // Find decoder
    const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        fprintf(stderr, "Unsupported codec\n");
        return false;
    }

    // Create codec context
    state->codec_context = avcodec_alloc_context3(codec);
    if (!state->codec_context) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return false;
    }

    // Copy parameters to context
    if (avcodec_parameters_to_context(state->codec_context, codec_params) < 0) {
        fprintf(stderr, "Could not copy codec parameters to context\n");
        return false;
    }

    // Open codec
    if (avcodec_open2(state->codec_context, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return false;
    }

    // Allocate frame and packet
    state->frame = av_frame_alloc();
    state->packet = av_packet_alloc();
    if (!state->frame || !state->packet) {
        fprintf(stderr, "Could not allocate frame or packet\n");
        return false;
    }

    // Initialize SWS context for scaling
    state->sws_context = sws_getContext(
        state->codec_context->width, state->codec_context->height, state->codec_context->pix_fmt,
        FRAME_WIDTH, FRAME_HEIGHT, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    if (!state->sws_context) {
        fprintf(stderr, "Could not initialize the conversion context\n");
        return false;
    }

    // Allocate RGB buffer
    state->rgb_buffer = (uint8_t *)malloc(FRAME_WIDTH * FRAME_HEIGHT * 3);
    if (!state->rgb_buffer) {
        fprintf(stderr, "Failed to allocate RGB buffer\n");
        return false;
    }

    printf("FFmpeg initialized successfully\n");
    return true;
}

// Read and process a single video frame
bool process_frame(ServerState *state) {
    // Check if we need a new packet
    int ret;
    bool frame_available = false;

    while (!frame_available) {
        // Try to receive a frame from the existing packet
        ret = avcodec_receive_frame(state->codec_context, state->frame);

        if (ret == 0) {
            // We have a frame
            frame_available = true;
        } else if (ret == AVERROR(EAGAIN)) {
            // Need more packets
            if (av_read_frame(state->format_context, state->packet) < 0) {
                // End of file or error, seek back to start
                av_seek_frame(state->format_context, state->video_stream_index, 0, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(state->codec_context);
                continue;
            }

            if (state->packet->stream_index != state->video_stream_index) {
                // Not a video packet
                av_packet_unref(state->packet);
                continue;
            }

            // Send packet to decoder
            ret = avcodec_send_packet(state->codec_context, state->packet);
            av_packet_unref(state->packet);

            if (ret < 0) {
                fprintf(stderr, "Error sending packet for decoding\n");
                return false;
            }
        } else {
            // Error
            fprintf(stderr, "Error during decoding\n");
            return false;
        }
    }

    // Convert frame to RGB
    uint8_t *dst_data[4] = {state->rgb_buffer, NULL, NULL, NULL};
    int dst_linesize[4] = {FRAME_WIDTH * 3, 0, 0, 0};

    sws_scale(state->sws_context,
              (const uint8_t * const *)state->frame->data, state->frame->linesize,
              0, state->codec_context->height,
              dst_data, dst_linesize);

    state->frame_count++;
    return true;
}

// Send frame in chunks via UDP
void send_frame(ServerState *state) {
    // Only send if we have a client address
    if (state->client_addr.sin_addr.s_addr == 0) {
        return;
    }

    // Calculate number of chunks
    size_t frame_size = FRAME_WIDTH * FRAME_HEIGHT * 3;
    int num_chunks = CALC_NUM_CHUNKS(frame_size, MAX_PACKET_SIZE - sizeof(FrameChunkHeader));

    // Allocate memory for message (header + data)
    size_t max_msg_size = sizeof(FrameChunkHeader) + MAX_PACKET_SIZE;
    uint8_t *msg_buffer = (uint8_t *)malloc(max_msg_size);

    // Send frame in chunks
    for (int i = 0; i < num_chunks; i++) {
        // Calculate chunk offset and size
        size_t chunk_offset = i * (MAX_PACKET_SIZE - sizeof(FrameChunkHeader));
        size_t chunk_size = (chunk_offset + (MAX_PACKET_SIZE - sizeof(FrameChunkHeader)) <= frame_size)
                          ? (MAX_PACKET_SIZE - sizeof(FrameChunkHeader))
                          : (frame_size - chunk_offset);

        // Prepare header
        FrameChunkHeader *header = (FrameChunkHeader *)msg_buffer;
        header->msg_type = MSG_TYPE_FRAME_CHUNK;
        header->frame_id = state->frame_count;
        header->chunk_index = i;
        header->total_chunks = num_chunks;
        header->width = FRAME_WIDTH;
        header->height = FRAME_HEIGHT;
        header->chunk_size = chunk_size;
        header->chunk_offset = chunk_offset;

        // Copy data
        memcpy(msg_buffer + sizeof(FrameChunkHeader),
               state->rgb_buffer + chunk_offset,
               chunk_size);

        // Send the chunk
        size_t msg_size = sizeof(FrameChunkHeader) + chunk_size;
        sendto(state->video_socket, msg_buffer, msg_size, 0,
               (struct sockaddr*)&state->client_addr, state->client_addr_len);

        state->chunk_count++;

        if (i % 10 == 0) {
            // Small delay every 10 chunks to prevent overwhelming the network
            usleep(1000);  // 1ms
        }
    }

    if (state->frame_count % 30 == 0) {
        printf("Sent frame %u in %d chunks (total: %u chunks)\n",
               state->frame_count, num_chunks, state->chunk_count);
    }

    free(msg_buffer);
}

// Check for control messages
void check_control_messages(ServerState *state) {
    // Try to receive a control message
    ControlMessage control;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int recv_size = recvfrom(state->control_socket, &control, sizeof(control), 0,
                            (struct sockaddr*)&client_addr, &addr_len);

    if (recv_size == sizeof(control) && control.msg_type == MSG_TYPE_CONTROL) {
        // Valid control message received
        memcpy(&state->last_control, &control, sizeof(control));

        // Update client address for video streaming
        memcpy(&state->client_addr, &client_addr, addr_len);
        state->client_addr.sin_port = htons(VIDEO_PORT);

        if (!state->client_connected) {
            printf("Client connected from %s! First control message received.\n",
                   inet_ntoa(client_addr.sin_addr));
            state->client_connected = true;
        }

        // Process control input
        printf("Control: X=%.2f, Y=%.2f, Buttons=[%d,%d,%d,%d,%d,%d,%d,%d]\n",
               control.x_axis, control.y_axis,
               control.buttons[0], control.buttons[1], control.buttons[2], control.buttons[3],
               control.buttons[4], control.buttons[5], control.buttons[6], control.buttons[7]);

        // Add your motor control or other logic here
    }
}

// Cleanup resources
void cleanup(ServerState *state) {
    // Free network resources
    if (state->video_socket >= 0) close(state->video_socket);
    if (state->control_socket >= 0) close(state->control_socket);

    // Free FFmpeg resources
    if (state->rgb_buffer) free(state->rgb_buffer);
    if (state->frame) av_frame_free(&state->frame);
    if (state->packet) av_packet_free(&state->packet);
    if (state->codec_context) avcodec_free_context(&state->codec_context);
    if (state->sws_context) sws_freeContext(state->sws_context);
    if (state->format_context) avformat_close_input(&state->format_context);

    printf("Server cleanup complete\n");
}

// Calculate time until next frame should be sent (for frame rate control)
int calculate_wait_time(ServerState *state) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // Calculate time elapsed since last frame
    long elapsed_us = (current_time.tv_sec - state->last_frame_time.tv_sec) * 1000000 +
                      (current_time.tv_usec - state->last_frame_time.tv_usec);

    // Time per frame in microseconds
    long frame_time_us = 1000000 / TARGET_FPS;

    // Calculate wait time
    long wait_time = frame_time_us - elapsed_us;

    // Update last frame time
    state->last_frame_time = current_time;

    // Return wait time (or 0 if we're behind)
    return (wait_time > 0) ? wait_time : 0;
}

int main() {
    printf("===== UDP Video Streaming Server Starting =====\n");

    // Initialize server state
    ServerState state = {0};
    state.video_socket = -1;
    state.control_socket = -1;

    // Initialize UDP sockets
    if (!init_network(&state)) {
        fprintf(stderr, "Failed to initialize network\n");
        cleanup(&state);
        return EXIT_FAILURE;
    }

    // Initialize FFmpeg and open video
    if (!init_video(&state)) {
        fprintf(stderr, "Failed to initialize video\n");
        cleanup(&state);
        return EXIT_FAILURE;
    }

    printf("Server initialized successfully. Waiting for client...\n");

    // Initialize timing
    gettimeofday(&state.last_frame_time, NULL);

    // Main loop
    while (1) {
        // Check for control messages
        check_control_messages(&state);

        // If no client yet, wait and continue
        if (!state.client_connected) {
            usleep(100000); // 100ms
            continue;
        }

        // Process and send frame
        if (process_frame(&state)) {
		printf("Sending to client at %s:%d\n", inet_ntoa(state.client_addr.sin_addr), ntohs(state.client_addr.sin_port));
            send_frame(&state);
        }

        // Control frame rate
        int wait_time = calculate_wait_time(&state);
        if (wait_time > 0) {
            usleep(wait_time);
        }
    }

    // We never reach here in this example, but proper cleanup is important
    cleanup(&state);
    return 0;
}
