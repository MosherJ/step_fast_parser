# step_fast_parser
parse a step-fast proto data files

编译程序:
bash
make

运行解析:
bash
./step_fast_parser input.bin output_prefix 8

其中8是线程数，可调整
输出文件:
output_prefix_market_data.csv - 解析后的行情数据


测试脚本ut_test_case.sh
1. 编译所有程序
bash
chmod +x test_runner.sh
./ut_test_case.sh
2. 手动测试步骤
bash
# 1. 生成50MB测试数据
./step_fast_data_generator test.bin 100000 50

# 2. 用4线程解析
./step_fast_parser test.bin output 4

# 3. 查看结果
head -20 output_market_data.csv
wc -l output_market_data.csv

# 4. 验证数据
python verify_results.py output_market_data.csv
3. 预期的输出
text
Generating test data:
  Output file: test.bin
  Target messages: 100000
  Target size: 50 MB

Generation complete:
  Total bytes: 52428800 (50.00 MB)
  Messages generated: 99987
  Average message size: 524.35 bytes
五、测试数据集特征
数据大小：可生成50MB-500MB的测试数据

消息数量：10万到100万条消息

数据结构：

包含正确的STEP头部和尾部

包含FAST编码的payload

插入一些无效数据模拟真实场景

包含CRC32校验和

可验证性：

生成预期的CSV文件用于验证

包含序列号用于顺序检查

时间戳递增便于验证

这个测试数据集能够全面测试解析器的：

✓ 多线程正确性

✓ 边界处理能力

✓ 错误恢复能力

✓ 性能表现

✓ 输出格式正确性
