#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/spi/spidev.h>

#define SPI_DEVICE "/dev/spidev1.0" // SPI1 device
#define FRAMEBUFFER_DEVICE "/dev/fb0" // Framebuffer device
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

void set_pixel(uint16_t *fb, int x, int y, uint16_t color, struct fb_var_screeninfo vinfo) {
    if (x >= 0 && x < vinfo.xres && y >= 0 && y < vinfo.yres) {
        long location = x + vinfo.xres * y;
        fb[location] = color;
    }
}

int main() {
    int spi_fd, fb_fd;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    uint16_t *framebuffer;
    size_t screensize;

    // Open SPI device
    spi_fd = open(SPI_DEVICE, O_RDWR);
    if (spi_fd < 0) {
        perror("Failed to open SPI device");
        return 1;
    }

    uint8_t mode = SPI_MODE_0;
    uint32_t speed = 500000;
    uint8_t bits_per_word = 8;

    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    // Open framebuffer device
    fb_fd = open(FRAMEBUFFER_DEVICE, O_RDWR);
    if (fb_fd < 0) {
        perror("Failed to open framebuffer device");
        close(spi_fd);
        return 1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) || ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Failed to get screen info");
        close(spi_fd);
        close(fb_fd);
        return 1;
    }

    screensize = vinfo.yres_virtual * finfo.line_length;

    framebuffer = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((intptr_t)framebuffer == -1) {
        perror("Failed to mmap framebuffer");
        close(spi_fd);
        close(fb_fd);
        return 1;
    }

    memset(framebuffer, 0, screensize); // Clear framebuffer

    uint8_t buffer[7];
    while (1) {
        // Read 7 bytes from SPI
        if (read(spi_fd, buffer, sizeof(buffer)) == sizeof(buffer)) {
            int x = buffer[0] | (buffer[1] << 8);
            int y = buffer[2] | (buffer[3] << 8);
            uint16_t color = buffer[4] | (buffer[5] << 8);

            // Write pixel to framebuffer
            set_pixel(framebuffer, x, y, color, vinfo);
        } else {
            perror("Failed to read from SPI");
        }
    }

    munmap(framebuffer, screensize);
    close(fb_fd);
    close(spi_fd);
    return 0;
}
