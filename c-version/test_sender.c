#include <tox/tox.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <target_tox_address>\n", argv[0]);
        return 1;
    }

    Tox *tox = tox_new(NULL, NULL);
    if (!tox) {
        printf("Failed to create tox\n");
        return 1;
    }

    uint8_t address[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, address);
    printf("My address: ");
    for (int i = 0; i < TOX_ADDRESS_SIZE; i++) {
        printf("%02x", address[i]);
    }
    printf("\n");

    Tox_Err_Bootstrap err;
    uint8_t pubkey[32];
    const char *key = "933BA20B2E258B4C0D475B6DECE90C7E827FE83EFA9655414E7841251B19A72C";
    for (int j = 0; j < 32; j++) {
        char tmp[3] = {key[j*2], key[j*2+1], 0};
        pubkey[j] = strtol(tmp, NULL, 16);
    }
    tox_add_tcp_relay(tox, "104.225.141.59", 33445, pubkey, &err);
    printf("Bootstrap TCP: err=%d\n", err);

    uint8_t target[TOX_ADDRESS_SIZE];
    memset(target, 0, sizeof(target));
    size_t len = strlen(argv[1]);
    for (size_t i = 0; i < len && i < TOX_ADDRESS_SIZE * 2; i += 2) {
        char tmp[3] = {argv[1][i], argv[1][i+1], 0};
        target[i/2] = strtol(tmp, NULL, 16);
    }

    Tox_Err_Friend_Add ferr;
    uint32_t fn = tox_friend_add(tox, target, (const uint8_t *)"Hello!", 6, &ferr);
    printf("Friend request sent: fn=%u err=%d (%s)\n", fn, ferr, tox_err_friend_add_to_string(ferr));

    for (int i = 0; i < 2000; i++) {
        tox_iterate(tox, NULL);
        usleep(tox_iteration_interval(tox) * 1000);
    }

    tox_kill(tox);
    return 0;
}