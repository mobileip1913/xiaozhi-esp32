# 自动停止定时器功能实现

> 本文档记录了为测试UDP通信而实现的3秒自动停止定时器功能。

## 功能概述

为了解决"唤醒以后不会自动停止"的问题，实现了一个3秒自动停止定时器功能。当设备检测到唤醒词并进入监听状态后，会在3秒后自动发送stop信息给服务器，确保服务器能收到一段完整的语音数据。

## 实现细节

### 1. 头文件修改 (`main/application.h`)

#### 1.1 添加必要的头文件
```cpp
#include <functional>  // 用于 std::function
```

#### 1.2 添加定时器句柄
```cpp
esp_timer_handle_t auto_stop_timer_handle_ = nullptr;
```

#### 1.3 添加公共方法声明
```cpp
void StartAutoStopTimer();
void StopAutoStopTimer();
```

### 2. 实现文件修改 (`main/application.cc`)

#### 2.1 构造函数中添加定时器创建
```cpp
// 创建自动停止定时器
esp_timer_create_args_t auto_stop_timer_args = {
    .callback = [](void* arg) {
        Application* app = (Application*)arg;
        app->Schedule([app]() {
            ESP_LOGI(TAG, "Auto stop timer triggered, sending stop listening command");
            if (app->protocol_ && app->device_state_ == kDeviceStateListening) {
                app->protocol_->SendStopListening();
                app->SetDeviceState(kDeviceStateIdle);
            }
        });
    },
    .arg = this,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "auto_stop_timer",
    .skip_unhandled_events = true
};
esp_timer_create(&auto_stop_timer_args, &auto_stop_timer_handle_);
```

#### 2.2 析构函数中添加定时器清理
```cpp
if (auto_stop_timer_handle_ != nullptr) {
    esp_timer_stop(auto_stop_timer_handle_);
    esp_timer_delete(auto_stop_timer_handle_);
}
```

#### 2.3 实现定时器控制方法
```cpp
void Application::StartAutoStopTimer() {
    if (auto_stop_timer_handle_ != nullptr) {
        ESP_LOGI(TAG, "Starting auto stop timer (3 seconds)");
        esp_timer_start_once(auto_stop_timer_handle_, 3000000); // 3 seconds = 3000000 microseconds
    }
}

void Application::StopAutoStopTimer() {
    if (auto_stop_timer_handle_ != nullptr) {
        ESP_LOGI(TAG, "Stopping auto stop timer");
        esp_timer_stop(auto_stop_timer_handle_);
    }
}
```

#### 2.4 状态管理中的定时器控制

在 `SetDeviceState` 方法中添加了定时器的启动和停止逻辑：

- **进入 listening 状态时启动定时器**：
  ```cpp
  case kDeviceStateListening:
      // ... 其他代码 ...
      // 启动3秒自动停止定时器
      StartAutoStopTimer();
      break;
  ```

- **退出 listening 状态时停止定时器**：
  ```cpp
  case kDeviceStateIdle:
  case kDeviceStateConnecting:
  case kDeviceStateSpeaking:
      // ... 其他代码 ...
      // 停止自动停止定时器
      StopAutoStopTimer();
      break;
  ```

## 工作流程

1. **唤醒词检测**：设备检测到唤醒词
2. **进入监听状态**：设备状态变为 `kDeviceStateListening`
3. **启动定时器**：自动启动3秒定时器
4. **音频传输**：设备开始向服务器发送音频数据
5. **定时器触发**：3秒后定时器触发，自动发送stop命令
6. **状态转换**：设备状态变为 `kDeviceStateIdle`

## 日志输出

功能会输出以下日志信息：

```
I (xxxx) Application: Starting auto stop timer (3 seconds)
I (xxxx) Application: Auto stop timer triggered, sending stop listening command
I (xxxx) Application: Stopping auto stop timer
```

## 注意事项

1. **线程安全**：定时器回调使用 `Schedule` 方法确保在主事件循环中执行
2. **状态检查**：定时器触发时会检查当前设备状态，只有在 listening 状态时才发送stop命令
3. **资源管理**：在析构函数中正确清理定时器资源
4. **状态同步**：在状态转换时正确启动和停止定时器

## 测试建议

1. **编译测试**：确保代码能正常编译
2. **功能测试**：测试唤醒词检测后3秒自动停止功能
3. **日志验证**：检查日志输出是否正确
4. **服务器验证**：确认服务器能收到完整的语音数据

## 后续优化

1. **可配置时间**：将3秒时间设为可配置参数
2. **VAD集成**：结合VAD检测结果优化停止时机
3. **用户交互**：允许用户手动停止录音
4. **错误处理**：添加更多的错误处理逻辑
