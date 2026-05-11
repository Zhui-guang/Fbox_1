#ifndef __APP_ERROR_H__
#define __APP_ERROR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    APP_OK = 0,
    APP_ERR_SPI_LCD_INIT = 1,
    APP_ERR_ST7789_INIT = 2,
    APP_ERR_AUDIO_I2S_INIT = 3
} AppErrorCode;

const char *App_ErrorToString(AppErrorCode code);

#ifdef __cplusplus
}
#endif

#endif /* __APP_ERROR_H__ */

