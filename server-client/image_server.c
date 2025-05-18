#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#define IMAGE_PATH "image2.jpg" // Path to your image file

int main() {
    // Read the image file
    FILE *file = fopen(IMAGE_PATH, "rb");
    if (!file) {
        perror("Failed to open image file");
        return EXIT_FAILURE;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer for image data
    unsigned char *image_data = malloc(file_size);
    if (!image_data) {
        perror("Memory allocation failed");
        fclose(file);
        return EXIT_FAILURE;
    }

    // Read the image file into buffer
    size_t bytes_read = fread(image_data, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        perror("Failed to read entire file");
        free(image_data);
        return EXIT_FAILURE;
    }

    printf("Read image file: %s (%ld bytes)\n", IMAGE_PATH, file_size);

    // Initialize ZMQ context
    void *context = zmq_ctx_new();

    // Create socket for reply pattern
    void *responder = zmq_socket(context, ZMQ_REP);

    // Bind to TCP port
    int rc = zmq_bind(responder, "tcp://*:5555");
    if (rc != 0) {
        fprintf(stderr, "Failed to bind socket: %s\n", zmq_strerror(zmq_errno()));
        free(image_data);
        zmq_close(responder);
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    printf("Server started at tcp://*:5555\n");
    printf("Waiting for client requests...\n");

    while (1) {
        // Wait for client request
        char buffer[10];
        zmq_recv(responder, buffer, 10, 0);
        printf("Received request: %s\n", buffer);

        // Send image size first
        zmq_send(responder, &file_size, sizeof(file_size), ZMQ_SNDMORE);

        // Send the image data
        zmq_send(responder, image_data, file_size, 0);

        printf("Sent image (%ld bytes)\n", file_size);
    }

    // Clean up
    free(image_data);
    zmq_close(responder);
    zmq_ctx_destroy(context);

    return 0;
}
