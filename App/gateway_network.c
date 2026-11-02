/*
 * 网络任务：STM32 通过 USART3 的 AT 指令控制 ESP8266，建立 TCP，再发送 MQTT 3.1.1 报文。
 * 调用关系：gateway_network_task -> connect_modem / flush_cache / flush_live -> send_packet。
 * AT 文本与 TCP 二进制载荷的分流在 gateway_at.c 完成；底层 UART 在 Port/gateway_board.c。
 * 本工程使用普通单连接模式，不使用透传；MQTT 发布为 QoS0。
 */
#include "gateway.h"
#include "gateway_board.h"
#include "gateway_protocol.h"
#include "gateway_at.h"
#include <stdio.h>
#include <string.h>
static gw_at_parser_t modem;
/* 从 UART 环形缓冲取走已有字节，交给 AT/MQTT 解析器，并更新网络任务心跳。 */
static void pump(void)
{
    uint8_t b[128];
    int n;
    do
    {
        n = gateway_wifi_read(b, sizeof(b));
        if (n > 0)
            gw_at_feed(&modem, b, (size_t)n);
    } while (n == (int)sizeof(b));
    g_gateway_diag.heartbeat_network = HAL_GetTick();
}
/*
 * 等待指定事件，timeout 单位为毫秒；每轮休眠 5ms，不连续空转。
 * 返回 0 表示指定事件出现；-1 表示超时、失败或连接关闭。
 */
static int wait_for(uint32_t event, uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    do
    {
        pump();
        if (modem.events & (GW_AT_FAIL | GW_AT_CLOSED))
            return -1;
        if (modem.events & event)
        {
            modem.events &= ~event;
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    } while (HAL_GetTick() - start < timeout);
    return -1;
}
/* 发送一条 AT 命令前清除相关旧事件，避免把上一条命令的结果误当成本次结果。 */
static int command(const char *text, uint32_t event, uint32_t timeout)
{
    modem.events &= ~(event | GW_AT_FAIL | GW_AT_CLOSED);
    if (gateway_wifi_send(text, strlen(text)))
        return -1;
    return wait_for(event, timeout);
}
/*
 * 发送 TCP 数据的三步：CIPSEND=长度 -> 等待 > 提示 -> 发送原始字节并等待 SEND OK。
 * SEND OK 是模块传输反馈，不是 MQTT 服务器的持久化确认。
 */
static int send_packet(const uint8_t *p, size_t n)
{
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", (unsigned)n);
    if (command(cmd, GW_AT_PROMPT, 1500))
        return -1;
    modem.events &= ~GW_AT_SEND_OK;
    if (gateway_wifi_send(p, n))
        return -1;
    return wait_for(GW_AT_SEND_OK, 4000);
}
/*
 * 检查插入 AT 命令的字符串：最多 100 字节，不允许双引号、反斜杠和控制字符。
 * 本实现选择拒绝这些字符，未实现 AT 参数转义。
 */
static int safe_at_string(const char *p)
{
    size_t n = 0;
    while (p[n])
    {
        if (p[n] == '"' || p[n] == '\\' || (unsigned char)p[n] < 32 || n >= 100)
            return 0;
        n++;
    }
    return 1;
}
/*
 * 依次完成模块探测、连接 Wi-Fi、连接 TCP、MQTT CONNECT/CONNACK。
 * 返回 0 才把网络状态置 3；返回 -1 后由主循环按 30 秒间隔重试。
 */
static int connect_modem(void)
{
    /* 重新连接时清空解析状态和遗留 UART 数据，避免旧响应混入新会话。 */
    char cmd[256];
    uint8_t packet[180];
    int n;
    gw_at_reset(&modem);
    gateway_wifi_discard();
    g_gateway_diag.network_state = 1;
    if (!safe_at_string(GW_WIFI_SSID) || !safe_at_string(GW_WIFI_PASSWORD) ||
        !safe_at_string(GW_MQTT_HOST))
        return -1;
    if (command("AT\r\n", GW_AT_OK, 1000) || command("ATE0\r\n", GW_AT_OK, 1000) ||
        command("AT+CWMODE=1\r\n", GW_AT_OK, 1000))
        return -1;
    /* 旧连接可能本来就不存在，因此允许这一步返回 ERROR，后续命令会重新清事件。 */
    command("AT+CIPCLOSE\r\n", GW_AT_OK, 500);
    /* CIPMUX=0：单连接；CIPMODE=0：普通发送模式，接收数据格式为 +IPD,长度:载荷。 */
    if (command("AT+CIPMUX=0\r\n", GW_AT_OK, 1000) || command("AT+CIPMODE=0\r\n", GW_AT_OK, 1000))
        return -1;
    /* 拼接 Wi-Fi 配置。命令含密码，因此不将命令内容写入日志。 */
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", GW_WIFI_SSID, GW_WIFI_PASSWORD);
    if (command(cmd, GW_AT_OK, 15000))
        return -1;
    g_gateway_diag.network_state = 2;
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n", GW_MQTT_HOST, GW_MQTT_PORT);
    if (command(cmd, GW_AT_OK, 10000))
        return -1;
    /* 发送 MQTT CONNECT 后，还要等服务器的 CONNACK；TCP 建立不等于 MQTT 登录成功。 */
    n = gw_mqtt_connect(packet, sizeof(packet), GW_MQTT_CLIENT_ID, 30);
    modem.events &= ~GW_AT_CONNACK;
    if (n < 0 || send_packet(packet, (size_t)n) || wait_for(GW_AT_CONNACK, 4000))
        return -1;
    g_gateway_diag.network_state = 3;
    gateway_log("MQTT connected; QoS0 transport acknowledgements only");
    return 0;
}
/*
 * 尝试补传一条 Flash 缓存。返回 1=发送并标记成功，0=本轮无可推进记录，-1=发送失败。
 * 先 peek，发送完成后再 ack；网络等待期间不持有 Flash 互斥锁。
 */
static int flush_cache(void)
{
    char json[GW_RECORD_PAYLOAD + 1];
    uint8_t packet[512];
    uint32_t sequence;
    int len, n;
    if (!g_gateway_diag.storage_ready)
        return 0;
    if (xSemaphoreTake(g_storage_mutex, pdMS_TO_TICKS(500)) != pdTRUE)
        return 0;
    /* 仅复制队头，读出不会消费；复制完成立刻解锁，让采集任务仍可保存新样本。 */
    len = gw_store_peek(&g_cache_store, json, sizeof(json), &sequence);
    xSemaphoreGive(g_storage_mutex);
    if (len <= 0)
        return 0; /* 本地存储错误不妨碍后续尝试内存实时发送。 */
    n = gw_mqtt_publish(packet, sizeof(packet), GW_MQTT_TOPIC, (const uint8_t *)json, (size_t)len);
    if (n < 0 || send_packet(packet, (size_t)n))
        return -1;
    /* SEND OK仅确认ESP传输反馈，无法证明云端已持久保存。 */
    if (xSemaphoreTake(g_storage_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return -1;
    /* 仅在 SEND OK 后标记记录已发。若发送结果不确定，记录保留，可能重发。 */
    n = gw_store_ack(&g_cache_store, sequence);
    g_gateway_diag.cache_pending = g_cache_store.pending;
    xSemaphoreGive(g_storage_mutex);
    return n < 0 ? 0 : 1;
}
/*
 * Flash 保存失败时的内存发送路径。static 变量保留一个正在重试的样本。
 * 成功前不丢弃这个发送槽；此外队列只保留一个最新等待样本，无法保存全部历史。
 */
static int flush_live(void)
{
    static gw_sample_t sample;
    static int pending;
    char temp[24] = "null", humi[24] = "null", light[24] = "null", json[160];
    uint8_t packet[512];
    int n, length;
    /* 先取入本任务的重试槽；发送失败时保留 pending，下次连接恢复后继续尝试。 */
    if (!pending)
    {
        if (xQueueReceive(g_sample_queue, &sample, 0) != pdTRUE)
            return 0;
        pending = 1;
    }
    if (sample.temperature_valid)
    {
        snprintf(temp, sizeof(temp), "%.1f", sample.temperature);
        snprintf(humi, sizeof(humi), "%.1f", sample.humidity);
    }
    if (sample.light_valid)
        snprintf(light, sizeof(light), "%.0f", sample.light);
    length = snprintf(json, sizeof(json), "{\"temperature\":%s,\"shidu\":%s,\"light\":%s}", temp,
                      humi, light);
    if (length <= 0 || length >= (int)sizeof(json))
        return -1;
    n = gw_mqtt_publish(packet, sizeof(packet), GW_MQTT_TOPIC, (const uint8_t *)json,
                        (size_t)length);
    if (n < 0 || send_packet(packet, (size_t)n))
        return -1;
    pending = 0;
    return 1;
}
/* 网络任务主循环：处理串口、按间隔重连、补传缓存、发送降级样本、维持 MQTT 心跳。 */
void gateway_network_task(void *arg)
{
    /* 默认 SSID 或 client ID 为空时不连接；0xC0 0x00 是 MQTT PINGREQ 心跳报文。 */
    uint32_t retry = 0, ping = 0;
    uint8_t request[2] = {0xc0, 0};
    int configured = GW_WIFI_SSID[0] && GW_MQTT_CLIENT_ID[0], i, r;
    (void)arg;
    gateway_log(configured ? "Wi-Fi configured; probing ESP8266 on USART3"
                           : "Wi-Fi not configured; no external module required to run UI");
    for (;;)
    {
        pump();
        if (configured && g_gateway_diag.network_state != 3 &&
            (retry == 0 || HAL_GetTick() - retry >= 30000))
        {
            if (connect_modem())
            {
                g_gateway_diag.network_state = 1;
                gateway_log("Wi-Fi/MQTT unavailable; records retained for retry");
            }
            else
                ping = HAL_GetTick();
            retry = HAL_GetTick();
        }
        if (g_gateway_diag.network_state == 3)
        {
            if (modem.events & (GW_AT_CLOSED | GW_AT_FAIL))
            {
                g_gateway_diag.network_state = 1;
                retry = HAL_GetTick();
                gateway_log("Network disconnected; retaining unsent records");
            }
            else
            {
                /* 每轮最多补传 5 条，记录之间让出 CPU；随后也给内存实时样本发送机会。 */
                for (i = 0; i < 5; i++)
                {
                    r = flush_cache();
                    if (r < 0)
                    {
                        g_gateway_diag.network_state = 1;
                        retry = HAL_GetTick();
                        break;
                    }
                    if (!r)
                        break;
                    pump();
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                if (g_gateway_diag.network_state == 3 && flush_live() < 0)
                {
                    g_gateway_diag.network_state = 1;
                    retry = HAL_GetTick();
                }
                /* 每 10 秒发送 MQTT 心跳；等不到 PINGRESP 时回到重连流程。 */
                if (g_gateway_diag.network_state == 3 && HAL_GetTick() - ping >= 10000)
                {
                    modem.events &= ~GW_AT_PONG;
                    if (send_packet(request, 2) || wait_for(GW_AT_PONG, 2000))
                    {
                        g_gateway_diag.network_state = 1;
                        retry = HAL_GetTick();
                        gateway_log("MQTT heartbeat timed out");
                    }
                    ping = HAL_GetTick();
                }
            }
        }
        g_gateway_diag.heartbeat_network = HAL_GetTick();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
