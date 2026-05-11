# Fbox_1 Architecture (Current)

## 1) Layering

- App/Game layer
  - `src/app.c` (lifecycle + scheduler only)
  - `src/app_input.c` (input event routing + page action logic)
  - `src/app_render.c` (screen/serial rendering)
  - `src/snake_game.c`
  - `src/brick_game.c`
- Service layer
  - `src/input_service.c` (scan + debounce + edge/repeat + event queue)
  - `src/ui_layout.c` (header/content/footer page layout)
  - `src/ui_widgets.c` (reusable widgets: list/dialog/status bar)
  - `src/screen.c` (display abstraction API)
  - `src/storage_service.c` (flash persistence, delayed save)
- Driver layer
  - `src/st7789.c`, `src/spi_lcd.c`, `src/adc.c`, `src/keys.c`, `src/usart.c`
  - bring-up error code: `src/app_error.c`

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

### Text Rendering Update

- ASCII text path now draws each 8x16 glyph via one RGB565 block write
  (`GFX_DrawChar8x16 -> ST7789_DrawRGB565Bitmap`), reducing per-pixel command overhead.
- UTF-8 fallback entry added:
  - `Screen_DrawTextUtf8Fallback` / `GFX_DrawTextUtf8Fallback`
  - non-ASCII currently renders boxed placeholder glyphs (framework ready for real Chinese glyph table).

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
- `GAME` (router to active game: Snake/Brick)
- `MUSIC_PLAYER`
- `INPUT_TEST`
- `SETTINGS`
- `ABOUT`

## 5.1 Boot Animation Refresh Strategy

- Boot page static content is rendered once.
- Progress bar is updated as a local region only.
- Crest bitmap is rendered in 2x scale at boot for better visual prominence.
- This removes repeated full-screen redraw during boot animation.

## 6) Game Router (Current)

- Menu supports two games:
  - `Snake Game`
  - `Brick Game`
- App keeps:
  - `active_game` (`GameOps*`)
  - `active_game_id` (Snake/Brick)
- `APP_STATE_GAME` runs generic flow:
  - input -> `active_game->handle_input`
  - update -> `active_game->update`
  - render -> `active_game->render`
  - exit -> `active_game->consume_exit_request`

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
  - `Audio_Service_IsReady()`

## 7.1 Fault/Fallback Handling

- Unified error codes:
  - `APP_ERR_SPI_LCD_INIT`
  - `APP_ERR_ST7789_INIT`
  - `APP_ERR_AUDIO_I2S_INIT`
- Fatal LCD bring-up failures enter fault loop with LED blink and serial error.
- Audio I2S failure shows non-fatal fault dialog then continues in silent mode.

## 8) Persistent Storage (Flash)

- Target: STM32F407 internal flash A/B pages
  - Page A: Sector 10 (`0x080C0000`)
  - Page B: Sector 11 (`0x080E0000`)
- Stored fields:
  - `sound_enabled`
  - `volume_percent`
  - `snake_high_score`
  - `brick_high_score`
- Data integrity:
  - `magic + version + payload_size + sequence + crc32`
- Write policy:
  - mark dirty on change
  - delayed flush (`~600ms`) to avoid frequent erase/write
  - always write to inactive page, then switch active page
- Current integration:
  - boot: load latest valid page by sequence
  - apply audio config and both game high scores
  - settings: toggle sound/volume updates storage
  - game router: snake/brick high score updates storage

## 9) Audio Pipeline (DMA)

- I2S3 output is now DMA circular double-buffered.
- Half/full DMA callbacks set refill flags.
- Main loop fills only the ready half-buffer with mixed SFX/BGM samples.
- Benefit:
  - removes blocking `HAL_I2S_Transmit` calls from app loop
  - reduces render/input jitter under frequent audio events

### MAX98357 Wiring (Planned / Current Code Mapping)

- `LRC/WS` -> `PA15` (`I2S3_WS`)
- `BCLK` -> `PC10` (`I2S3_CK`)
- `DIN` -> `PC12` (`I2S3_SD`)
- `GAIN` -> follow module default or resistor config (hardware)
- `SD` (shutdown pin, if present) -> tie to enable as needed
- `VIN`/`3V3` and `GND` per module requirement

## 9.1 External SPI Flash (W25Q128)

- Driver: `src/w25q128.c`, `include/w25q128.h`
- Bus: shared `SPI1`
  - `PA5` = SCK
  - `PA6` = MISO
  - `PA7` = MOSI
- Chip Select:
  - `PA4` = W25Q128 `CS`
- Boot self-check:
  - `main` calls `W25Q128_Init()` and `W25Q128_ReadJedecId()`
  - Serial log prints JEDEC ID (`EF 40 18` expected for W25Q128)
- Pin conflict note:
  - Because onboard W25Q128 uses `PA4` as CS, audio `I2S3_WS` was moved from `PA4` to `PA15`.

## 10) Boot Logo Asset

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

## 11) Music Player Planning

- Detailed plan and PCM parameter baseline documented in:
  - `docs/MUSIC_PLAYER_PLAN.md`
- Current recommended default for external W25Q128 playback:
  - `22050 Hz`, `16-bit PCM`, `Mono`

