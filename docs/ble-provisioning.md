## ESP32-S3 蓝牙配网功能需求书（BLE Provisioning）

### 1. 背景与目标
- **背景**：项目运行在乐鑫 ESP32‑S3 平台，需要支持首开/重置后通过 BLE 下发 Wi‑Fi SSID/密码，实现设备联网配置，且不影响既有语音交互、显示与协议栈。
- **核心目标**：
  - 设备在无有效 Wi‑Fi 配置时进入 **BLE 配网模式**，用户通过手机 App 完成 SSID/密码配置。
  - 与现有状态机和 UI 完全兼容；完成后保存到 NVS 并进入正常联网流程。
  - 支持按键长按 5 秒清空 Wi‑Fi 凭据并重新进入 BLE 配网。

### 2. 范围与非目标
- **范围**：
  - 仅实现 BLE 传输通道的 Wi‑Fi 配网（ESP‑IDF Wi‑Fi Provisioning Manager + BLE）。
  - 仅 ESP32‑S3 目标平台。
- **非目标（暂不实现）**：
  - 声波配网、SoftAP 配网以及其它任意非 BLE 方式。
  - 安全版本 v2（证书/签名）后续再评估。

### 3. 术语与依赖
- **Wi‑Fi Provisioning Manager**：ESP‑IDF 官方配网管理组件（`wifi_prov_mgr`）。
- **BLE Scheme**：使用 `wifi_prov_scheme_ble`（NimBLE）。
- **Security**：采用 Security v1（PoP 验证）。
- **PoP（Proof of Possession）**：固定值 `123456`（上线前可评估升级）。
- **依赖**：ESP‑IDF 5.4+；开启 NimBLE 与 `wifi_prov_mgr` 组件。

### 4. 关键需求
- **设备广播名称**：`Liuliu-xxxx`
  - `xxxx` 为设备 MAC 地址后两字节（建议 4 位十六进制大写，不带冒号），例如 `Liuliu-7A3F`。
- **安全与验证**：
  - Security v1；PoP 固定为 `123456`。
  - 不在日志中明文回显 PoP。
- **触发进入配网**：
  - 首次启动或 NVS 中无有效 Wi‑Fi 凭据时，自动进入 BLE 配网。
  - 手动触发：按键长按 5 秒，清除 Wi‑Fi 凭据并进入 BLE 配网（无二次确认）。
- **退出配网**：
  - 成功：保存凭据、停止 provisioning、关闭 BLE 控制器、进入联网流程。
  - 失败/超时：停止 provisioning 并提示，可再次长按进入。
  - 默认超时：10 分钟（可通过 Kconfig 配置）。
- **与现有代码兼容**：
  - 复用 `kDeviceStateWifiConfiguring` 展示与交互。
  - BLE 仅在配网窗口开启，用完即关，避免占用内存影响音频。
  - 不改变音频服务、WebSocket/MQTT 协议、OTA 与主循环语义。

### 5. 交互与 UI/UX
- **进入 BLE 配网（kDeviceStateWifiConfiguring）**：
  - 屏幕状态：显示“配置 Wi‑Fi（蓝牙）”。
  - 显示内容：
    - 设备名：`Liuliu-xxxx`
    - PoP：`123456`
    - 引导文案：打开手机蓝牙，使用“ESP BLE Provisioning”类 App 搜索设备并输入 PoP 完成配置。
  - 声音提示：进入配网播放提示音；成功播放成功音；失败播放错误音。
- **配置进行中**：
  - 收到 SSID 后显示“正在连接…”。
  - 连接失败（如密码错误）提示“密码错误，请重试”。
- **成功**：
  - 显示“配置成功，正在联网…”，随后进入正常联网。
- **长按按键（5s）清空网络并进入配网**：
  - 倒计时式提示（建议）：按住 5s 进入蓝牙配网，松手即取消；不需要二次确认。

### 6. 状态机与流程
- **启动阶段**：
  1. 设备启动 → 读取 NVS Wi‑Fi 凭据。
  2. 若无凭据 → `SetDeviceState(kDeviceStateWifiConfiguring)` → 启动 BLE provisioning。
  3. 若有凭据 → 正常联网流程。
- **BLE 配网阶段**：
  1. 开启 NimBLE 与 `wifi_prov_mgr`，设置设备名与 PoP。
  2. 等待手机端下发 SSID/密码。
  3. 收到后尝试连接目标 AP；成功则写入 NVS；失败提示错误。
  4. 成功或超时/取消后均停止 provisioning 并关闭 BLE 控制器。
- **手动重置网络**：
  - 检测到按键长按 ≥5s → 擦除 Wi‑Fi 凭据 → 进入 `kDeviceStateWifiConfiguring` 并启动 BLE 配网。

### 7. 失败场景与处理
- **PoP 错误**：App 端无法通过；在设备端记录失败计数（可选），引导核对 PoP。
- **密码错误**：显示“密码错误，请重试”，继续保持在配网界面。
- **BLE 断开/异常**：记录日志并保持在配网模式，允许重新连接；超时后自动退出。
- **超时（默认 10 分钟）**：提示“配网超时”，停止 provisioning；用户可长按重试。

### 8. 配置项（Kconfig 与默认）
- 新增选项：
  - `CONFIG_ENABLE_BLE_PROVISIONING`（bool，默认 y，仅 ESP32‑S3 生效）：是否启用 BLE 配网能力。
  - `CONFIG_BLE_PROV_TIMEOUT_MIN`（int，默认 10，范围 1–60）：BLE 配网超时（分钟）。
- 既有/相关：
  - `CONFIG_USE_ACOUSTIC_WIFI_PROVISIONING=n`（保持关闭）。
- `sdkconfig.defaults.esp32s3`：确保启用 NimBLE / `wifi_prov_mgr` 所需依赖与内存选项。

### 9. 数据持久化
- **存储**：成功后将 SSID/密码写入 NVS（沿用系统 Wi‑Fi 栈默认存储）。
- **清除**：长按按键 ≥5s 清空 Wi‑Fi 凭据（不影响其它业务 NVS 条目）。

### 10. 日志与可观测性
- 日志 TAG：`PROV`（配网核心流程）、`BLE`（控制器生命周期）、`WIFI`（连接结果）。
- 关键节点打印：
  - 进入/退出配网；设备名与安全模式（不打印 PoP）。
  - 收到 SSID；连接成功/失败与错误码；超时。

### 11. 兼容性与资源
- 仅在配网阶段启用 BLE；结束后关闭以释放内存。
- 不改变 `Application` 的主循环语义；仅在 `kDeviceStateWifiConfiguring` 驱动 BLE 配网。
- 不影响音频服务与网络协议（WebSocket/MQTT）。

### 12. 安全说明
- 当前使用 Security v1 + 固定 PoP `123456`，为便于调试与量产前联调。
- 上线前建议：改为随机/每设备唯一 PoP（或升级 Security v2）。

### 13. 验收标准
- 首次上电（无凭据）自动进入 BLE 配网；手机 App 搜索到 `Liuliu-xxxx` 并使用 PoP `123456` 成功下发 SSID/密码。
- 配网成功后断电重启可自动联网。
- 长按按键 5 秒可清空网络并重新进入 BLE 配网，无二次确认。
- 在 BLE 配网期间，音频/显示/协议功能不异常；配网结束后 BLE 已正确关闭。
- 超时 10 分钟未完成配网，会给出提示并退出；用户可再次长按重试。

### 14. 研发实现建议（供开发参考）
- 新增模块：`main/network/ble_provisioning.{h,cc}`，封装 `start/stop`、事件回调与超时。
- 在 `Application` 中：
  - 在检测无 Wi‑Fi 配置时 `SetDeviceState(kDeviceStateWifiConfiguring)` 并调用 `BleProvisioning::Start()`。
  - 配网成功回调中：停止 BLE、切换到联网流程。
- 按键长按的检测沿用现有按键框架；回调中执行清空 Wi‑Fi 凭据 + 进入配网。

