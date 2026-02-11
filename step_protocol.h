/* step_protocol.h - STEP协议定义 */
#ifndef STEP_PROTOCOL_H
#define STEP_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* STEP协议Tag定义 */
#define STEP_START_TAG      0x53544550  /* "STEP" ASCII */
#define STEP_MARKET_DATA    0x4D444154  /* "MDAT" - 行情数据 */
#define STEP_ORDER_DATA     0x4F524445  /* "ORDE" - 订单数据 */
#define STEP_TRADE_DATA     0x54524144  /* "TRAD" - 成交数据 */

/* STEP协议头结构 */
#pragma pack(push, 1)
typedef struct {
    uint32_t    start_tag;      /* 起始标识: 0x53544550 */
    uint32_t    msg_type;       /* 消息类型 */
    uint32_t    msg_length;     /* 消息总长度(包含头部) */
    uint64_t    timestamp;      /* 时间戳(纳秒) */
    uint32_t    seq_num;        /* 序列号 */
    uint16_t    version;        /* 协议版本 */
    uint8_t     flags;          /* 标志位 */
    uint8_t     reserved;       /* 保留字段 */
} step_header_t;

/* STEP消息尾部校验 */
typedef struct {
    uint32_t    checksum;       /* CRC32校验 */
} step_trailer_t;
#pragma pack(pop)

/* FAST协议相关定义 */
#define FAST_TEMPLATE_ID    1   /* 行情模板ID */
#define FAST_PRESENCE_MAP   0x80 /* 字段存在标志掩码 */

/* FAST字段类型 */
typedef enum {
    FAST_UINT32,
    FAST_UINT64,
    FAST_DECIMAL,
    FAST_STRING,
    FAST_TIMESTAMP
} fast_field_type_t;

/* FAST字段定义 */
typedef struct {
    uint32_t    field_id;
    const char  *field_name;
    fast_field_type_t type;
} fast_field_def_t;

/* CSV输出缓冲区 */
#define MAX_CSV_LINE_LEN    4096
#define MAX_FIELDS_PER_MSG  128

#endif /* STEP_PROTOCOL_H */