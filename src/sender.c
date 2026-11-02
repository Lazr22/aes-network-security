#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 65536

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printf("Usage: %s IP PORT FILE\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    const char *filename = argv[3];

    FILE *file = fopen(filename, "rb");

    if (!file) {
        perror("fopen");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("socket");
        fclose(file);
        return 1;
    }

    struct sockaddr_in server = {0};

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address\n");
        fclose(file);
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server,
                sizeof(server)) < 0) {
        perror("connect");
        fclose(file);
        close(sock);
        return 1;
    }

    printf("[SENDER] Connected to %s:%d\n", ip, port);

    char buffer[BUFFER_SIZE];

    unsigned long long total = 0;

    while (1) {

        size_t n = fread(buffer, 1, sizeof(buffer), file);

        if (n > 0) {

            size_t sent = 0;

            while (sent < n) {

                ssize_t s = send(sock,
                                 buffer + sent,
                                 n - sent,
                                 0);

                if (s <= 0) {
                    perror("send");
                    fclose(file);
                    close(sock);
                    return 1;
                }

                sent += (size_t)s;
            }

            total += n;

            if (total % (10ULL * 1024 * 1024) < n) {
                printf("[SENDER] Sent %llu MB\n",
                       total / (1024 * 1024));
            }
        }

        if (n < sizeof(buffer)) {

            if (ferror(file)) {
                perror("fread");
                fclose(file);
                close(sock);
                return 1;
            }

            break;
        }
    }

    shutdown(sock, SHUT_WR);

    fclose(file);
    close(sock);

    printf("[SENDER] Finished: %llu bytes\n", total);

    return 0;
}

