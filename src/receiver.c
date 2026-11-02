#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 65536

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s PORT OUTPUT_FILE\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    const char *filename = argv[2];

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[RECEIVER] Listening on port %d\n", port);

    int client = accept(server_fd, NULL, NULL);

    if (client < 0) {
        perror("accept");
        close(server_fd);
        return 1;
    }

    FILE *file = fopen(filename, "wb");

    if (!file) {
        perror("fopen");
        close(client);
        close(server_fd);
        return 1;
    }

    char buffer[BUFFER_SIZE];

    unsigned long long total = 0;

    while (1) {

        ssize_t n = recv(client, buffer,
                         sizeof(buffer), 0);

        if (n < 0) {
            perror("recv");
            fclose(file);
            close(client);
            close(server_fd);
            return 1;
        }

        if (n == 0)
            break;

        if (fwrite(buffer, 1, (size_t)n, file) !=
            (size_t)n) {
            perror("fwrite");
            fclose(file);
            close(client);
            close(server_fd);
            return 1;
        }

        total += (unsigned long long)n;
    }

    fclose(file);

    close(client);
    close(server_fd);

    printf("[RECEIVER] File received successfully\n");
    printf("[RECEIVER] Total bytes: %llu\n", total);

    return 0;
}

