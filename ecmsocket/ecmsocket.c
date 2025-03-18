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

#define FRAMEBUFFER_DEVICE "/dev/fb0" // Framebuffer device
#define PORT 5000 // Listening port

//#define DEBUG     //uncomment for debug output

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
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

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

    // Create and configure the server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Failed to create socket");
        munmap(framebuffer, screensize);
        close(fb_fd);
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        close(server_fd);
        munmap(framebuffer, screensize);
        close(fb_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Failed to listen on socket");
        close(server_fd);
        munmap(framebuffer, screensize);
        close(fb_fd);
        return 1;
    }

    printf("Server listening on port %d\n", PORT);

    uint8_t buffer[7];

    while (running) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Failed to accept connection");
            continue;
        }

        while (running) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes_read == sizeof(buffer)) {
                // Extract type, x,y , colour from packet
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

            } else if (bytes_read <= 0) {
                break;
            }
        }
        close(client_fd);
    }

    // Cleanup
    close(server_fd);
    munmap(framebuffer, screensize);
    close(fb_fd);

    return 0;
}

