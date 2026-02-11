/* step_fast_parser.c - 多线程解析引擎 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "step_protocol.h"

/* 全局配置 */
typedef struct {
    char        input_file[256];
    char        output_prefix[256];
    int         num_threads;
    size_t      chunk_size;     /* 每个线程处理的块大小 */
    int         verbose;
} parser_config_t;

/* 线程上下文 */
typedef struct {
    pthread_t       thread_id;
    int             thread_idx;
    const uint8_t   *data_start;
    const uint8_t   *data_end;
    FILE            **csv_files;    /* 每个消息类型一个CSV文件 */
    pthread_mutex_t *file_mutexes;  /* 文件写锁 */
    
    /* 统计信息 */
    size_t          bytes_processed;
    size_t          messages_parsed;
    size_t          errors_found;
    
    /* 指向全局配置 */
    parser_config_t *config;
} thread_context_t;

/* FAST模板定义 - 假设的行情消息模板 */
static fast_field_def_t market_data_template[] = {
    {1, "Symbol", FAST_STRING},
    {2, "BidPrice", FAST_DECIMAL},
    {3, "BidSize", FAST_UINT32},
    {4, "AskPrice", FAST_DECIMAL},
    {5, "AskSize", FAST_UINT32},
    {6, "LastPrice", FAST_DECIMAL},
    {7, "LastSize", FAST_UINT32},
    {8, "Volume", FAST_UINT64},
    {9, "Timestamp", FAST_TIMESTAMP},
    {10, "Exchange", FAST_STRING},
    {0, NULL, 0}  /* 结束标记 */
};

/* 解析FAST消息到CSV行 */
static int parse_fast_message(const uint8_t *data, size_t len, 
                             char *csv_buffer, size_t buffer_size) {
    /* FAST解析状态机 */
    const uint8_t *ptr = data;
    const uint8_t *end = data + len;
    char *csv_ptr = csv_buffer;
    int field_count = 0;
    
    /* 检查模板ID */
    if (ptr >= end) return -1;
    uint8_t template_id = *ptr++;
    
    if (template_id != FAST_TEMPLATE_ID) {
        return -1;  /* 不支持的模板 */
    }
    
    /* 解析存在位图 */
    if (ptr >= end) return -1;
    uint8_t presence_map = *ptr++;
    
    /* 根据模板解析字段 */
    fast_field_def_t *field = market_data_template;
    while (field->field_name != NULL) {
        int field_present = (presence_map >> (7 - (field_count % 8))) & 0x01;
        
        if (field_present) {
            switch (field->type) {
                case FAST_UINT32: {
                    /* 解码变长整数 */
                    uint32_t value = 0;
                    while (ptr < end && (*ptr & 0x80)) {
                        value = (value << 7) | (*ptr & 0x7F);
                        ptr++;
                    }
                    if (ptr < end) {
                        value = (value << 7) | *ptr++;
                    }
                    csv_ptr += snprintf(csv_ptr, buffer_size - (csv_ptr - csv_buffer),
                                       "%s%u", field_count > 0 ? "," : "", value);
                    break;
                }
                case FAST_STRING: {
                    /* 解码字符串长度 */
                    uint32_t str_len = 0;
                    while (ptr < end && (*ptr & 0x80)) {
                        str_len = (str_len << 7) | (*ptr & 0x7F);
                        ptr++;
                    }
                    if (ptr < end) {
                        str_len = (str_len << 7) | *ptr++;
                    }
                    
                    /* 复制字符串 */
                    if (ptr + str_len <= end) {
                        csv_ptr += snprintf(csv_ptr, buffer_size - (csv_ptr - csv_buffer),
                                           "%s\"%.*s\"", field_count > 0 ? "," : "", 
                                           str_len, ptr);
                        ptr += str_len;
                    }
                    break;
                }
                case FAST_DECIMAL: {
                    /* 简化处理为字符串 */
                    uint32_t mantissa = 0;
                    int8_t exponent = 0;
                    // ... 实际解码逻辑
                    csv_ptr += snprintf(csv_ptr, buffer_size - (csv_ptr - csv_buffer),
                                       "%s%.6f", field_count > 0 ? "," : "", 0.0);
                    break;
                }
                default:
                    break;
            }
            field_count++;
        }
        field++;
    }
    
    return 0;
}

/* 线程工作函数 */
static void *parse_thread_func(void *arg) {
    thread_context_t *ctx = (thread_context_t *)arg;
    const uint8_t *ptr = ctx->data_start;
    
    printf("Thread %d: processing %ld bytes\n", 
           ctx->thread_idx, ctx->data_end - ctx->data_start);
    
    while (ptr < ctx->data_end) {
        /* 查找STEP起始标记 */
        while (ptr < ctx->data_end - sizeof(step_header_t) + 1) {
            if (*(uint32_t*)ptr == STEP_START_TAG) {
                break;
            }
            ptr++;
        }
        
        if (ptr >= ctx->data_end - sizeof(step_header_t)) {
            break;  /* 没有完整的消息头 */
        }
        
        /* 解析STEP头 */
        step_header_t *header = (step_header_t *)ptr;
        
        /* 验证消息完整性 */
        if (ptr + header->msg_length > ctx->data_end) {
            /* 消息不完整，可能跨块 */
            break;
        }
        
        /* 验证校验和 */
        step_trailer_t *trailer = (step_trailer_t *)(ptr + header->msg_length - sizeof(step_trailer_t));
        // 实际应该计算CRC32并验证
        // uint32_t calc_crc = calculate_crc32(ptr, header->msg_length - sizeof(step_trailer_t));
        
        /* 提取FAST payload */
        size_t fast_offset = sizeof(step_header_t);
        size_t fast_length = header->msg_length - sizeof(step_header_t) - sizeof(step_trailer_t);
        
        if (fast_length > 0 && header->msg_type == STEP_MARKET_DATA) {
            /* 解析FAST消息 */
            char csv_line[MAX_CSV_LINE_LEN];
            if (parse_fast_message(ptr + fast_offset, fast_length, 
                                  csv_line, sizeof(csv_line)) == 0) {
                
                /* 写CSV文件 */
                pthread_mutex_lock(&ctx->file_mutexes[0]);  /* 假设索引0是行情CSV */
                fprintf(ctx->csv_files[0], "%s\n", csv_line);
                pthread_mutex_unlock(&ctx->file_mutexes[0]);
                
                ctx->messages_parsed++;
            } else {
                ctx->errors_found++;
            }
        }
        
        ctx->bytes_processed += header->msg_length;
        ptr += header->msg_length;
    }
    
    return NULL;
}

/* 内存映射文件并创建线程 */
int parse_step_file(parser_config_t *config) {
    int fd = open(config->input_file, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file: %s\n", strerror(errno));
        return -1;
    }
    
    /* 获取文件大小 */
    struct stat st;
    fstat(fd, &st);
    size_t file_size = st.st_size;
    
    /* 内存映射 */
    uint8_t *file_data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_data == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    
    close(fd);
    
    /* 创建CSV文件 */
    FILE *csv_files[1];  /* 简化：只创建行情CSV */
    pthread_mutex_t file_mutexes[1];
    
    char csv_filename[256];
    snprintf(csv_filename, sizeof(csv_filename), "%s_market_data.csv", 
             config->output_prefix);
    
    csv_files[0] = fopen(csv_filename, "w");
    if (!csv_files[0]) {
        fprintf(stderr, "Failed to create CSV file\n");
        munmap(file_data, file_size);
        return -1;
    }
    
    /* 写CSV头 */
    fprintf(csv_files[0], "Symbol,BidPrice,BidSize,AskPrice,AskSize,LastPrice,LastSize,Volume,Timestamp,Exchange\n");
    
    pthread_mutex_init(&file_mutexes[0], NULL);
    
    /* 计算每个线程处理的数据块 */
    size_t chunk_size = file_size / config->num_threads;
    thread_context_t *threads = calloc(config->num_threads, sizeof(thread_context_t));
    
    /* 创建线程 */
    for (int i = 0; i < config->num_threads; i++) {
        threads[i].thread_idx = i;
        threads[i].config = config;
        threads[i].csv_files = csv_files;
        threads[i].file_mutexes = file_mutexes;
        
        /* 分配数据块 */
        threads[i].data_start = file_data + i * chunk_size;
        
        /* 调整起始点，避免截断消息 */
        if (i > 0) {
            /* 向前查找STEP起始标记 */
            const uint8_t *adjust_ptr = threads[i].data_start;
            const uint8_t *search_start = adjust_ptr - sizeof(step_header_t) * 2;
            if (search_start < file_data) search_start = file_data;
            
            while (adjust_ptr >= search_start) {
                if (*(uint32_t*)adjust_ptr == STEP_START_TAG) {
                    threads[i].data_start = adjust_ptr;
                    break;
                }
                adjust_ptr--;
            }
        }
        
        /* 设置结束点 */
        if (i == config->num_threads - 1) {
            threads[i].data_end = file_data + file_size;
        } else {
            threads[i].data_end = file_data + (i + 1) * chunk_size;
            
            /* 向后调整到完整消息边界 */
            const uint8_t *ptr = threads[i].data_end;
            const uint8_t *search_end = ptr + sizeof(step_header_t) * 2;
            if (search_end > file_data + file_size) search_end = file_data + file_size;
            
            while (ptr < search_end) {
                if (*(uint32_t*)ptr == STEP_START_TAG) {
                    threads[i].data_end = ptr;
                    break;
                }
                ptr++;
            }
        }
        
        /* 创建线程 */
        pthread_create(&threads[i].thread_id, NULL, parse_thread_func, &threads[i]);
    }
    
    /* 等待线程完成 */
    size_t total_bytes = 0;
    size_t total_messages = 0;
    
    for (int i = 0; i < config->num_threads; i++) {
        pthread_join(threads[i].thread_id, NULL);
        
        total_bytes += threads[i].bytes_processed;
        total_messages += threads[i].messages_parsed;
        
        if (config->verbose) {
            printf("Thread %d: processed %ld bytes, %ld messages, %ld errors\n",
                   i, threads[i].bytes_processed, 
                   threads[i].messages_parsed, threads[i].errors_found);
        }
    }
    
    printf("\nTotal: %ld bytes, %ld messages parsed\n", total_bytes, total_messages);
    
    /* 清理资源 */
    for (int i = 0; i < 1; i++) {
        fclose(csv_files[i]);
        pthread_mutex_destroy(&file_mutexes[i]);
    }
    
    munmap(file_data, file_size);
    free(threads);
    
    return 0;
}

/* 主函数 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_prefix> [num_threads]\n", argv[0]);
        return 1;
    }
    
    parser_config_t config = {
        .num_threads = 4,  /* 默认4线程 */
        .chunk_size = 64 * 1024 * 1024,  /* 64MB块 */
        .verbose = 1
    };
    
    strncpy(config.input_file, argv[1], sizeof(config.input_file) - 1);
    strncpy(config.output_prefix, argv[2], sizeof(config.output_prefix) - 1);
    
    if (argc >= 4) {
        config.num_threads = atoi(argv[3]);
        if (config.num_threads <= 0) config.num_threads = 4;
    }
    
    printf("Starting STEP/FAST parser:\n");
    printf("  Input file: %s\n", config.input_file);
    printf("  Output prefix: %s\n", config.output_prefix);
    printf("  Threads: %d\n", config.num_threads);
    
    return parse_step_file(&config);
}