# Fbox_1 Hardware Resource Plan

本文档记录当前已经验证通过的 STM32F407VGT6 掌上小游戏机硬件资源分配。后续新增屏幕、音频、Flash 存储、游戏应用层时，优先以本文档为引脚和外设冲突检查依据。

## 1. 基础工程状态

当前工程基于 PlatformIO + STM32Cube HAL：

```text
platform = ststm32
board = genericSTM32F407VGT6
framework = stm32cube
MCU = STM32F407VGT6
```

已对照 CubeMX 示例工程迁移并验证：

```text
HSE = 8 MHz
PLL_M = 8
PLL_N = 336
PLL_P = 2
PLL_Q = 7
SYSCLK = 168 MHz
HCLK = 168 MHz
APB1 = 42 MHz
APB2 = 84 MHz
SysTick = 1 ms
```

已验证：

```text
ST-Link SWD 下载正常
Flash 启动正常
SysTick / HAL_Delay 正常
PB2 用户 LED 正常
USART2 printf 正常
ADC1 双通道摇杆正常
摇杆按下和四个独立按键正常
```

## 2. 下载与调试接口

当前稳定下载方式：ST-Link SWD。

| 功能 | STM32 引脚 | 说明 |
| --- | --- | --- |
| SWDIO | PA13 | 调试下载，禁止复用 |
| SWCLK | PA14 | 调试下载，禁止复用 |
| GND | GND | 必须共地 |
| VTref/3V3 | 3V3 | ST-Link 目标参考电压 |

PlatformIO 当前配置：

```ini
upload_protocol = stlink
debug_tool = stlink
```

注意：

```text
PA13 / PA14 后续不得作为普通 GPIO、SPI、ADC 或其他复用功能使用。
BOOT0 正常开发时保持 0，不需要进入系统 Bootloader。
```

## 3. 时钟资源

| 资源 | 引脚/参数 | 状态 | 说明 |
| --- | --- | --- | --- |
| HSE_IN | PH0 | 已用 | 外部 8 MHz 晶振输入 |
| HSE_OUT | PH1 | 已用 | 外部 8 MHz 晶振输出 |
| SYSCLK | 168 MHz | 已验证 | 主系统时钟 |
| APB1 | 42 MHz | 已验证 | USART2、后续 I2C/I2S 等需核对 |
| APB2 | 84 MHz | 已验证 | ADC1、SPI1 等需核对 |
| SysTick | 1 ms | 已验证 | HAL_Delay 和软件调度基础 |

必须保留 `src/stm32f4xx_it.c` 中的：

```c
void SysTick_Handler(void)
{
    HAL_IncTick();
}
```

缺少该函数会导致 `HAL_Delay()` 卡死，表现为 LED 只亮不闪。

## 4. 已占用 GPIO 与外设

| 功能 | STM32 引脚 | 外设/模式 | 状态 | 说明 |
| --- | --- | --- | --- | --- |
| 用户 LED | PB2 | GPIO Output PP | 已验证 | 约 500 ms 心跳闪烁 |
| USART2_TX | PA2 | USART2 AF7 | 已验证 | 连接 USB-TTL RX |
| USART2_RX | PA3 | USART2 AF7 | 已验证 | 连接 USB-TTL TX |
| 摇杆 X | PA0 | ADC1_IN0 | 已验证 | 3.3V 模拟输入 |
| 摇杆 Y | PA1 | ADC1_IN1 | 已验证 | 3.3V 模拟输入 |
| 摇杆按下 A | PC13 | GPIO Input Pull-up | 已验证 | 低电平有效 |
| KEY_B | PB12 | GPIO Input Pull-up | 已验证 | 低电平有效 |
| KEY_START | PB13 | GPIO Input Pull-up | 已验证 | 低电平有效 |
| KEY_MENU | PB14 | GPIO Input Pull-up | 已验证 | 低电平有效 |
| KEY_EXTRA | PB15 | GPIO Input Pull-up | 已验证 | 低电平有效，备用 |

## 5. 当前接线表

### 5.1 USB-TTL 串口

| USB-TTL | STM32 | 说明 |
| --- | --- | --- |
| TXD | PA3 / USART2_RX | 交叉连接 |
| RXD | PA2 / USART2_TX | 交叉连接 |
| GND | GND | 必须共地 |

串口参数：

```text
115200 8N1
monitor_port = COM11
```

### 5.2 摇杆模块

| 摇杆模块 | STM32 | 说明 |
| --- | --- | --- |
| VCC | 3V3 | 不要接 5V，避免 ADC 输入超过 VDDA |
| GND | GND | 共地 |
| VRX | PA0 / ADC1_IN0 | X 轴 |
| VRY | PA1 / ADC1_IN1 | Y 轴 |
| SW | PC13 | 按下为低电平 |

ADC 已验证范围：

```text
X 居中约 1840~1850，范围约 0~4075
Y 居中约 2048~2050，范围约 0~4095
```

当前方向阈值采用进入/退出迟滞，配置集中在 `include/input_config.h`：

```text
X < 1200 -> 进入 LEFT，X > 1450 -> 退出 LEFT
X > 2500 -> 进入 RIGHT，X < 2250 -> 退出 RIGHT
Y < 1400 -> 进入 DOWN，Y > 1650 -> 退出 DOWN
Y > 2700 -> 进入 UP，Y < 2450 -> 退出 UP
```

输入扫描与长按重复参数：

```text
输入扫描周期 = 20 ms
按键去抖计数 = 3 次
方向长按 500 ms 后开始 repeat
repeat 间隔 = 180 ms
```

### 5.3 四个独立按键

| 按键 | STM32 | 事件 | 说明 |
| --- | --- | --- | --- |
| B | PB12 | INPUT_EVENT_B | 按键另一端接 GND |
| START | PB13 | INPUT_EVENT_START | 按键另一端接 GND |
| MENU | PB14 | INPUT_EVENT_MENU | 按键另一端接 GND |
| EXTRA | PB15 | INPUT_EVENT_EXTRA | 备用，按键另一端接 GND |

按键统一配置：

```text
GPIO Input Pull-up
松开 = 1
按下 = 0
软件中 pressed = 1
```

## 6. 输入事件定义

当前输入服务位定义：

| 事件 | 值 | 来源 |
| --- | --- | --- |
| INPUT_EVENT_UP | 0x0001 | 摇杆 Y 高方向 |
| INPUT_EVENT_DOWN | 0x0002 | 摇杆 Y 低方向 |
| INPUT_EVENT_LEFT | 0x0004 | 摇杆 X 低方向 |
| INPUT_EVENT_RIGHT | 0x0008 | 摇杆 X 高方向 |
| INPUT_EVENT_A | 0x0010 | 摇杆按下 PC13 |
| INPUT_EVENT_B | 0x0020 | PB12 |
| INPUT_EVENT_START | 0x0040 | PB13 |
| INPUT_EVENT_MENU | 0x0080 | PB14 |
| INPUT_EVENT_EXTRA | 0x0100 | PB15 |

输入服务输出：

```c
held_events      // 当前持续按下/方向保持
pressed_events   // 从未按到按下的边沿，已锁存
released_events  // 从按下到松开的边沿，已锁存
repeat_events    // 长按方向键产生的重复事件，主要用于菜单连续移动
```

事件消费策略：

```text
应用层每 20 ms 调用 Input_Service_Update()
菜单导航使用 pressed_events | repeat_events
确认/返回类操作只使用 pressed_events
应用层消费完成后调用 Input_Service_ClearEdgeEvents()
Input Test 页面额外锁存调试事件，避免 500 ms 打印周期漏掉 press/release/repeat
```

## 7. 后续外设预留建议

### 7.1 ST7789 SPI 彩屏

建议优先使用 SPI1，但需要避开已占用的 PA0/PA1/PA2/PA3/PA13/PA14。

当前已在代码中落地方案（`src/spi_lcd.c` + `src/st7789.c`）：

| LCD 信号 | 建议引脚 | 说明 |
| --- | --- | --- |
| SCK | PA5 / SPI1_SCK | 硬件 SPI1 |
| MOSI | PA7 / SPI1_MOSI | ST7789 通常只需 MOSI |
| CS | PD1 | 片选 |
| DC | PD15 | 命令/数据选择 |
| RST | PD4 | 屏幕硬复位 |
| BLK | PE8 | 背光控制（当前常亮） |

注意：PA6/SPI1_MISO 可不接。

### 7.2 MAX98357 I2S 音频

I2S 需要与 SPI/I2S 资源统一规划，暂不落地。建议后续优先核对开发板可用 I2S 丝印，再确定：

```text
BCLK
LRCLK / WS
DIN
```

### 7.3 Flash 存储

后续使用内部 Flash 保存最高分时，必须避免频繁写入：

```text
只在游戏结束且最高分刷新时写入
只在设置变化时写入
```

## 8. 当前风险与约束

```text
PA0/PA1 已用于摇杆 ADC，不能再用于其他模拟输入。
PA2/PA3 已用于 USART2 调试串口，不能再用于摇杆或其他外设。
PB12~PB15 已用于按键，若后续发现与屏幕/音频布线冲突，需要重新分配。
PB2 是用户 LED，同时与 BOOT1 相关，当前已验证可作为 LED 输出使用。
PA13/PA14 是 SWD，严禁复用。
摇杆 VCC 必须接 3V3，不接 5V。
```

## 9. 下一阶段建议

输入硬件验证已完成，下一阶段建议进入软件框架整理：

```text
1. main.c 改为系统调度入口
2. 新建 app_state 状态机
3. 新建 menu 页面骨架
4. 保留 input_service 作为正式输入层
5. 串口调试保留为 DEBUG_LOG 接口
```

