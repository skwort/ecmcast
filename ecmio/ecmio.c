#include <stdio.h>
#include <gpiod.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define CHIP_NAME "gpiochip0"
#define DATA_PINS {5, 6, 13, 19}  // Data inputs (5 is LSB)
#define CLOCK_PIN 26              // Rising edge triggers read
#define ACK_PIN 11                // Rising edge after each read
#define START_PIN 9               // Low to start, High to stop
#define STATUS_PINS {2, 3, 4}     // Status outputs (4 is LSB)
#define SERVER_IP "127.0.0.1"     // Localhost
#define SERVER_PORT 5000          // TCP Port

volatile int running = 1;

void handle_signal(int signal)
{
    running = 0;
}

void setup_signal_handler()
{
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

// Function to send data over TCP
void send_packet(uint8_t *packet)
{
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

int main(void)
{
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

    // Initialise data input lines
    for (int i = 0; i < 4; i++) {
        data_lines[i] = gpiod_chip_get_line(chip, data_pins[i]);
        gpiod_line_request_input(data_lines[i], "gpio_reader");
    }

    // Initialise status output lines
    for (int i = 0; i < 3; i++) {
        status_lines[i] = gpiod_chip_get_line(chip, status_pins[i]);
        gpiod_line_request_output(status_lines[i], "gpio_reader", 0);
    }

    // Initiallise control lines
    clock = gpiod_chip_get_line(chip, CLOCK_PIN);
    gpiod_line_request_input(clock, "gpio_reader");

    ack = gpiod_chip_get_line(chip, ACK_PIN);
    gpiod_line_request_output(ack, "gpio_reader", 0);

    start = gpiod_chip_get_line(chip, START_PIN);
    gpiod_line_request_input(start, "gpio_reader");

    while (running) {
        // Wait for START_PIN to go LOW (start transaction)
        while (gpiod_line_get_value(start) == 1 && running) {
            usleep(1000);
        }
        printf("Transaction started...\n");

        // Read 7 bytes (56 bits)
        for (int byte = 0; byte < 7; byte++) {

            uint8_t value = 0;

            // Read a single byte as two four-bit values
            for (int bit = 0; bit < 2; bit++) {
                printf("Waiting for rising edge on clock.\n\r");
                while (gpiod_line_get_value(clock) == 0 && running);
                usleep(10);  // Small delay for stability
                printf("Rising clock edge received.\n\r");

                // Set status lines
                for (int i = 0; i < 3; i++) {
                    gpiod_line_set_value(status_lines[i], ((byte >> i) & 0x01));
                }

                // Read data lines
                for (int i = 0; i < 4; i++) {
                    value |= gpiod_line_get_value(data_lines[i]) << i;
                }

                // Toggle ACK_PIN high then low
                gpiod_line_set_value(ack, 1);
                usleep(1000);
                gpiod_line_set_value(ack, 0);

                // Wait for clock to go low
                while (gpiod_line_get_value(clock) == 1 && running);  
            }

            packet[byte] = value;
            printf("Byte[%d] = %0X\n\r", byte, value);
        }

        printf("Transaction completed, sending packet...\n");
        send_packet(packet);
    }

    // Cleanup
    gpiod_chip_close(chip);
    return 0;
}