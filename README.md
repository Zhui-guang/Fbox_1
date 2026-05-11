# Fbox_1

基于 STM32F407VGT6（天空星高配版）的彩屏掌上游戏机项目。

## 当前版本

- 版本号：`v0.9.0-dev`
- 分支基线：`dev-stm32f407`
- 本地增强：输入事件队列、中文界面支持、ST7789 局部刷新、贪吃蛇/打砖块、音乐播放器骨架、W25Q128 PCM 播放链路。

## 硬件平台

- 主控：STM32F407VGT6
- 屏幕：ST7789 SPI 彩屏
- 输入：摇杆（ADC + 按压）+ 四按键
- 音频：MAX98357（I2S）
- 外部存储：W25Q128 SPI Flash

## 开发环境

- VSCode + PlatformIO
- Framework: STM32Cube HAL

## 说明

本仓库 `main` 分支用于阶段性整合快照；日常开发建议在功能分支完成后合并到 `dev-stm32f407`。
