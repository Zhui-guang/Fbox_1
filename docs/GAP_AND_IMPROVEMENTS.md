# Fbox_1 不足与改进路线（已同步当前代码）

## 已完成（本轮）
- 新增第二游戏：`Brick Game`（最小可玩）
- 主状态机改为 `APP_STATE_GAME` + `active_game(GameOps)` 路由
- 高分存储扩展：`snake_high_score` + `brick_high_score`
- 存储升级为 A/B 双页容错（Sector10/11 + sequence + CRC）
- 音频升级为 I2S DMA 循环双缓冲（非阻塞）

## 当前仍可优化点
1. 打砖块物理手感
- 目前为课程版可玩逻辑，球速/反弹角度可再细化（按命中挡板位置改变反射角）。

2. 游戏通用 HUD 规范
- Snake/Brick 已有头尾栏，但还可统一成同一套主题（颜色、图标、文案节奏）。

3. 音色模式切换
- 建议在 Settings 增加 `Soft / Classic` 两档。

4. 音频事件覆盖率
- 目前核心事件已接入，可再细分：连击、胜利旋律、失败下沉音。

5. 统一诊断页
- 建议加一页 Runtime Diagnostics：
  - Audio DMA running
  - Storage active page(A/B)
  - Storage sequence
  - Input queue overflow/peak

## 后续开发顺序（建议）
1. 打砖块手感调优 + 胜负页面美化
2. Settings 增加音色模式与音乐控制项
3. 统一 HUD 主题与页面动画细节
4. 诊断页 + 演示脚本文档封版

## 答辩可强调亮点
- 裸机状态机 + 模块化服务层（输入/显示/音频/存储）
- 非阻塞音频（I2S DMA 双缓冲）
- 双页容错存储（掉电恢复能力）
- 双游戏统一接口（GameOps），便于扩展第三个游戏
