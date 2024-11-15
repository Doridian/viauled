#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <hidapi/hidapi.h>
#include <linux/uleds.h>

#define RAW_HID_BUFFER_SIZE 32

#define CHANNEL_BACKLIGHT 1
#define CHANNEL_RGB_LIGHT 2
#define CHANNEL_RGB_MATRIX 3

#define BACKLIGHT_VALUE_BRIGHTNESS 1
#define BACKLIGHT_VALUE_EFFECT 2

#define RGB_MATRIX_VALUE_BRIGHTNESS 1
#define RGB_MATRIX_VALUE_EFFECT 2
#define RGB_MATRIX_VALUE_EFFECT_SPEED 3
#define RGB_MATRIX_VALUE_COLOR 4

#define CUSTOM_SET_VALUE 0x07
#define CUSTOM_GET_VALUE 0x08

#define QMK_INTERFACE 0x01

static volatile int running;

static hid_device *handle;

static int send_message(const unsigned char message_id, const unsigned char* msg, const int msg_len, unsigned char* out_buf, const int out_buf_len) {
    unsigned char data[RAW_HID_BUFFER_SIZE];
    data[0] = 0x00; // report ID
    data[1] = message_id;
    if (msg_len > 0 && msg_len <= (RAW_HID_BUFFER_SIZE - 2)) {
        memcpy(data + 2, msg, msg_len);
    }

    int res = hid_write(handle, data, RAW_HID_BUFFER_SIZE);
    if (res < RAW_HID_BUFFER_SIZE) {
        printf("Error on hid_write: %ls (%d)\n", hid_error(handle), res);
        return -1;
    }

    res = hid_read_timeout(handle, data, RAW_HID_BUFFER_SIZE, 1000);
    if (res < RAW_HID_BUFFER_SIZE) {
        printf("Error on hid_read_timeout: %ls (%d)\n", hid_error(handle), res);
        return -1;
    }

    if (out_buf_len < 1 || out_buf == NULL) {
        return 0;
    }

    if (res > out_buf_len) {
        res = out_buf_len;
    }
    memcpy(out_buf, data, res);
    return res;
}

static int set_rgb_u8(const unsigned char key, const unsigned char value) {
    unsigned char msg[] = { CHANNEL_RGB_MATRIX, key, value };
    return send_message(CUSTOM_SET_VALUE, msg, sizeof(msg), NULL, 0);
}

static void set_rgb_brightness(const int brightness) {
    if (brightness < 0) {
        return;
    }

    set_rgb_u8(RGB_MATRIX_VALUE_BRIGHTNESS, brightness);
}

int main(int argc, char **argv) {
    running = 1;
   (void)hid_init();

    struct hid_device_info* info = hid_enumerate(0x32ac, 0x0012);
    while (info != NULL && info->interface_number != QMK_INTERFACE) {
        info = info->next;
    }

    if (info == NULL) {
        printf("Could not find keyboard!\n");
        return 1;
    }

    printf("Found HID at %s\n", info->path);

    handle = hid_open_path(info->path);
    if (!handle) {
        printf("Could not open keyboard!\n");
        return 1;
    }

	struct uleds_user_dev uleds_dev;
	strncpy(uleds_dev.name, "viauled::kbd_backlight", LED_MAX_NAME_SIZE);
	uleds_dev.max_brightness = 0xFF;

    int fd = open("/dev/uleds", O_RDWR);
    if (fd == -1) {
        perror("open(/dev/uleds)");
        return 1;
    }

    int ret = write(fd, &uleds_dev, sizeof(uleds_dev));
    if (ret == -1) {
        perror("write(/dev/uleds, &uleds_dev)");
        close(fd);
        return 1;
    }

    printf("uled initialized\n");

	int brightness;
    while (running) {
        ret = read(fd, &brightness, sizeof(brightness));
        if (ret == -1) {
            perror("read(/dev/uleds)");
            close(fd);
            return 1;
        }

        set_rgb_brightness(brightness);
    }

    close(fd);
    return 0;
}
