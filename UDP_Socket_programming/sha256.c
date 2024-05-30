#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha.h>

#define BUFSIZE 8192

#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha.h>

#define BUFSIZE 8192

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file_path>\n", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    unsigned char buffer[BUFSIZE];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, BUFSIZE, file))) {
        SHA256_Update(&sha256, buffer, bytesRead);
    }

    fclose(file);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    printf("SHA-256 hash of %s:\n", file_path);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");

    return 0;
}
