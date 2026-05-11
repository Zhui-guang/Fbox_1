# Fbox_1 Architecture (Current)

## 1) Layering

- App/Game layer
  - `src/app.c` (lifecycle + scheduler only)
  - `src/app_input.c` (input event routing + page action logic)
  - `src/app_render.c` (screen/serial rendering)
  - `src/snake_game.c`
- Service layer
  - `src/input_service.c` (scan + debounce + edge/repeat + event queue)
  - `src/ui_layout.c` (header/content/footer page layout)
  - `src/screen.c` (display abstraction API)
  - `src/storage_service.c` (flash persistence, delayed save)
- Driver layer
  - `src/st7789.c`, `src/spi_lcd.c`, `src/adc.c`, `src/keys.c`, `src/usart.c`

### App Context

- Shared runtime state moved to `AppContext` (`include/app_priv.h`):
  - state id
  - menu cursor
  - render dirty flag
  - timing ticks
  - debug edge latches
- Benefit:
  - removes large static-cluster from one file
  - makes future split to multi-game router easier
  - isolates input/render responsibilities for safer iteration

## 2) Display Call Chain

- App/Game never depends on ST7789 directly.
- Current path:
  - `App/Snake -> Screen_* -> GFX_* -> ST7789/SPI`

This keeps business logic independent from panel-specific details.

### ST7789 Wiring (Current)

- `GND` -> board `GND`
- `VCC` -> board `3V3`
- `SCL` -> `PA5` (`SPI1_SCK`)
- `SDA` -> `PA7` (`SPI1_MOSI`)
- `CS` -> `PD1`
- `DS/DC` -> `PD15`
- `RES` -> `PD4`
- `BLK` -> `PE8` (backlight control, active high in current code)

Reference in code:
- `src/spi_lcd.c` (SPI pins)
- `src/st7789.c` (CS/RST/DC/BL pin macros)

## 3) Input Pipeline

- Scan period: `INPUT_SCAN_PERIOD_MS` (20 ms).
- For each scan:
  - ADC + GPIO sampling
  - key debounce
  - joystick hysteresis direction decision
  - build held events
  - derive `pressed/released/repeat`
  - push one `InputEventFrame` into a lightweight ring queue

Queue behavior:
- fixed size: 16
- overflow strategy: drop oldest, keep latest interaction

App consumption:
- `app_process_input()` drains all queued frames each cycle.
- Navigation uses `pressed | repeat`.
- Confirm/back uses `pressed` only.

## 4) Page Layout Rules

- Physical resolution: `240x280`
- Standard sections:
  - Header: `UI_TOP_H`
  - Content: middle region
  - Footer: `UI_BOTTOM_H`
- Rounded-corner safe area enabled:
  - `UI_SAFE_MARGIN_X/Y`
  - Header/footer text shifted by one character guard (`8 px`) at start.

Rule:
- Do not place critical first/last characters at top/bottom line edges.
- Boot page uses animated placeholder badge + title text:
  - `Fbox1.0 Game Console`

## 5) Current App States

- `BOOT_LOGO`
- `MAIN_MENU`
- `SNAKE_GAME`
- `MUSIC_PLAYER`
- `INPUT_TEST`
- `SETTINGS`
- `ABOUT`

## 6) Next Extension Path

- Add Brick game using same game interface shape as Snake:
  - init / enter / handle_input / update / render / consume_exit
- Add audio service hook points:
  - menu move/confirm
  - snake eat/hit/pause/game over

## 7) Audio (Quiet Mode First)

- Service file: `src/audio_service.c`
- Current behavior:
  - event chain is connected
  - I2S3 minimal output path is initialized
  - default `enabled=0` (silent)
  - default volume `1%` (low, for classroom/rest environment)
  - supports demo melody playback (simplified "Dong Fang Hong" phrase)
- Runtime control API:
  - `Audio_Service_SetEnabled(1/0)`
  - `Audio_Service_SetVolumePercent(0..100)`
  - `Audio_Service_PlayDemoMusic()`

## 8) Persistent Storage (Flash)

- Target: STM32F407 internal flash Sector 11 (`0x080E0000`)
- Stored fields:
  - `sound_enabled`
  - `volume_percent`
  - `snake_high_score`
- Data integrity:
  - `magic + version + payload_size + crc32`
- Write policy:
  - mark dirty on change
  - delayed flush (`~600ms`) to avoid frequent erase/write
- Current integration:
  - boot: load storage, apply audio config and snake high score
  - settings: toggle sound/volume updates storage
  - snake: when high score changes, storage updated

### MAX98357 Wiring (Planned / Current Code Mapping)

- `LRC/WS` -> `PA4` (`I2S3_WS`)
- `BCLK` -> `PC10` (`I2S3_CK`)
- `DIN` -> `PC12` (`I2S3_SD`)
- `GAIN` -> follow module default or resistor config (hardware)
- `SD` (shutdown pin, if present) -> tie to enable as needed
- `VIN`/`3V3` and `GND` per module requirement

## 9) Boot Logo Asset

- Source image copied into project:
  - `assets/院徽.jpg`
- Converted for firmware rendering:
  - `include/logo_badge.h`
  - `src/logo_badge.c` (RGB565 C array, 52x52)
- Boot page now renders real crest + text:
  - `Fbox1.0 Game Console`

- Rendering path:
  - Screen_DrawRGB565Bitmap -> GFX_DrawRGB565Bitmap -> ST7789_DrawRGB565Bitmap
  - avoids per-pixel API overhead during logo draw

