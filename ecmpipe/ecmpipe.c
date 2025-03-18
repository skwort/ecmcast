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
#include <errno.h>

//#define DEBUG     //uncomment for debug output

#define FRAMEBUFFER_DEVICE "/dev/fb0" // Framebuffer device
#define NAMED_PIPE "ecmcast0" // Named pipe
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
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

static volatile int running = 1;

void handle_signal(int signal) {
    running = 0;
}


/**
 * @brief Set the pixel object
 * 
 * @param fb 
 * @param x 
 * @param y 
 * @param color 
 * @param vinfo 
 * @param finfo 
 */
void set_pixel(uint16_t *fb, int x, int y, uint16_t color, struct fb_var_screeninfo vinfo, struct fb_fix_screeninfo finfo) {
    if (x >= 0 && x < vinfo.xres && y >= 0 && y < vinfo.yres) {
        long location = (x * (vinfo.bits_per_pixel / 8)) + (y * finfo.line_length);
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
void fill_screen(uint16_t *framebuffer, size_t screensize, uint16_t colour) {

    unsigned long count;
    
    for (count = 0; count < ((screensize/2)); count++) {
        *(framebuffer+count) = colour;
    }
}

int main() {
    int fb_fd;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    uint16_t *framebuffer;
    size_t screensize;
    int pipe_fd;

    // Handle Ctrl+C to stop the program
    signal(SIGINT, handle_signal);

    // Open framebuffer device
    fb_fd = open(FRAMEBUFFER_DEVICE, O_RDWR);
    if (fb_fd < 0) {
        perror("Failed to open framebuffer device");
        return 1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) || ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Failed to get screen info");
        close(fb_fd);
        return 1;
    }

    screensize = vinfo.yres_virtual * finfo.line_length;

    framebuffer = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((intptr_t)framebuffer == -1) {
        perror("Failed to mmap framebuffer");
        close(fb_fd);
        return 1;
    }

    memset(framebuffer, 0, screensize); // Clear framebuffer

    // Create named pipe if it doesn't exist
    if (mkfifo(NAMED_PIPE, 0666) < 0 && errno != EEXIST) {
        perror("Failed to create named pipe");
        munmap(framebuffer, screensize);
        close(fb_fd);
        return 1;
    }

    // Open the named pipe for reading
    pipe_fd = open(NAMED_PIPE, O_RDONLY);
    if (pipe_fd < 0) {
        perror("Failed to open named pipe");
        munmap(framebuffer, screensize);
        close(fb_fd);
        return 1;
    }

    uint8_t buffer[7];

    // Main loop
    while (running) {

        ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer));
        if (bytes_read == sizeof(buffer)) {

            // Extract type, x,y,colour from packet
            uint8_t type = buffer[0];
            int x = buffer[1] | (buffer[2] << 8);
            int y = buffer[3] | (buffer[4] << 8);
            uint16_t colour = buffer[5] | (buffer[6] << 8);

            if (type == PKT_TYPE_FILLBLANK) {
                memset(framebuffer, 0x0000, screensize); // Blank screen

            } else if (type == PKT_TYPE_SETPIXEL) {
                
#ifdef DEBUG
                printf("Received: Type=%d, X=%d, Y=%d, Color=0x%04X\n", type, x, y, colour);
#endif
                set_pixel(framebuffer, x, y, colour, vinfo, finfo);

            } else if (type == PKT_TYPE_FILLRED) {
            fill_screen(framebuffer, screensize, RED_RGB565);      // Fill screen with Red

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
                // Fill screen with White
                fill_screen(framebuffer, screensize, colour);

            }

        } else if (bytes_read < 0) {
            perror("Failed to read from named pipe");
        }
    }

    // Cleanup
    close(pipe_fd);
    unlink(NAMED_PIPE);
    munmap(framebuffer, screensize);
    close(fb_fd);

    return 0;
}
