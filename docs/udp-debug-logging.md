# UDP调试日志说明

> 本文档说明了为调试UDP通信问题而添加的详细日志功能。

## 概述

为了解决"服务器总是收不到UDP包"的问题，我们在UDP通信的关键节点添加了详细的日志打印，包括：
- UDP包发送状态
- UDP包接收状态
- UDP连接建立和关闭
- 音频包加密和解密过程
- 错误和异常情况

## 添加的日志位置

### 1. MQTT命令发送日志

#### 1.1 Start Listening命令日志
```cpp
// 在 protocol.cc 中
ESP_LOGI(TAG, "Sending start listening command - mode: %s, session_id: %s", 
         mode == kListeningModeRealtime ? "realtime" : 
         mode == kListeningModeAutoStop ? "auto" : "manual", 
         session_id_.c_str());

// 在 mqtt_protocol.cc 中
ESP_LOGI(TAG, "MQTT message sent successfully - topic: %s, message: %s", 
         publish_topic_.c_str(), text.c_str());
```

#### 1.2 Stop Listening命令日志
```cpp
// 在 mqtt_protocol.cc 中
ESP_LOGI(TAG, "MQTT message sent successfully - topic: %s, message: %s", 
         publish_topic_.c_str(), text.c_str());
```

### 2. UDP包发送日志 (`SendAudio` 函数)

```cpp
// 发送前状态
ESP_LOGI(TAG, "Preparing to send audio packet - timestamp: %lu, payload_size: %u, sequence: %lu", 
         packet->timestamp, (unsigned int)packet->payload.size(), local_sequence_ + 1);

// 加密成功
ESP_LOGI(TAG, "Audio packet encrypted successfully - encrypted_size: %u, nonce_size: %u", 
         (unsigned int)encrypted.size(), (unsigned int)aes_nonce_.size());

// 发送结果
ESP_LOGI(TAG, "UDP audio packet sent successfully - bytes_sent: %d, sequence: %lu, timestamp: %lu, server: %s:%d", 
         send_result, local_sequence_, packet->timestamp, udp_server_.c_str(), udp_port_);

// 发送失败
ESP_LOGE(TAG, "Failed to send UDP audio packet - send_result: %d, sequence: %lu, timestamp: %lu", 
         send_result, local_sequence_, packet->timestamp);
```

### 2. UDP连接建立日志 (`OpenAudioChannel` 函数)

```cpp
// 连接建立
ESP_LOGI(TAG, "Connecting to UDP server: %s:%d", udp_server_.c_str(), udp_port_);
ESP_LOGI(TAG, "UDP connection initiated to %s:%d", udp_server_.c_str(), udp_port_);
```

### 3. UDP包接收日志 (UDP消息回调)

```cpp
// 包接收
ESP_LOGI(TAG, "UDP packet received - size: %u bytes", (unsigned int)data.size());

// 包解析
ESP_LOGI(TAG, "UDP packet parsed - timestamp: %lu, sequence: %lu", timestamp, sequence);

// 解密成功
ESP_LOGI(TAG, "UDP packet decrypted successfully - decrypted_size: %u, sample_rate: %d, frame_duration: %d", 
         (unsigned int)decrypted_size, packet->sample_rate, packet->frame_duration);

// 传递给音频服务
ESP_LOGI(TAG, "Audio packet passed to audio service");
```

### 4. UDP通道关闭日志 (`CloseAudioChannel` 函数)

```cpp
ESP_LOGI(TAG, "Closing UDP audio channel");
ESP_LOGI(TAG, "UDP channel closed - server: %s:%d", udp_server_.c_str(), udp_port_);
```

### 5. UDP通道状态检查日志 (`IsAudioChannelOpened` 函数)

```cpp
ESP_LOGD(TAG, "Audio channel status check - udp: %s, error: %s, timeout: %s, result: %s", 
         udp_ != nullptr ? "yes" : "no", 
         error_occurred_ ? "yes" : "no", 
         IsTimeout() ? "yes" : "no",
         is_open ? "open" : "closed");
```

## 日志级别说明

- **ESP_LOGI**: 重要的状态信息，如UDP包发送成功、连接建立等
- **ESP_LOGW**: 警告信息，如UDP通道不可用等
- **ESP_LOGE**: 错误信息，如发送失败、解密失败等
- **ESP_LOGD**: 调试信息，如状态检查等

## 使用方法

### 1. 编译和烧录
重新编译项目并烧录到设备，确保日志级别设置为INFO或更低：

```bash
# 在sdkconfig中设置
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

### 2. 查看日志
通过串口或网络查看设备日志输出：

```bash
# 串口查看
idf.py monitor

# 或者通过ESP-IDF的日志系统查看
```

### 3. 调试步骤

1. **启动设备并建立连接**
   - 查看MQTT连接日志
   - 查看UDP通道建立日志

2. **发送音频数据**
   - 查看音频包准备日志
   - 查看加密过程日志
   - 查看UDP发送结果日志

3. **接收音频数据**
   - 查看UDP包接收日志
   - 查看包解析日志
   - 查看解密过程日志

## 常见问题排查

### 1. 服务器收不到UDP包

**检查日志：**
- 是否显示"UDP audio packet sent successfully"
- 发送的字节数是否正确
- 目标服务器地址和端口是否正确

**可能原因：**
- 防火墙阻止
- 网络路由问题
- 服务器UDP端口未开放

### 2. UDP连接建立失败

**检查日志：**
- 是否显示"Connecting to UDP server"
- 服务器地址和端口是否正确
- 网络连接是否正常

### 3. 音频包处理异常

**检查日志：**
- 包大小是否合理
- 序列号是否连续
- 解密是否成功

## 日志示例

### 正常发送流程
```
I (1234) MQTT: Preparing to send audio packet - timestamp: 1234567890, payload_size: 120, sequence: 1
I (1235) MQTT: Audio packet encrypted successfully - encrypted_size: 144, nonce_size: 24
I (1236) MQTT: UDP audio packet sent successfully - bytes_sent: 144, sequence: 1, timestamp: 1234567890, server: 192.168.1.100:8888
```

### 正常接收流程
```
I (2345) MQTT: UDP packet received - size: 144 bytes
I (2346) MQTT: UDP packet parsed - timestamp: 1234567890, sequence: 1
I (2347) MQTT: UDP packet decrypted successfully - decrypted_size: 120, sample_rate: 16000, frame_duration: 60
I (2348) MQTT: Audio packet passed to audio service
```

## 注意事项

1. **日志级别**: 确保日志级别设置正确，否则可能看不到某些日志
2. **性能影响**: 大量日志可能影响性能，生产环境建议调整日志级别
3. **存储空间**: 长时间运行可能产生大量日志，注意存储空间管理
4. **敏感信息**: 日志中可能包含网络地址等敏感信息，注意安全

## 后续优化建议

1. **日志轮转**: 实现日志文件轮转，避免存储空间耗尽
2. **远程日志**: 实现日志远程传输，方便远程调试
3. **性能监控**: 添加UDP通信性能统计
4. **错误统计**: 统计UDP包丢失率、重传率等指标
