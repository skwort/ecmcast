#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <gpiod.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define FRAMEBUFFER_DEVICE "/dev/fb0" // Framebuffer device
#define BLANK_RGB565 0x0000
#define RED_RGB565 0xF800
#define GREEN_RGB565 0x07E0
#define BLUE_RGB565 0x001F
#define WHITE_RGB565 0xFFFF

// Packet types
#define PKT_TYPE_FILLBLANK  0
#define PKT_TYPE_SETPIXEL   1
#define PKT_TYPE_FILLRED    2
#define PKT_TYPE_FILLGREEN  3
#define PKT_TYPE_FILLBLUE   4
#define PKT_TYPE_FILLWHITE  5
#define PKT_TYPE_FILLCOLOUR 6
#define PKT_TYPE_RESOLUTION 7

#define CHIP_NAME "gpiochip0"
#define DATA_PINS {5, 6, 13, 19}  // Data inputs (5 is LSB)
#define CLOCK_PIN 26              // Rising edge triggers read
#define ACK_PIN 11                // Rising edge after each read
#define START_PIN 9               // Low to start, High to stop
#define STATUS_PINS {2, 3, 4}     // Status outputs (4 is LSB)
		
static volatile int running = 1;

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

void set_pixel(uint16_t *fb, int x, int y, uint16_t color,
               struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo)
{
    if (x >= 0 && x < vinfo->xres && y >= 0 && y < vinfo->yres) {
        long location = (x * (vinfo->bits_per_pixel / 8)) + (y * finfo->line_length);
        uint16_t *pixel = (uint16_t *)((uint8_t *)fb + location);
        *pixel = color;
    }
}

/**
 * @brief Fill the screen with a colour.
 *
 * @param Framebuffer  
 * @param screensize 
 * @param colour 
 */
void fill_screen(uint16_t *framebuffer, size_t screensize, uint16_t colour)
{
    unsigned long count;
    
    for (count = 0; count < (screensize / 2); count++) {
        *(framebuffer + count) = colour;
    }
}

int main(void)
{
    int fb_fd, server_fd, client_fd;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    uint16_t *framebuffer;
    size_t screensize;
    setup_signal_handler();
    // Open framebuffer device
    fb_fd = open(FRAMEBUFFER_DEVICE, O_RDWR);
    if (fb_fd < 0) {
        perror("Failed to open framebuffer device");
        return 1;
    }
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) ||
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Failed to get screen info");
        close(fb_fd);
        return 1;
    }
    printf("vinfo.xres %d, vinfo.yres %d,vinfo.bits_per_pixel %d, finfo.line_length %d\n\r", vinfo.xres, vinfo.yres,vinfo.bits_per_pixel, finfo.line_length);
    screensize = vinfo.yres_virtual * finfo.line_length;
    framebuffer = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((intptr_t)framebuffer == -1) {
        perror("Failed to mmap framebuffer");
        close(fb_fd);
        return 1;
    }

    memset(framebuffer, 0, screensize); // Clear framebuffer
    
    struct gpiod_chip *chip;
    struct gpiod_line *data_lines[4], *clock, *ack, *start, *status_lines[4];
    int data_pins[] = DATA_PINS;
    int status_pins[] = STATUS_PINS;
    
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

#ifdef DEBUG
        printf("Transaction started...\n");
#endif /* DEBUG */

        // Read 7 bytes (56 bits)
        uint8_t packet[7] = {0};
        for (int byte = 0; byte < 7; byte++) {
            uint8_t value = 0;
            // Read a single byte as two four-bit values
            for (int bit = 0; bit < 2; bit++) {
#ifdef DEBUG
                printf("Waiting for rising edge on clock.\n\r");
#endif /* DEBUG */

                while (gpiod_line_get_value(clock) == 0 && running);
                usleep(10);  // Small delay for stability

#ifdef DEBUG
                printf("Rising clock edge received.\n\r");
#endif /* DEBUG */

                // Set status lines
                for (int i = 0; i < 3; i++) {
                    gpiod_line_set_value(status_lines[i], ((byte >> i) & 0x01));
                }

                // Read data lines
                for (int i = 0; i < 4; i++) {
                    value |= gpiod_line_get_value(data_lines[i]) << (i + (bit * 4));
                }

                // Toggle ACK_PIN high then low
                gpiod_line_set_value(ack, 1);
                usleep(10);
                gpiod_line_set_value(ack, 0);

                // Wait for clock to go low
                while (gpiod_line_get_value(clock) == 1 && running);  
            }
            packet[byte] = value;
#ifdef DEBUG
            printf("Byte[%d] = %0X\n\r", byte, value);
#endif /* DEBUG */
        }

#ifdef DEBUG
        printf("Transaction completed, writing to framebuffer packet...\n");
#endif /* DEBUG */

        uint8_t type = packet[0];
        int x = packet[1] | (packet[2] << 8);
        int y = packet[3] | (packet[4] << 8);
        uint16_t colour = packet[5] | (packet[6] << 8);

        if (type == PKT_TYPE_FILLBLANK) {
            // Blank screen
            memset(framebuffer, 0x0000, screensize);
        } else if (type == PKT_TYPE_SETPIXEL) {
            // Set particular pixel
#ifdef DEBUG
	   printf("Received: Type=%d, X=%d, Y=%d, Color=0x%04X\n", type, x, y, colour);
#endif /* DEBUG */

            set_pixel(framebuffer, x, y, colour, &vinfo, &finfo);

        } else if (type == PKT_TYPE_FILLRED) {
            // Fill screen with Red
            fill_screen(framebuffer, screensize, RED_RGB565);

        } else if (type == PKT_TYPE_FILLGREEN) {
            // Fill screen with Green
            fill_screen(framebuffer, screensize, GREEN_RGB565);

        } else if (type == PKT_TYPE_FILLBLUE) {
            // Fill screen with Blue
            fill_screen(framebuffer, screensize, BLUE_RGB565);

        } else if (type == PKT_TYPE_FILLWHITE) {
            // Fill screen with White
            fill_screen(framebuffer, screensize, WHITE_RGB565);

        } else if (type == PKT_TYPE_FILLCOLOUR) {
            // Fill screen with Colour 
            fill_screen(framebuffer, screensize, colour);

        }
    }

    // Cleanup
    gpiod_chip_close(chip);
    munmap(framebuffer, screensize);
    close(fb_fd);

    return 0;
}