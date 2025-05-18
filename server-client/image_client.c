#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

/* NOTE: Keep in mind to change the server ip */
// #define SERVER_IP "192.168.1.246" // Replace with your server's IP
#define SERVER_IP "127.0.0.1" // Replace with your server's IP
#define OUTPUT_IMAGE "received_image.jpg"

int main() {
    // Initialize ZMQ context
    void *context = zmq_ctx_new();

    // Create socket for request pattern
    void *requester = zmq_socket(context, ZMQ_REQ);

    // Connect to server
    char connection_string[100];
    sprintf(connection_string, "tcp://%s:5555", SERVER_IP);
    printf("Connecting to server at %s\n", connection_string);

    int rc = zmq_connect(requester, connection_string);
    if (rc != 0) {
        fprintf(stderr, "Failed to connect: %s\n", zmq_strerror(zmq_errno()));
        zmq_close(requester);
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    // Send request for image
    const char *request = "GET_IMAGE";
    zmq_send(requester, request, strlen(request), 0);
    printf("Sent request: %s\n", request);

    // Receive image size first
    long file_size;
    zmq_recv(requester, &file_size, sizeof(file_size), 0);
    printf("Image size: %ld bytes\n", file_size);

    // Allocate memory for image data
    unsigned char *image_data = malloc(file_size);
    if (!image_data) {
        perror("Memory allocation failed");
        zmq_close(requester);
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    // Receive image data
    int bytes_received = zmq_recv(requester, image_data, file_size, 0);
    if (bytes_received != file_size) {
        fprintf(stderr, "Received %d bytes, expected %ld bytes\n", bytes_received, file_size);
    } else {
        printf("Received full image: %d bytes\n", bytes_received);
    }

    // Save the image to a file
    FILE *output_file = fopen(OUTPUT_IMAGE, "wb");
    if (!output_file) {
        perror("Failed to create output file");
        free(image_data);
        zmq_close(requester);
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    size_t bytes_written = fwrite(image_data, 1, bytes_received, output_file);
    fclose(output_file);

    if (bytes_written != bytes_received) {
        fprintf(stderr, "Failed to write all data to file\n");
    } else {
        printf("Image saved to %s\n", OUTPUT_IMAGE);
    }

    // Clean up
    free(image_data);
    zmq_close(requester);
    zmq_ctx_destroy(context);

    return 0;
}
