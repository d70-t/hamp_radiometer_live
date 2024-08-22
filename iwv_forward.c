#include <stdio.h>
#include <errno.h>

#ifdef __GNUC__
errno_t fopen_s(FILE **f, const char *name, const char *mode) {
    errno_t ret = 0;
    *f = fopen(name, mode);
    /* Can't be sure about 1-to-1 mapping of errno and MS' errno_t */
    if (!*f)
        ret = errno;
    return ret;
}
#endif

void init_socks() {}

typedef struct {
    int dummy;
} socket_handle_t;

socket_handle_t open_socket() {
    socket_handle_t h = {0};
    return h;
}

int udp_push(socket_handle_t* h, char* msg) {
    printf("%s", msg);
    return 0;
}

void close_socks() {}

int is_leap_year(int year) {
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

int days_of_year(int year) {
    if (is_leap_year(year) ) {
        return 366;
    } else {
        return 365;
    }
}

int days_of_month(int year, int month) {
    switch(month) {
        case 1: return 31;
        case 2: return is_leap_year(year)?29:28;
        case 3: return 31;
        case 4: return 30;
        case 5: return 31;
        case 6: return 30;
        case 7: return 31;
        case 8: return 31;
        case 9: return 30;
        case 10: return 31;
        case 11: return 30;
        case 12: return 31;
    }
    return 0;
}

int get_iwv_sample(char * filename, int * time, float * value) {
    FILE * handle;
    if(fopen_s(&handle, filename, "rb") != 0) {
        printf("can't open '%s'", filename);
        return -1;
    }
    fseek(handle, 0, SEEK_END);
    long size = ftell(handle);

    printf("file size: %ld\n", size);

    long packet_count = (size - 24) / 13;
    long last_packet_start = 24 + (packet_count - 1) * 13;

    fseek(handle, last_packet_start, SEEK_SET);

    unsigned char buffer[13];
    fread(buffer, 13, 1, handle);
    fclose(handle);

    for(int i = 0; i < 13; ++i) {
        printf("%d ", buffer[i]);
    }
    printf("\n");

    *time = *((int*)buffer);
    *value = *((float*)(buffer + 5));

    return 0;
}

int format_planet(char * textbuffer, int bufsz, int time, float value) {
    int seconds = time % 60;
    int minutes = (time / 60) % 60;
    int hours = (time / (60 * 60)) % 24;
    int days_since_epoch = time / (60 * 60 * 24);

    int days_left = days_since_epoch;
    int year = 2001;

    while (days_of_year(year) < days_left) {
        days_left -= days_of_year(year);
        ++year;
    }

    int month = 1;
    while (days_of_month(year, month) < days_left) {
        days_left -= days_of_month(year, month);
        ++month;
    }

    int day = days_left + 1;

    return snprintf(textbuffer, bufsz, "KV,%04d-%02d-%02dT%02d:%02d:%02d,%f\r\n", year, month, day, hours, minutes, seconds, value);

}

int main(int argc, char** argv) {
    if ( argc < 2 ) {
        printf("too few arguments, need IWV filename!\n");
        return -1;
    }

    init_socks();
    socket_handle_t sock = open_socket();

    int time;
    float value;
    if(get_iwv_sample(argv[1], &time, &value)) {
        printf("can't read IWV\n");
        return -1;
    }

    char textbuffer[100];
    format_planet(textbuffer, 100, time, value);

    udp_push(&sock, textbuffer);

    close_socks();
}
