#include <stdio.h>
#include <stdlib.h>
#include <gpiod.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#define CHIP_NAME "gpiochip0"
#define DATA_PINS {5, 6, 13, 19}  // Data inputs (5 is LSB)
#define CLOCK_PIN 26              // Rising edge triggers read
#define ACK_PIN 11                // Rising edge after each read
#define START_PIN 9               // Low to start, High to stop
#define STATUS_PINS {2, 3, 4}     // Status outputs (2 is LSB)
#define SERVER_IP "127.0.0.1"     // Localhost
#define SERVER_PORT 5000          // TCP Port

// Function to send data over TCP
void send_packet(uint8_t *packet) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        close(sock);
        return;
    }

    send(sock, packet, 7, 0);
    close(sock);
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *data_lines[4], *clock, *ack, *start, *status_lines[4];
    int data_pins[] = DATA_PINS;
    int status_pins[] = STATUS_PINS;
    uint8_t packet[7];
    
    // Open GPIO chip
    chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip) {
        perror("Failed to open GPIO chip");
        return 1;
    }

    // Get data input lines
    for (int i = 0; i < 4; i++) {
        data_lines[i] = gpiod_chip_get_line(chip, data_pins[i]);
        gpiod_line_request_input(data_lines[i], "gpio_reader");
    }

    // Set status input lines
    for (int i = 0; i < 3; i++) {
        status_lines[i] = gpiod_chip_get_line(chip, status_pins[i]);
        gpiod_line_request_output(data_lines[i], "gpio_reader", 0);
    }

    // Get control lines
    clock = gpiod_chip_get_line(chip, CLOCK_PIN);
    gpiod_line_request_input(clock, "gpio_reader");

    ack = gpiod_chip_get_line(chip, ACK_PIN);
    gpiod_line_request_output(ack, "gpio_reader", 0);

    start = gpiod_chip_get_line(chip, START_PIN);
    gpiod_line_request_input(start, "gpio_reader");

    while (1) {
        // Wait for START_PIN to go LOW (start transaction)
        while (gpiod_line_get_value(start) == 1) {
            usleep(1000);
        }

        printf("Transaction started...\n");

        // Read 7 bytes (56 bits)
        for (int byte = 0; byte < 7; byte++) {
            uint8_t value = 0;

            // Get data input lines
            for (int i = 0; i < 4; i++) {
                data_lines[i] = gpiod_chip_get_line(chip, data_pins[i]);
                gpiod_line_request_input(data_lines[i], "gpio_reader");
        }

            for (int bit = 0; bit < 2; bit++) {  // 4-bit values, two per byte
                while (gpiod_line_get_value(clock) == 0); // Wait for rising edge
                usleep(10);  // Small delay for stability

		printf("clk\n\r");

                //Set Status bits
                for (int i = 0; i < 3; i++) {
                    gpiod_line_set_value(status_lines[i], ((byte >> i) & 0x01));
                }

		printf("val %X\n\r", value);

                // Toggle ACK_PIN high then low
                gpiod_line_set_value(ack, 1);
                usleep(10);
                gpiod_line_set_value(ack, 0);

                while (gpiod_line_get_value(clock) == 1);  // Wait for clock to go low
            }

            packet[byte] = value;
	    printf("%d - %0X\n\r", byte, value);
        }

        // Wait for START_PIN to go HIGH (end transaction)
        //while (gpiod_line_get_value(start) == 0) {
        //    usleep(1000);
       // }

        printf("Transaction completed, sending packet...\n");

        // Send packet via TCP
        send_packet(packet);
    }

    // Cleanup
    gpiod_chip_close(chip);
    return 0;
}