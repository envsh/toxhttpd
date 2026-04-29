#include "bootstrap.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static BootstrapNode nodes[3] = {
    {"104.225.141.59", "-", 33445, "933BA20B2E258B4C0D475B6DECE90C7E827FE83EFA9655414E7841251B19A72C"},
    {"43.198.227.166", "-", 3389, "AD13AB0D434BCE6C83FE2649237183964AE3341D0AFB3BE1694B18505E4E135E"},
    {"3.0.24.15", "-", 33445, "E20ABCF38CDBFFD7D04B29C956B33F7B27A3BB7AF0618101617B036E4AEA402D"}
};

void bootstrap_all(Tox *tox)
{
    Tox_Err_Bootstrap err;
    for (int i = 0; i < 3; i++) {
        uint8_t pubkey[32];
        const char *key = nodes[i].public_key;
        for (int j = 0; j < 32; j++) {
            char tmp[3] = {key[j*2], key[j*2+1], 0};
            pubkey[j] = strtol(tmp, NULL, 16);
        }
        tox_bootstrap(tox, nodes[i].ipv4, nodes[i].port, pubkey, &err);
        LOGI("Bootstrap %d: UDP %s:%d", i, nodes[i].ipv4, nodes[i].port);
        tox_add_tcp_relay(tox, nodes[i].ipv4, nodes[i].port, pubkey, &err);
        LOGI("Bootstrap %d: TCP relay %s:%d", i, nodes[i].ipv4, nodes[i].port);
    }
}

int bootstrap_load(const char *filename)
{
    (void)filename;
    LOGI("bootstrap: using 3 hardcoded nodes");;
    return 0;
}

void bootstrap_random(uint8_t *pubkey, uint16_t *port, char *address)
{
    srand(time(NULL));
    int idx = rand() % 3;
    
    strcpy(address, nodes[idx].ipv4);
    *port = nodes[idx].port;
    
    const char *key = nodes[idx].public_key;
    for (int i = 0; i < 32; i++) {
        char tmp[3] = {key[i*2], key[i*2+1], 0};
        pubkey[i] = strtol(tmp, NULL, 16);
    }
    
    LOGI("bootstrap: node %d - %s:%d", idx, address, *port);
}