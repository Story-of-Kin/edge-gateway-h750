/*
 * Flash 记录存储的公共接口。同一实现可用于循环日志或待发送缓存。
 * 重要单位：地址/size/capacity 是字节，next_slot/head_slot 是记录槽序号。
 * 此模块不内置任务锁；运行时调用者必须用 g_storage_mutex 串行访问。
 */
#ifndef GATEWAY_STORE_H
#define GATEWAY_STORE_H
#include <stdint.h>
#include <stddef.h>
/* 一条记录占256字节，其中20字节是头部，最多236字节正文；擦除单位是4KB扇区。 */
#define GW_RECORD_SIZE 256u
#define GW_RECORD_PAYLOAD 236u
#define GW_DATA_START 0x01800000u
#define GW_DATA_END 0x02000000u
#define GW_SECTOR_SIZE 4096u
/*
 * 底层驱动函数指针。user 是传给每个驱动函数的上下文，可用于真实Flash或测试模型。
 * read/write/erase 返回0表示成功，非0表示I/O错误；erase 一次擦一个对齐扇区。
 */
typedef struct
{
    int (*read)(void *user, uint32_t address, void *out, uint32_t length);
    int (*write)(void *user, uint32_t address, const void *data, uint32_t length);
    int (*erase)(void *user, uint32_t address);
    void *user;
} gw_flash_t;
/*
 * 分区运行状态：base/size是边界，next_slot是下一写入候选，next_sequence是递增序号；
 * records为有效记录计数，pending为未确认缓存数，head_slot指向最早待发记录；cache非0启用缓存规则。
 */
typedef struct
{
    gw_flash_t io;
    uint32_t base, size, next_slot, next_sequence, records, pending, head_slot;
    int cache;
} gw_store_t;
/* 扫描分区重建状态，不做整片擦除。成功0，参数错误-1，读取错误-2；失败后不能继续使用该实例。 */
int gw_store_open(gw_store_t *s, const gw_flash_t *io, uint32_t base, uint32_t size, int cache);
/* 追加一条：正文写入并读回后才提交magic。成功0，参数错误-1，I/O错误-2，空间不足-3。 */
int gw_store_append(gw_store_t *s, const char *text, size_t length, uint32_t timestamp);
/* peek只读出，不消费；发送完成后由调用者另行ack。 */
/*
 * 复制最早待发记录而不消费；返回正文长度、0表示空、负数表示错误。
 * capacity 至少要比正文多1字节，供函数追加字符串结束符。
 */
int gw_store_peek(gw_store_t *s, char *text, size_t capacity, uint32_t *sequence);
/* 按 sequence 确认当前队头；成功0、参数/记录不符-1、I/O或重选队头错误-2。 */
int gw_store_ack(gw_store_t *s, uint32_t sequence);
/* 读取最近日志，index=0表示最新一条；返回正文长度、0表示无记录、负数表示错误。 */
int gw_store_recent(gw_store_t *s, uint32_t index, char *text, size_t capacity,
                    uint32_t *timestamp);
#endif
