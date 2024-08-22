#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#if _MSC_VER

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib") //Winsock Library

#include <Windows.h>

#else

#include <unistd.h>

#endif

#define TARGET_ADDR "192.168.1.255"
#define TARGET_PORT 5001

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

#if _MSC_VER

typedef struct {
    SOCKET socket;
    struct sockaddr_in address;
} socket_handle_t;

void init_socks() {
    static WSADATA wsa;
    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
	{
		printf("Failed. Error Code : %d",WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");
}

int open_socket(socket_handle_t * h, char* target_address, int target_port) {
	if ( (h->socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
	{
		printf("socket() failed with error code : %d" , WSAGetLastError());
		return -1;
	}
    char broadcast = 1;
    if (setsockopt(h->socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        printf("can't set broadcast option");
        closesocket(h->socket);
        return -1;
    }

	memset((char *) &h->address, 0, sizeof(h->address));
	h->address.sin_family = AF_INET;
	h->address.sin_port = htons(target_port);

    if (inet_pton(AF_INET, (PCSTR)(target_address), &h->address.sin_addr.s_addr) < 0) {
        printf("can't set socket address");
        closesocket(h->socket);
        return -1;
    }
    return 0;
}

void close_socks() {
    WSACleanup();
}

int udp_push(socket_handle_t* h, char* msg) {
    printf("UDP: %s", msg);

    if (sendto(h->socket, msg, (int)strlen(msg) , 0 , (struct sockaddr *) &(h->address), sizeof(h->address)) == SOCKET_ERROR) {
        printf("sendto() failed with error code : %d" , WSAGetLastError());
        return -1;
	}
    return 0;
}

void second_sleep(int seconds) {
    Sleep(seconds * 1000);
}

#else

void init_socks() {}

typedef struct {
    int dummy;
} socket_handle_t;

int open_socket(socket_handle_t * h, char* target_address, int target_port) {
    (void) h;
    (void) target_address;
    (void) target_port;
    return 0;
}

int udp_push(socket_handle_t* h, char* msg) {
    (void) h;
    printf("%s", msg);
    return 0;
}

void close_socks() {}

void second_sleep(int seconds) {
    usleep(((long)seconds) * 1000000);
}

#endif

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

typedef struct {
    char * filename;
    int count;
    int interval;
    char * target_address;
    int target_port;
} config_t;

void show_help(int argc, char** argv) {
    (void) argc;
    printf("IWV forwarder\r\n");
    printf("-------------\r\n");
    printf("%s <IWV.TMP filename> [options]\r\n", argv[0]);
    printf("-h: show help\r\n");
    printf("-n <count>: repeat n times (0 == infinite, default == 1)\r\n");
    printf("-i <interval>: wait i seconds between repetitions (default == 60)\r\n");
    printf("-a <address>: target IP address (default == 192.168.1.255)\r\n");
    printf("-p <port>: target UDP port address (default == 5001)\r\n");
}

int parse_args(config_t * config, int argc, char** argv) {
    if ( argc < 2 ) {
        printf("too few arguments, need IWV filename!\r\n");
        show_help(argc, argv);
        return -1;
    }
    config->filename = argv[1];
    config->count = 1;
    config->interval = 60;
    config->target_address = TARGET_ADDR;
    config->target_port = TARGET_PORT;
    for(int i = 2; i < argc; ++i) {
        if(strcmp(argv[i], "-h") == 0) {
            show_help(argc, argv);
            return -1;
        } else if(strcmp(argv[i], "-n") == 0) {
            ++i;
            if (i >= argc) {
                printf("missing argument to '-n'");
                return -1;
            }
            config->count = atoi(argv[i]);
        } else if(strcmp(argv[i], "-i") == 0) {
            ++i;
            if (i >= argc) {
                printf("missing argument to '-i'");
                return -1;
            }
            config->interval = atoi(argv[i]);
        } else if(strcmp(argv[i], "-a") == 0) {
            ++i;
            if (i >= argc) {
                printf("missing argument to '-a'");
                return -1;
            }
            config->target_address = argv[i];
        } else if(strcmp(argv[i], "-p") == 0) {
            ++i;
            if (i >= argc) {
                printf("missing argument to '-p'");
                return -1;
            }
            config->target_port = atoi(argv[i]);
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    config_t config;
    if(parse_args(&config, argc, argv) != 0) { return -1; }

    printf("reading '%s', %d times with %d seconds interval.\r\n", config.filename, config.count, config.interval);
    printf("sending to %s:%d.\r\n", config.target_address, config.target_port);

    init_socks();
    socket_handle_t sock;
    if(open_socket(&sock, config.target_address, config.target_port)) {
        printf("can't open socket!\n");
        close_socks();
        return -1;
    }

    int time;
    float value;

    for(int i = 0; i < config.count || config.count == 0; ++i) {
        if(get_iwv_sample(config.filename, &time, &value)) {
            printf("can't read IWV\n");
        } else {
            char textbuffer[100];
            format_planet(textbuffer, 100, time, value);
            udp_push(&sock, textbuffer);
        }

        if( i + 1 != config.count ) {
            second_sleep(config.interval);
        }
    }

    close_socks();
}
