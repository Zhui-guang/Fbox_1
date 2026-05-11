#ifndef __APP_STATE_H__
#define __APP_STATE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 应用层页面状态。
 * 当前阶段先用串口菜单验证输入链路；后续接 ST7789 时，这些状态可以直接对应屏幕页面。
 */
typedef enum
{
    APP_STATE_BOOT_LOGO = 0,
    APP_STATE_MAIN_MENU,
    APP_STATE_SNAKE_GAME,
    APP_STATE_MUSIC_PLAYER,
    APP_STATE_INPUT_TEST,
    APP_STATE_ABOUT,
    APP_STATE_SETTINGS
} AppState;

#ifdef __cplusplus
}
#endif

#endif /* __APP_STATE_H__ */
