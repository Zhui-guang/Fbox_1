# Music Player 方案与音频参数（W25Q128）

## 1. 目标与约束
- 板载外部存储：W25Q128（16MB）
- 功放：MAX98357（I2S）
- 现有音频底座：`audio_service`（I2S DMA + BGM/SFX 混音）
- 目标：在不破坏游戏实时性的前提下，实现可用的音乐播放器

## 2. 推荐音频参数（最终建议）
### 2.1 首选参数（平衡方案）
- 采样率：`22050 Hz`
- 位深：`16-bit PCM (signed little-endian)`
- 声道：`Mono`
- 数据布局：裸 PCM 连续帧（后续由 `player_service` 分块喂给 `audio_service`）

### 2.2 画质优先参数（可选）
- 采样率：`32000 Hz`
- 位深：`16-bit PCM`
- 声道：`Mono`
- 使用场景：短曲/少曲目，追求更亮更清晰

### 2.3 导出与母带处理规范
- 峰值限制：`-3 dBFS`
- 首尾淡入淡出：`5~10 ms`
- 去 DC 偏置
- 避免硬削波（clipping）

## 3. 容量预算（单声道 16bit）
- 22.05kHz：约 `44.1 KB/s`
- 32kHz：约 `64 KB/s`

参考：
- 60 秒 @22.05k：约 `2.6 MB`
- 60 秒 @32k：约 `3.8 MB`
- 180 秒 @22.05k：约 `7.9 MB`
- 180 秒 @32k：约 `11.5 MB`

结论：
- 16MB 的 W25Q128 足够课程演示级播放器
- 若曲库增大，优先采用 22.05kHz，再考虑 ADPCM

## 4. 模块架构（与 audio_service 解耦）
## 4.1 模块划分
- `player_service`：播放状态机、曲目队列、播放控制 API
- `audio_service`：I2S DMA 输出 + BGM/SFX 混音（已具备）
- `music_library`：曲目元数据、片段索引、资源地址
- `player_ui`：列表页/播放页渲染与按键映射

## 4.2 核心数据结构
- `TrackMeta {id, name, duration_ms, type(TONE/PCM), data_ptr, data_len}`
- `PlayerState {STOPPED, PLAYING, PAUSED}`
- `PlayerContext {current_track, position_ms, repeat_mode, volume, state}`

## 4.3 控制接口
- `Player_Init()`
- `Player_Play(track_id)`
- `Player_Pause()` / `Player_Resume()`
- `Player_Stop()`
- `Player_Next()` / `Player_Prev()`
- `Player_SetVolume(percent)`
- `Player_Tick(now)`

## 5. 与 audio_service 的交互边界
- `player_service` 不直接操作 I2S/DMA
- 仅通过“BGM 数据请求/事件”与 `audio_service` 交互
- SFX 通道保持现有优先级（不被播放器破坏）

建议增加接口（后续实现）：
- `Audio_BgmFeedPcm16(const int16_t* pcm, uint16_t frames, uint32_t sample_rate)`
- `Audio_BgmStop()`
- `Audio_BgmSetGain(uint8_t percent)`

## 6. W25Q128 资源组织建议
- 方案 A（首版简单）：
  - 固件内置曲目索引表（地址+长度）
  - PCM 数据以连续块写入 Flash
- 方案 B（可维护）：
  - Flash 头部目录区 + 多曲文件区
  - 支持后续更新曲目包

推荐首版先做 A，快速稳定。

### 6.1 当前代码已落地的元数据字段
- `TrackMeta.sample_rate_hz`
- `TrackMeta.bits_per_sample`
- `TrackMeta.channels`
- `TrackMeta.flash_addr`
- `TrackMeta.data_len`

说明：
- 当前 `Player_Play(PCM)` 已接入首块读取探测（probe），用于验证 Flash 读链路；
- 还未进入“持续流式解码/播放”，这是下一步工作。

### 6.2 PCM 打包参数（本项目统一）
- 格式：`signed PCM little-endian`
- 采样率：`22050 Hz`
- 位深：`16-bit`
- 声道：`mono`

### 6.3 推荐烧录布局（W25Q128）
- `0x000000`：曲库索引区（后续）
- `0x001000`：Track#2 PCM 起始（建议）
- 按 4KB 扇区对齐每首歌起始地址

## 7. UI 交互建议
- 列表页：曲目选择 + 当前状态
- 播放页：进度条、播放/暂停、上一首/下一首、循环模式
- 设置联动：总音量、播放器音量偏置、音色模式

## 8. 实施顺序（建议）
1. `music_library` 与 `TrackMeta` 建立（先 1~2 首）
2. `player_service` 状态机骨架（Play/Pause/Stop/Next/Prev）
3. `audio_service` 增加 BGM PCM 喂入接口
4. `player_ui` 列表页 + 播放页
5. 曲目批量导入与容量压测

## 9. 当前结论（本项目推荐）
- 第一版统一采用：`22050 Hz / 16-bit / Mono`
- 这是本项目“音质、容量、实现复杂度”最优平衡点
- 关键展示曲可单独用 `32000 Hz` 做高音质版本
