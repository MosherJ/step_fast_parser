/* step_fast_data_generator.c - 生成测试数据集 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

/* 协议定义 (与解析器一致) */
#pragma pack(push, 1)
typedef struct {
    uint32_t    start_tag;      /* 起始标识: 0x53544550 */
    uint32_t    msg_type;       /* 消息类型 */
    uint32_t    msg_length;     /* 消息总长度 */
    uint64_t    timestamp;      /* 时间戳 */
    uint32_t    seq_num;        /* 序列号 */
    uint16_t    version;        /* 协议版本 */
    uint8_t     flags;          /* 标志位 */
    uint8_t     reserved;       /* 保留字段 */
} step_header_t;

typedef struct {
    uint32_t    checksum;       /* CRC32校验 */
} step_trailer_t;
#pragma pack(pop)

#define STEP_START_TAG      0x53544550
#define STEP_MARKET_DATA    0x4D444154
#define MAX_SYMBOLS         100
#define MAX_PAYLOAD_SIZE    1024

/* 简单的CRC32计算 (简化版) */
uint32_t simple_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

/* 生成FAST协议payload (简化版) */
size_t generate_fast_payload(uint8_t *buffer, uint32_t seq_num) {
    uint8_t *ptr = buffer;
    
    /* 模板ID = 1 */
    *ptr++ = 0x01;  
    
    /* 存在位图: 假设所有字段都存在 (0xFF) */
    *ptr++ = 0xFF;  
    
    /* 字段1: 股票代码 (字符串) */
    const char *symbols[] = {"AAPL", "GOOGL", "MSFT", "AMZN", "TSLA", 
                           "FB", "NVDA", "JPM", "V", "WMT"};
    const char *symbol = symbols[seq_num % 10];
    uint8_t symbol_len = strlen(symbol);
    
    /* 编码字符串长度 */
    if (symbol_len < 128) {
        *ptr++ = symbol_len;
    } else {
        *ptr++ = 0x80 | (symbol_len >> 7);
        *ptr++ = symbol_len & 0x7F;
    }
    
    /* 复制字符串 */
    memcpy(ptr, symbol, symbol_len);
    ptr += symbol_len;
    
    /* 字段2: 买价 (模拟十进制) */
    double bid_price = 100.0 + (seq_num % 100) * 0.5 + (rand() % 100) * 0.01;
    int32_t price_int = (int32_t)(bid_price * 10000);
    
    /* 编码变长整数 */
    uint32_t temp = price_int;
    if (temp >= 0x80) {
        if (temp >= 0x4000) {
            if (temp >= 0x200000) {
                *ptr++ = 0x80 | ((temp >> 21) & 0x7F);
            }
            *ptr++ = 0x80 | ((temp >> 14) & 0x7F);
        }
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    /* 字段3: 买量 */
    uint32_t bid_size = 100 + (rand() % 1000);
    temp = bid_size;
    if (temp >= 0x80) {
        if (temp >= 0x4000) {
            *ptr++ = 0x80 | ((temp >> 14) & 0x7F);
        }
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    /* 字段4: 卖价 */
    double ask_price = bid_price + 0.01 + (rand() % 10) * 0.01;
    price_int = (int32_t)(ask_price * 10000);
    
    temp = price_int;
    if (temp >= 0x80) {
        if (temp >= 0x4000) {
            if (temp >= 0x200000) {
                *ptr++ = 0x80 | ((temp >> 21) & 0x7F);
            }
            *ptr++ = 0x80 | ((temp >> 14) & 0x7F);
        }
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    /* 字段5: 卖量 */
    uint32_t ask_size = 100 + (rand() % 1000);
    temp = ask_size;
    if (temp >= 0x80) {
        if (temp >= 0x4000) {
            *ptr++ = 0x80 | ((temp >> 14) & 0x7F);
        }
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    /* 字段6: 最后成交价 */
    double last_price = bid_price + (rand() % 3) * 0.01;
    price_int = (int32_t)(last_price * 10000);
    
    temp = price_int;
    if (temp >= 0x80) {
        if (temp >= 0x4000) {
            *ptr++ = 0x80 | ((temp >> 14) & 0x7F);
        }
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    /* 字段7: 最后成交量 */
    uint32_t last_size = 10 + (rand() % 100);
    temp = last_size;
    if (temp >= 0x80) {
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    /* 字段8: 总成交量 */
    uint64_t volume = 1000000 + seq_num * 100 + (rand() % 1000);
    temp = volume;
    if (temp >= 0x80) {
        if (temp >= 0x4000) {
            if (temp >= 0x200000) {
                if (temp >= 0x10000000) {
                    *ptr++ = 0x80 | ((temp >> 28) & 0x7F);
                }
                *ptr++ = 0x80 | ((temp >> 21) & 0x7F);
            }
            *ptr++ = 0x80 | ((temp >> 14) & 0x7F);
        }
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    /* 字段9: 时间戳 */
    uint64_t ts = 1609459200000000000ULL + seq_num * 1000000000ULL;
    temp = ts >> 32;
    if (temp >= 0x80) {
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    temp = ts & 0xFFFFFFFF;
    if (temp >= 0x80) {
        if (temp >= 0x4000) {
            if (temp >= 0x200000) {
                *ptr++ = 0x80 | ((temp >> 21) & 0x7F);
            }
            *ptr++ = 0x80 | ((temp >> 14) & 0x7F);
        }
        *ptr++ = 0x80 | ((temp >> 7) & 0x7F);
    }
    *ptr++ = temp & 0x7F;
    
    /* 字段10: 交易所 */
    const char *exchange = "NYSE";
    uint8_t exchange_len = strlen(exchange);
    *ptr++ = exchange_len;
    memcpy(ptr, exchange, exchange_len);
    ptr += exchange_len;
    
    return ptr - buffer;
}

/* 生成STEP消息 */
size_t generate_step_message(uint8_t *buffer, uint32_t seq_num) {
    uint8_t *ptr = buffer;
    step_header_t *header = (step_header_t *)ptr;
    
    /* 填充STEP头 */
    header->start_tag = STEP_START_TAG;
    header->msg_type = STEP_MARKET_DATA;
    header->timestamp = 1609459200000000000ULL + seq_num * 1000000000ULL;
    header->seq_num = seq_num;
    header->version = 1;
    header->flags = 0;
    header->reserved = 0;
    
    /* 生成FAST payload */
    uint8_t fast_payload[MAX_PAYLOAD_SIZE];
    size_t fast_len = generate_fast_payload(fast_payload, seq_num);
    
    /* 计算消息总长度 */
    size_t total_len = sizeof(step_header_t) + fast_len + sizeof(step_trailer_t);
    header->msg_length = total_len;
    
    /* 复制FAST payload */
    memcpy(ptr + sizeof(step_header_t), fast_payload, fast_len);
    
    /* 计算并填充校验和 */
    step_trailer_t *trailer = (step_trailer_t *)(ptr + sizeof(step_header_t) + fast_len);
    trailer->checksum = simple_crc32(buffer, sizeof(step_header_t) + fast_len);
    
    return total_len;
}

/* 主函数 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <output_file> <num_messages> [file_size_MB]\n", argv[0]);
        printf("Example: %s test_data.bin 100000 100\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    int num_messages = atoi(argv[2]);
    int target_size_mb = (argc >= 4) ? atoi(argv[3]) : 100;
    
    if (num_messages <= 0) {
        num_messages = 100000;
    }
    
    printf("Generating test data:\n");
    printf("  Output file: %s\n", filename);
    printf("  Target messages: %d\n", num_messages);
    printf("  Target size: %d MB\n", target_size_mb);
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Failed to open output file");
        return 1;
    }
    
    /* 生成一些无效数据前缀 (模拟真实场景) */
    uint8_t junk_data[128];
    for (int i = 0; i < sizeof(junk_data); i++) {
        junk_data[i] = rand() % 256;
    }
    fwrite(junk_data, 1, sizeof(junk_data), fp);
    
    /* 生成STEP消息 */
    srand(time(NULL));
    uint8_t message_buffer[4096];
    
    size_t total_bytes = 0;
    size_t messages_generated = 0;
    
    for (int i = 0; i < num_messages; i++) {
        /* 偶尔插入一些无效数据 */
        if (i > 0 && i % 1000 == 0) {
            uint8_t noise[16];
            for (int j = 0; j < sizeof(noise); j++) {
                noise[j] = rand() % 256;
            }
            fwrite(noise, 1, sizeof(noise), fp);
            total_bytes += sizeof(noise);
        }
        
        /* 生成STEP消息 */
        size_t msg_len = generate_step_message(message_buffer, i);
        fwrite(message_buffer, 1, msg_len, fp);
        total_bytes += msg_len;
        messages_generated++;
        
        /* 显示进度 */
        if (i > 0 && i % 10000 == 0) {
            printf("  Generated %d messages, %ld MB\n", 
                   i, total_bytes / (1024 * 1024));
        }
        
        /* 如果达到目标大小，停止生成 */
        if (total_bytes >= (size_t)target_size_mb * 1024 * 1024) {
            break;
        }
    }
    
    /* 添加一些尾部无效数据 */
    for (int i = 0; i < 64; i++) {
        junk_data[i] = rand() % 256;
    }
    fwrite(junk_data, 1, 64, fp);
    total_bytes += 64;
    
    fclose(fp);
    
    printf("\nGeneration complete:\n");
    printf("  Total bytes: %ld (%.2f MB)\n", 
           total_bytes, (double)total_bytes / (1024 * 1024));
    printf("  Messages generated: %ld\n", messages_generated);
    printf("  Average message size: %.2f bytes\n", 
           (double)total_bytes / messages_generated);
    
    /* 生成对应的CSV文件用于验证 */
    char csv_filename[256];
    snprintf(csv_filename, sizeof(csv_filename), "%s_expected.csv", filename);
    
    FILE *csv_fp = fopen(csv_filename, "w");
    if (csv_fp) {
        fprintf(csv_fp, "Symbol,BidPrice,BidSize,AskPrice,AskSize,LastPrice,LastSize,Volume,Timestamp,Exchange\n");
        
        /* 生成前100条消息的CSV用于验证 */
        for (int i = 0; i < 100 && i < messages_generated; i++) {
            const char *symbols[] = {"AAPL", "GOOGL", "MSFT", "AMZN", "TSLA", 
                                   "FB", "NVDA", "JPM", "V", "WMT"};
            const char *symbol = symbols[i % 10];
            
            double bid_price = 100.0 + (i % 100) * 0.5 + (rand() % 100) * 0.01;
            double ask_price = bid_price + 0.01 + (rand() % 10) * 0.01;
            double last_price = bid_price + (rand() % 3) * 0.01;
            
            uint32_t bid_size = 100 + (rand() % 1000);
            uint32_t ask_size = 100 + (rand() % 1000);
            uint32_t last_size = 10 + (rand() % 100);
            uint64_t volume = 1000000 + i * 100 + (rand() % 1000);
            uint64_t timestamp = 1609459200000000000ULL + i * 1000000000ULL;
            
            fprintf(csv_fp, "%s,%.2f,%u,%.2f,%u,%.2f,%u,%lu,%lu,NYSE\n",
                   symbol, bid_price, bid_size, ask_price, ask_size,
                   last_price, last_size, volume, timestamp);
        }
        fclose(csv_fp);
        printf("  Verification CSV created: %s\n", csv_filename);
    }
    
    return 0;
}