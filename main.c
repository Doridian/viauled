#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>

#include <hidapi/hidapi.h>
#include <linux/uleds.h>

#define DEFAULT_LED_DEV "viauled::kbd_backlight"
#define DEFAULT_HID_DEV ""
#define DEFAULT_VID 0x32ac
#define DEFAULT_PID 0x0012
#define STARTUP_BRIGHTNESS 0xFF

// The len is slightly too long due to the %s, but just 2 bytes aren't worth it
#define ULED_SYSFS_PATH "/sys/class/leds/%s/brightness"
#define ULED_SYSFS_PATH_LEN strlen(ULED_SYSFS_PATH)

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

#define OPTARG_TO_NUMBER(VAR) { \
    VAR = strtol(optarg, &endptr, 0); \
    if (*endptr != '\0') { \
        printf("Invalid number for option %c: %s\n", opt, optarg); \
        return 1; \
    } \
}

int usage(char **argv) {
    printf("Usage (search by VID/PID): %s -v VID -p PID [-l LED_DEVICE]\n", argv[0]);
    printf("Usage (specify HID device): %s -h HID_DEVICE [-l LED_DEVICE]\n", argv[0]);
    return 1;
}

int wait_for_self(const char* led_dev) {
    char *sysfs_path = malloc(ULED_SYSFS_PATH_LEN + strlen(led_dev) + 1);
    sprintf(sysfs_path, ULED_SYSFS_PATH, led_dev);
    printf("Waiting for %s\n", sysfs_path);
    int fd;
    while (running) {
        fd = open(sysfs_path, O_RDWR);
        if (fd) {
            break;
        }
        if (errno == ENOENT) {
            continue;
        }
        perror("open(sysfs_path");
        return 1;
    }
#ifdef STARTUP_BRIGHTNESS
    char brightness_str[64]; // way too large, but whatever
    sprintf(brightness_str, "%d\n", STARTUP_BRIGHTNESS);
    printf("Setting brightness to %s", brightness_str);
    write(fd, brightness_str, strlen(brightness_str));
#endif
    (void)close(fd);
    printf("Startup complete!\n");
    return 0;
}

int main(int argc, char **argv) {
    int vid = DEFAULT_VID, pid = DEFAULT_PID, opt;
    char *endptr;

    char *led_dev = malloc(strlen(DEFAULT_LED_DEV) + 1);
    strcpy(led_dev, DEFAULT_LED_DEV);

    char *hid_dev = malloc(strlen(DEFAULT_HID_DEV) + 1);
    strcpy(hid_dev, DEFAULT_HID_DEV);

    running = 1;

    while ((opt = getopt(argc, argv, "wv:p:l:h:")) != -1) {
        switch (opt) {
            case 'v':
                OPTARG_TO_NUMBER(vid)
                break;
            case 'p':
                OPTARG_TO_NUMBER(pid)
                break;
            case 'h':
                hid_dev = realloc(hid_dev, strlen(optarg) + 1);
                strcpy(hid_dev, optarg);
                break;
            case 'l':
                led_dev = realloc(led_dev, strlen(optarg) + 1);
                strcpy(led_dev, optarg);
                break;
            case 'w':
                return wait_for_self(led_dev);
            default:
                return usage(argv);
        }
    }

    (void)hid_init();

    if (hid_dev[0] == '\0') {
        if (vid < 0x0000 || vid > 0xFFFF) {
            printf("Invalid VID: %04X\n", vid);
            return usage(argv);
        }

        if (pid < 0x0000 || pid > 0xFFFF) {
            printf("Invalid PID: %04X\n", pid);
            return usage(argv);
        }

        struct hid_device_info* info = hid_enumerate(vid, pid);
        while (info != NULL && info->interface_number != QMK_INTERFACE) {
            info = info->next;
        }

        if (info == NULL) {
            printf("Could not find HID!\n");
            return 1;
        }

        hid_dev = realloc(hid_dev, strlen(info->path) + 1);
        strcpy(hid_dev, info->path);
    }

    printf("Using HID at %s\n", hid_dev);
    handle = hid_open_path(hid_dev);
    if (!handle) {
        printf("Could not open HID!\n");
        return 1;
    }

	struct uleds_user_dev uleds_dev;
	strncpy(uleds_dev.name, led_dev, LED_MAX_NAME_SIZE);
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
