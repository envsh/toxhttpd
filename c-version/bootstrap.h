#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include <stdint.h>
#include <tox/tox.h>

typedef struct BootstrapNode {
    const char *ipv4;
    const char *ipv6;
    uint16_t port;
    const char *public_key;
} BootstrapNode;

int bootstrap_load(const char *filename);
void bootstrap_random(uint8_t *pubkey, uint16_t *port, char *address);
void bootstrap_all(Tox *tox);

#endif /* BOOTSTRAP_H */