#ifndef GATEWAY_STORE_H
#define GATEWAY_STORE_H
#include <stdint.h>
#include <stddef.h>
#define GW_RECORD_SIZE 256u
#define GW_RECORD_PAYLOAD 236u
#define GW_DATA_START 0x01800000u
#define GW_DATA_END 0x02000000u
#define GW_SECTOR_SIZE 4096u
typedef struct {
    int (*read)(void *user,uint32_t address,void *out,uint32_t length);
    int (*write)(void *user,uint32_t address,const void *data,uint32_t length);
    int (*erase)(void *user,uint32_t address);
    void *user;
} gw_flash_t;
typedef struct {
    gw_flash_t io;
    uint32_t base,size,next_slot,next_sequence,records,pending,head_slot;
    int cache;
} gw_store_t;
int gw_store_open(gw_store_t *s,const gw_flash_t *io,uint32_t base,uint32_t size,int cache);
int gw_store_append(gw_store_t *s,const char *text,size_t length,uint32_t timestamp);
/* Returns payload length, 0 if empty, negative on error. Peek never consumes. */
int gw_store_peek(gw_store_t *s,char *text,size_t capacity,uint32_t *sequence);
int gw_store_ack(gw_store_t *s,uint32_t sequence);
int gw_store_recent(gw_store_t *s,uint32_t index,char *text,size_t capacity,uint32_t *timestamp);
#endif
