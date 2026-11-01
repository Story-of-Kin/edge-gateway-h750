#ifndef GATEWAY_AT_H
#define GATEWAY_AT_H
#include <stdint.h>
#include <stddef.h>
#define GW_AT_OK 1u
#define GW_AT_SEND_OK 2u
#define GW_AT_PROMPT 4u
#define GW_AT_FAIL 8u
#define GW_AT_CLOSED 16u
#define GW_AT_CONNACK 32u
#define GW_AT_PONG 64u
typedef struct {
    char line[256];size_t line_length;
    uint32_t ipd_remaining;
    uint8_t mqtt[512];size_t mqtt_length;
    uint32_t events;
} gw_at_parser_t;
void gw_at_reset(gw_at_parser_t *p);
void gw_at_feed(gw_at_parser_t *p,const uint8_t *data,size_t length);
#endif
