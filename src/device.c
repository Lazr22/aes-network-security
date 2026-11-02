#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#define KEY_SIZE 32
#define NONCE_SIZE 12
#define TAG_SIZE 16

#define CHUNK_SIZE 1432
#define MAX_RECORD (NONCE_SIZE + CHUNK_SIZE + TAG_SIZE)

static unsigned char key[KEY_SIZE];

int send_all(int fd, const void *data, size_t len)
{
    const unsigned char *p = data;

    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);

        if (n <= 0)
            return -1;

        p += n;
        len -= (size_t)n;
    }

    return 0;
}

int recv_all(int fd, void *data, size_t len)
{
    unsigned char *p = data;

    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);

        if (n <= 0)
            return -1;

        p += n;
        len -= (size_t)n;
    }

    return 0;
}

int hex_to_key(const char *hex)
{
    if (strlen(hex) != KEY_SIZE * 2)
        return -1;

    for (int i = 0; i < KEY_SIZE; i++) {

        unsigned int value;

        if (sscanf(hex + i * 2, "%2x", &value) != 1)
            return -1;

        key[i] = (unsigned char)value;
    }

    return 0;
}

int create_server(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    int opt = 1;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int connect_to(const char *ip, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
        return -1;

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr,
                sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int encrypt_record(
    const unsigned char *plaintext,
    int plaintext_len,
    unsigned char *record,
    int *record_len)
{
    unsigned char nonce[NONCE_SIZE];
    unsigned char tag[TAG_SIZE];

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx)
        return -1;

    int len = 0;
    int ciphertext_len = 0;

    if (RAND_bytes(nonce, sizeof(nonce)) != 1)
        goto error;

    if (EVP_EncryptInit_ex(
            ctx, EVP_aes_256_gcm(),
            NULL, NULL, NULL) != 1)
        goto error;

    if (EVP_CIPHER_CTX_ctrl(
            ctx, EVP_CTRL_GCM_SET_IVLEN,
            NONCE_SIZE, NULL) != 1)
        goto error;

    if (EVP_EncryptInit_ex(
            ctx, NULL, NULL, key, nonce) != 1)
        goto error;

    memcpy(record, nonce, NONCE_SIZE);

    if (EVP_EncryptUpdate(
            ctx,
            record + NONCE_SIZE,
            &len,
            plaintext,
            plaintext_len) != 1)
        goto error;

    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(
            ctx,
            record + NONCE_SIZE + ciphertext_len,
            &len) != 1)
        goto error;

    ciphertext_len += len;

    if (EVP_CIPHER_CTX_ctrl(
            ctx,
            EVP_CTRL_GCM_GET_TAG,
            TAG_SIZE,
            tag) != 1)
        goto error;

    memcpy(record + NONCE_SIZE + ciphertext_len,
           tag, TAG_SIZE);

    *record_len = NONCE_SIZE + ciphertext_len + TAG_SIZE;

    EVP_CIPHER_CTX_free(ctx);

    return 0;

error:

    EVP_CIPHER_CTX_free(ctx);
    return -1;
}

int decrypt_record(
    const unsigned char *record,
    int record_len,
    unsigned char *plaintext,
    int *plaintext_len)
{
    if (record_len < NONCE_SIZE + TAG_SIZE)
        return -1;

    const unsigned char *nonce = record;

    const unsigned char *ciphertext =
        record + NONCE_SIZE;

    int ciphertext_len =
        record_len - NONCE_SIZE - TAG_SIZE;

    const unsigned char *tag =
        record + NONCE_SIZE + ciphertext_len;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx)
        return -1;

    int len = 0;
    int total = 0;

    int ok = 0;

    if (EVP_DecryptInit_ex(
            ctx, EVP_aes_256_gcm(),
            NULL, NULL, NULL) != 1)
        goto done;

    if (EVP_CIPHER_CTX_ctrl(
            ctx, EVP_CTRL_GCM_SET_IVLEN,
            NONCE_SIZE, NULL) != 1)
        goto done;

    if (EVP_DecryptInit_ex(
            ctx, NULL, NULL, key, nonce) != 1)
        goto done;

    if (EVP_DecryptUpdate(
            ctx,
            plaintext,
            &len,
            ciphertext,
            ciphertext_len) != 1)
        goto done;

    total = len;

    if (EVP_CIPHER_CTX_ctrl(
            ctx,
            EVP_CTRL_GCM_SET_TAG,
            TAG_SIZE,
            (void *)tag) != 1)
        goto done;

    /*
     * Authentication is checked here.
     * Do not forward plaintext before this succeeds.
     */

    if (EVP_DecryptFinal_ex(
            ctx,
            plaintext + total,
            &len) != 1)
        goto done;

    total += len;

    *plaintext_len = total;

    ok = 1;

done:

    EVP_CIPHER_CTX_free(ctx);

    return ok ? 0 : -1;
}

/* DEVICE 1: ENCRYPT */

int run_encrypt(int listen_port,
                const char *next_ip,
                int next_port)
{
    printf("[DEVICE 1] Starting encryption service\n");

    int server = create_server(listen_port);

    if (server < 0) {
        perror("server");
        return 1;
    }

    printf("[DEVICE 1] Waiting for Sender...\n");

    int sender = accept(server, NULL, NULL);

    if (sender < 0) {
        perror("accept");
        close(server);
        return 1;
    }

    printf("[DEVICE 1] Sender connected\n");

    int next = connect_to(next_ip, next_port);

    if (next < 0) {
        perror("connect to Device 2");
        close(sender);
        close(server);
        return 1;
    }

    printf("[DEVICE 1] Connected to Device 2\n");

    unsigned char input[CHUNK_SIZE];
    unsigned char record[MAX_RECORD];

    unsigned long long total = 0;

    while (1) {

        ssize_t n = recv(sender, input,
                         sizeof(input), 0);

        if (n < 0) {
            perror("recv");
            goto fail;
        }

        if (n == 0)
            break;

        int record_len = 0;

        if (encrypt_record(input, (int)n,
                           record, &record_len) != 0) {
            fprintf(stderr, "[DEVICE 1] Encryption failed\n");
            goto fail;
        }

        uint32_t frame_len = htonl((uint32_t)record_len);

        if (send_all(next, &frame_len,
                     sizeof(frame_len)) < 0)
            goto fail;

        if (send_all(next, record,
                     (size_t)record_len) < 0)
            goto fail;

        total += (unsigned long long)n;

        if (total % (10ULL * 1024 * 1024) < (unsigned long long)n) {
            printf("[DEVICE 1] Encrypted %llu MB\n",
                   total / (1024 * 1024));
        }
    }

    shutdown(next, SHUT_WR);

    printf("[DEVICE 1] Encryption completed: %llu bytes\n",
           total);

    close(sender);
    close(next);
    close(server);

    return 0;

fail:

    close(sender);
    close(next);
    close(server);

    return 1;
}

/* DEVICE 2: DECRYPT AND VERIFY */

int run_decrypt(int listen_port,
                const char *receiver_ip,
                int receiver_port)
{
    printf("[DEVICE 2] Starting decryption service\n");

    int server = create_server(listen_port);

    if (server < 0) {
        perror("server");
        return 1;
    }

    printf("[DEVICE 2] Waiting for Device 1...\n");

    int device1 = accept(server, NULL, NULL);

    if (device1 < 0) {
        perror("accept");
        close(server);
        return 1;
    }

    printf("[DEVICE 2] Device 1 connected\n");

    int receiver = connect_to(receiver_ip, receiver_port);

    if (receiver < 0) {
        perror("receiver connection");
        close(device1);
        close(server);
        return 1;
    }

    printf("[DEVICE 2] Connected to Receiver\n");

    unsigned char record[MAX_RECORD];
    unsigned char plaintext[CHUNK_SIZE + 16];

    unsigned long long total = 0;

    while (1) {

        uint32_t frame_len;

        ssize_t first = recv(device1, &frame_len,
                             sizeof(frame_len), 0);

        if (first < 0) {
            perror("recv");
            goto fail;
        }

        if (first == 0)
            break;

        /*
         * Read the remaining prefix bytes if TCP
         * delivered only part of the header.
         */

        if (first < (ssize_t)sizeof(frame_len)) {
            if (recv_all(device1,
                         ((unsigned char *)&frame_len) + first,
                         sizeof(frame_len) - (size_t)first) < 0)
                goto fail;
        }

        uint32_t length = ntohl(frame_len);

        if (length < NONCE_SIZE + TAG_SIZE ||
            length > MAX_RECORD) {
            fprintf(stderr, "[DEVICE 2] Invalid record length\n");
            goto fail;
        }

        if (recv_all(device1, record, length) < 0)
            goto fail;

        int plaintext_len = 0;

        if (decrypt_record(record, (int)length,
                           plaintext, &plaintext_len) != 0) {

            fprintf(stderr,
                    "[DEVICE 2] AUTHENTICATION FAILED!\n");

            goto fail;
        }

        /*
         * Only authenticated plaintext is forwarded.
         */

        if (send_all(receiver, plaintext,
                     (size_t)plaintext_len) < 0)
            goto fail;

        total += (unsigned long long)plaintext_len;

        if (total % (10ULL * 1024 * 1024) <
            (unsigned long long)plaintext_len) {
            printf("[DEVICE 2] Verified and decrypted %llu MB\n",
                   total / (1024 * 1024));
        }
    }

    shutdown(receiver, SHUT_WR);

    printf("[DEVICE 2] All authentication tags valid\n");
    printf("[DEVICE 2] Decrypted bytes: %llu\n", total);

    close(device1);
    close(receiver);
    close(server);

    return 0;

fail:

    fprintf(stderr, "[DEVICE 2] Transfer aborted\n");

    close(device1);
    close(receiver);
    close(server);

    return 1;
}

int main(int argc, char *argv[])
{
    if (argc != 6) {
        printf("Usage:\n");

        printf("Encrypt:\n");
        printf("./device encrypt LISTEN_PORT NEXT_IP NEXT_PORT KEYHEX\n");

        printf("Decrypt:\n");
        printf("./device decrypt LISTEN_PORT RECEIVER_IP RECEIVER_PORT KEYHEX\n");

        return 1;
    }

    if (hex_to_key(argv[5]) != 0) {
        fprintf(stderr, "Invalid AES key\n");
        return 1;
    }

    int listen_port = atoi(argv[2]);
    int next_port = atoi(argv[4]);

    if (listen_port < 1 || listen_port > 65535 ||
        next_port < 1 || next_port > 65535) {
        fprintf(stderr, "Invalid port\n");
        return 1;
    }

    if (strcmp(argv[1], "encrypt") == 0) {
        return run_encrypt(listen_port,
                           argv[3], next_port);
    }

    if (strcmp(argv[1], "decrypt") == 0) {
        return run_decrypt(listen_port,
                           argv[3], next_port);
    }

    fprintf(stderr, "Unknown mode\n");

    return 1;
}

