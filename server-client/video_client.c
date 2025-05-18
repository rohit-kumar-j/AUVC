#define GL_SILENCE_DEPRECATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <GLFW/glfw3.h>

#include "common.h"

// Frame management
typedef struct {
    uint32_t frame_id;
    uint32_t width;
    uint32_t height;
    uint32_t total_chunks;
    uint32_t chunks_received;
    uint8_t *chunks_status;
    uint8_t *frame_data;
    bool complete;
} FrameBuffer;

// Client state
typedef struct {
    // UDP sockets
    int video_socket;
    int control_socket;
    struct sockaddr_in server_video_addr;
    struct sockaddr_in server_control_addr;

    // OpenGL/GLFW
    GLFWwindow *window;
    GLuint texture_id;

    // Frame management
    FrameBuffer current_frame;
    FrameBuffer display_frame;

    // Control state
    ControlMessage control_msg;

    // Statistics
    uint32_t frames_received;
    uint32_t frames_displayed;
    uint32_t chunks_received;

    // Timing
    struct timeval last_control_time;
    struct timeval last_stats_time;
} ClientState;

// Forward declare callback functions
void error_callback(int error, const char* description);
void resize_callback(GLFWwindow* window, int width, int height);

// GLFW error callback
void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Window resize callback
void resize_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Initialize UDP sockets
bool init_network(ClientState *state) {
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

    // Set up server video address
    memset(&state->server_video_addr, 0, sizeof(state->server_video_addr));
    state->server_video_addr.sin_family = AF_INET;
    state->server_video_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    state->server_video_addr.sin_port = htons(VIDEO_PORT);

    // Set up server control address
    memset(&state->server_control_addr, 0, sizeof(state->server_control_addr));
    state->server_control_addr.sin_family = AF_INET;
    state->server_control_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    state->server_control_addr.sin_port = htons(CONTROL_PORT);

    // Bind video socket to receive video data
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(VIDEO_PORT);

    if (bind(state->video_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("Failed to bind video socket");
        close(state->video_socket);
        close(state->control_socket);
        return false;
    }

    // Set video socket to non-blocking
    int flags = fcntl(state->video_socket, F_GETFL, 0);
    fcntl(state->video_socket, F_SETFL, flags | O_NONBLOCK);

    printf("UDP sockets initialized: Connected to %s (Video: port %d, Control: port %d)\n",
          SERVER_IP, VIDEO_PORT, CONTROL_PORT);
    return true;
}

// Initialize GLFW and OpenGL
bool init_graphics(ClientState *state) {
    printf("Initializing GLFW and OpenGL...\n");

    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

    // Set error callback
    glfwSetErrorCallback(error_callback);

    // Create window
    state->window = glfwCreateWindow(FRAME_WIDTH, FRAME_HEIGHT, "Video Stream Client", NULL, NULL);
    if (!state->window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    // Make OpenGL context current
    glfwMakeContextCurrent(state->window);

    // Set resize callback
    glfwSetFramebufferSizeCallback(state->window, resize_callback);

    // Create texture
    glGenTextures(1, &state->texture_id);
    glBindTexture(GL_TEXTURE_2D, state->texture_id);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Initialize empty texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, FRAME_WIDTH, FRAME_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    // Clear screen to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(state->window);

    printf("GLFW and OpenGL initialized successfully\n");
    return true;
}

// Initialize frame buffers
bool init_frame_buffers(ClientState *state) {
    printf("Initializing frame buffers...\n");

    // Initialize current frame
    state->current_frame.frame_id = 0;
    state->current_frame.width = FRAME_WIDTH;
    state->current_frame.height = FRAME_HEIGHT;
    state->current_frame.total_chunks = 0;
    state->current_frame.chunks_received = 0;
    state->current_frame.chunks_status = NULL;
    state->current_frame.frame_data = NULL;
    state->current_frame.complete = false;

    // Initialize display frame
    state->display_frame.frame_id = 0;
    state->display_frame.width = FRAME_WIDTH;
    state->display_frame.height = FRAME_HEIGHT;
    state->display_frame.total_chunks = 0;
    state->display_frame.chunks_received = 0;
    state->display_frame.chunks_status = NULL;
    state->display_frame.frame_data = malloc(FRAME_WIDTH * FRAME_HEIGHT * 3);
    state->display_frame.complete = false;

    if (!state->display_frame.frame_data) {
        fprintf(stderr, "Failed to allocate display frame buffer\n");
        return false;
    }

    // Clear display frame
    memset(state->display_frame.frame_data, 0, FRAME_WIDTH * FRAME_HEIGHT * 3);

    printf("Frame buffers initialized\n");
    return true;
}

// Allocate or reallocate frame resources
bool ensure_frame_resources(FrameBuffer *frame, uint32_t width, uint32_t height, uint32_t total_chunks) {
    // Check if dimensions or chunk count changed
    if (frame->width != width || frame->height != height ||
        frame->total_chunks != total_chunks || !frame->frame_data) {

        // Free old resources if they exist
        if (frame->chunks_status) {
            free(frame->chunks_status);
            frame->chunks_status = NULL;
        }

        if (frame->frame_data) {
            free(frame->frame_data);
            frame->frame_data = NULL;
        }

        // Update frame properties
        frame->width = width;
        frame->height = height;
        frame->total_chunks = total_chunks;
        frame->chunks_received = 0;
        frame->complete = false;

        // Allocate new resources
        frame->chunks_status = (uint8_t *)calloc(total_chunks, sizeof(uint8_t));
        frame->frame_data = (uint8_t *)malloc(width * height * 3);

        if (!frame->chunks_status || !frame->frame_data) {
            fprintf(stderr, "Failed to allocate frame resources\n");
            return false;
        }

        // Initialize frame data to black
        memset(frame->frame_data, 0, width * height * 3);

        printf("Allocated frame resources: %dx%d, %d chunks\n", width, height, total_chunks);
    }

    return true;
}

// Reset frame for new frame ID
void reset_frame(FrameBuffer *frame, uint32_t frame_id) {
    frame->frame_id = frame_id;
    frame->chunks_received = 0;
    frame->complete = false;

    // Reset chunk status
    if (frame->chunks_status) {
        memset(frame->chunks_status, 0, frame->total_chunks);
    }
}

// Process incoming video chunks
void process_video_chunks(ClientState *state) {
    // Allocate buffer for receiving chunks
    uint8_t *chunk_buffer = (uint8_t *)malloc(sizeof(FrameChunkHeader) + MAX_PACKET_SIZE);
    if (!chunk_buffer) {
        fprintf(stderr, "Failed to allocate chunk buffer\n");
        return;
    }

    // Process all available chunks
    while (1) {
        // Try to receive a chunk
        struct sockaddr_in sender_addr;
        socklen_t sender_addr_len = sizeof(sender_addr);

        int recv_size = recvfrom(state->video_socket, chunk_buffer,
                                sizeof(FrameChunkHeader) + MAX_PACKET_SIZE, 0,
                                (struct sockaddr*)&sender_addr, &sender_addr_len);

        if (recv_size <= 0) {
            // No more chunks or error
            break;
        }

        // Check if we received at least a header
        if ((size_t)recv_size < sizeof(FrameChunkHeader)) {
            fprintf(stderr, "Received incomplete chunk header\n");
            continue;
        }

        // Parse header
        FrameChunkHeader *header = (FrameChunkHeader *)chunk_buffer;

        // Validate message type
        if (header->msg_type != MSG_TYPE_FRAME_CHUNK) {
            fprintf(stderr, "Received invalid message type: %d\n", header->msg_type);
            continue;
        }

        // Validate chunk size
        if (header->chunk_size > MAX_PACKET_SIZE ||
            (size_t)recv_size != sizeof(FrameChunkHeader) + header->chunk_size) {
            fprintf(stderr, "Received invalid chunk size\n");
            continue;
        }

        // Check if this is a new frame
        if (header->frame_id != state->current_frame.frame_id) {
            // New frame
            reset_frame(&state->current_frame, header->frame_id);
        }

        // Ensure we have resources for this frame
        if (!ensure_frame_resources(&state->current_frame, header->width, header->height,
                                   header->total_chunks)) {
            fprintf(stderr, "Failed to ensure frame resources\n");
            continue;
        }

        // Process the chunk
        uint32_t chunk_index = header->chunk_index;

        // Skip if we've already received this chunk
        if (chunk_index >= header->total_chunks || state->current_frame.chunks_status[chunk_index]) {
            continue;
        }

        // Copy chunk data to frame buffer
        uint8_t *chunk_data = chunk_buffer + sizeof(FrameChunkHeader);
        memcpy(state->current_frame.frame_data + header->chunk_offset, chunk_data, header->chunk_size);

        // Mark chunk as received
        state->current_frame.chunks_status[chunk_index] = 1;
        state->current_frame.chunks_received++;
        state->chunks_received++;

        // Check if frame is complete
        if (state->current_frame.chunks_received == state->current_frame.total_chunks) {
            state->current_frame.complete = true;
            state->frames_received++;

            // Swap frames (copy current to display)
            memcpy(state->display_frame.frame_data, state->current_frame.frame_data,
                  state->current_frame.width * state->current_frame.height * 3);
            state->display_frame.width = state->current_frame.width;
            state->display_frame.height = state->current_frame.height;
            state->display_frame.frame_id = state->current_frame.frame_id;
            state->display_frame.complete = true;

            // Mark that we've displayed this frame
            state->frames_displayed++;

            if (state->frames_displayed % 30 == 0) {
                printf("Received frame %u (complete with %u chunks)\n",
                       state->current_frame.frame_id, state->current_frame.total_chunks);
            }
        }
    }

    free(chunk_buffer);
}

// Update texture with current display frame
void update_texture(ClientState *state) {
    if (state->display_frame.complete) {
        glBindTexture(GL_TEXTURE_2D, state->texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                    state->display_frame.width, state->display_frame.height,
                    0, GL_RGB, GL_UNSIGNED_BYTE, state->display_frame.frame_data);
    }
}

// Render the frame
void render(ClientState *state) {
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, state->texture_id);

    // Draw texture as quad
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    glfwSwapBuffers(state->window);
}

// Send control input to server
void send_control_input(ClientState *state) {
    // Check if it's time to send a control update
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // Send control every 20ms (50Hz)
    long elapsed_ms = (current_time.tv_sec - state->last_control_time.tv_sec) * 1000 +
                      (current_time.tv_usec - state->last_control_time.tv_usec) / 1000;

    if (elapsed_ms < 20) {
        return;
    }

    // Update control message
    state->control_msg.msg_type = MSG_TYPE_CONTROL;

    // Check for joystick
    if (glfwJoystickPresent(GLFW_JOYSTICK_1)) {
        int count;
        const float *axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
        if (count >= 2) {
            state->control_msg.x_axis = axes[0];
            state->control_msg.y_axis = axes[1];
        }

        const unsigned char *buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &count);
        for (int i = 0; i < 8 && i < count; i++) {
            state->control_msg.buttons[i] = buttons[i];
        }
    } else {
        // Keyboard controls
        state->control_msg.x_axis = 0.0f;
        state->control_msg.y_axis = 0.0f;

        // WASD for movement
        if (glfwGetKey(state->window, GLFW_KEY_W) == GLFW_PRESS) state->control_msg.y_axis -= 1.0f;
        if (glfwGetKey(state->window, GLFW_KEY_S) == GLFW_PRESS) state->control_msg.y_axis += 1.0f;
        if (glfwGetKey(state->window, GLFW_KEY_A) == GLFW_PRESS) state->control_msg.x_axis -= 1.0f;
        if (glfwGetKey(state->window, GLFW_KEY_D) == GLFW_PRESS) state->control_msg.x_axis += 1.0f;

        // Map keys to buttons
        state->control_msg.buttons[0] = glfwGetKey(state->window, GLFW_KEY_SPACE) == GLFW_PRESS;
        state->control_msg.buttons[1] = glfwGetKey(state->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        state->control_msg.buttons[2] = glfwGetKey(state->window, GLFW_KEY_E) == GLFW_PRESS;
        state->control_msg.buttons[3] = glfwGetKey(state->window, GLFW_KEY_Q) == GLFW_PRESS;
    }

    // Send control message
    sendto(state->control_socket, &state->control_msg, sizeof(state->control_msg), 0,
          (struct sockaddr*)&state->server_control_addr, sizeof(state->server_control_addr));

    // Update timestamp
    state->last_control_time = current_time;
}

// Print statistics
void print_statistics(ClientState *state) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // Print stats every 1000ms
    long elapsed_ms = (current_time.tv_sec - state->last_stats_time.tv_sec) * 1000 +
                      (current_time.tv_usec - state->last_stats_time.tv_usec) / 1000;

    if (elapsed_ms >= 1000) {
        printf("Statistics: Frames received=%u, displayed=%u, chunks=%u\n",
               state->frames_received, state->frames_displayed, state->chunks_received);

        // Update timestamp
        state->last_stats_time = current_time;
    }
}

// Cleanup resources
void cleanup(ClientState *state) {
    // Free network resources
    if (state->video_socket >= 0) close(state->video_socket);
    if (state->control_socket >= 0) close(state->control_socket);

    // Free frame buffers
    if (state->current_frame.chunks_status) free(state->current_frame.chunks_status);
    if (state->current_frame.frame_data) free(state->current_frame.frame_data);
    if (state->display_frame.chunks_status) free(state->display_frame.chunks_status);
    if (state->display_frame.frame_data) free(state->display_frame.frame_data);

    // Clean up OpenGL/GLFW
    if (state->texture_id) glDeleteTextures(1, &state->texture_id);
    if (state->window) glfwDestroyWindow(state->window);
    glfwTerminate();

    printf("Client cleanup complete\n");
}

int main() {
    printf("===== UDP Video Streaming Client Starting =====\n");

    // Initialize client state
    ClientState state = {0};
    state.video_socket = -1;
    state.control_socket = -1;

    // Initialize UDP sockets
    if (!init_network(&state)) {
        fprintf(stderr, "Failed to initialize network\n");
        cleanup(&state);
        return EXIT_FAILURE;
    }

    printf("Attempting to connect to server at %s:%d\n", SERVER_IP, CONTROL_PORT);

    // Initialize graphics
    if (!init_graphics(&state)) {
        fprintf(stderr, "Failed to initialize graphics\n");
        cleanup(&state);
        return EXIT_FAILURE;
    }

    // Initialize frame buffers
    if (!init_frame_buffers(&state)) {
        fprintf(stderr, "Failed to initialize frame buffers\n");
        cleanup(&state);
        return EXIT_FAILURE;
    }

    printf("Client initialized successfully. Connecting to server...\n");

    // Initialize timing
    gettimeofday(&state.last_control_time, NULL);
    gettimeofday(&state.last_stats_time, NULL);

    // Send initial control message to establish connection
    state.control_msg.msg_type = MSG_TYPE_CONTROL;
    sendto(state.control_socket, &state.control_msg, sizeof(state.control_msg), 0,
          (struct sockaddr*)&state.server_control_addr, sizeof(state.server_control_addr));

    printf("Sent initial control message. Waiting for video...\n");

    // Main loop
    while (!glfwWindowShouldClose(state.window)) {
        // Process video chunks
        process_video_chunks(&state);

        // Update texture and render
        update_texture(&state);
        render(&state);

        // Send control input
        send_control_input(&state);

        // Print statistics
        print_statistics(&state);

        // Poll events
        glfwPollEvents();

        // Small delay to prevent CPU hogging
        usleep(1000);  // 1ms
    }

    printf("Exiting...\n");

    // Cleanup
    cleanup(&state);

    return 0;
}
