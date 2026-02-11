#!/bin/bash
# test_runner.sh - 测试脚本

echo "=== STEP/FAST Parser Test Suite ==="

# 1. 编译程序
echo "1. Compiling programs..."
gcc -Wall -O3 -o step_fast_data_generator step_fast_data_generator.c
gcc -Wall -O3 -pthread -D_GNU_SOURCE -o step_fast_parser step_fast_parser.c

# 2. 生成测试数据
echo -e "\n2. Generating test data..."
./step_fast_data_generator test_data.bin 200000 50

echo -e "\n3. Testing with different thread counts..."

# 3. 测试不同线程数
for threads in 1 2 4 8; do
    echo -e "\n   Testing with $threads threads..."
    time ./step_fast_parser test_data.bin output_${threads}thread $threads
    
    # 验证输出
    if [ -f "output_${threads}thread_market_data.csv" ]; then
        line_count=$(wc -l < "output_${threads}thread_market_data.csv")
        echo "   Output lines: $((line_count - 1))"  # 减去标题行
    fi
done

# 4. 验证数据一致性
echo -e "\n4. Verifying data consistency..."
if [ -f "output_1thread_market_data.csv" ] && [ -f "output_8thread_market_data.csv" ]; then
    # 比较文件大小
    size1=$(stat -f%z "output_1thread_market_data.csv" 2>/dev/null || stat -c%s "output_1thread_market_data.csv")
    size8=$(stat -f%z "output_8thread_market_data.csv" 2>/dev/null || stat -c%s "output_8thread_market_data.csv")
    
    echo "   Single-thread output size: $size1 bytes"
    echo "   8-thread output size: $size8 bytes"
    
    if [ "$size1" -eq "$size8" ]; then
        echo "   ✓ Output sizes match"
    else
        echo "   ✗ Output sizes differ!"
    fi
    
    # 比较前100行（跳过标题）
    echo -e "\n   Comparing first 100 data lines..."
    tail -n +2 output_1thread_market_data.csv | head -100 > /tmp/output1_100.csv
    tail -n +2 output_8thread_market_data.csv | head -100 > /tmp/output8_100.csv
    
    diff_result=$(diff /tmp/output1_100.csv /tmp/output8_100.csv)
    if [ -z "$diff_result" ]; then
        echo "   ✓ First 100 data lines match"
    else
        echo "   ✗ First 100 data lines differ!"
        echo "$diff_result" | head -20
    fi
fi

# 5. 性能测试
echo -e "\n5. Performance test with large file..."
echo "   Generating 500MB test file..."
./step_fast_data_generator large_test.bin 1000000 500

echo -e "\n   Parsing with 8 threads..."
time ./step_fast_parser large_test.bin large_output 8

echo -e "\n=== Test Complete ==="