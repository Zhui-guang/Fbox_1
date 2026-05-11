# PCM Pack Tool (W25Q128)

用途：
- 根据 PCM 文件大小自动计算 `duration_ms` 与 `data_len`
- 生成可直接粘贴到 `src/music_library.c` 的 `TrackMeta` 条目

## 命令示例

```powershell
python tools/pcm_pack/pcm_pack.py `
  --name "Jiu Jiu Lao Qin" `
  --id 2 `
  --pcm "C:\Users\DELL\Desktop\Fbox\赳赳老秦，复我河山.pcm" `
  --addr 0x001000 `
  --sample-rate 22050 `
  --bits 16 `
  --channels 1
```

## 参数约定

- `--sample-rate 22050`
- `--bits 16`
- `--channels 1`
- `--addr` 建议按 `4KB` 对齐（`0x1000` 边界）

## 结果使用

1. 将输出的结构体条目粘贴到 `src/music_library.c` 对应曲目
2. 重新编译下载
3. 在串口确认 `PCM probe ok` 日志

