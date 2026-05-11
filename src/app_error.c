#include "app_error.h"

const char *App_ErrorToString(AppErrorCode code)
{
    switch (code)
    {
    case APP_OK:
        return "APP_OK";
    case APP_ERR_SPI_LCD_INIT:
        return "APP_ERR_SPI_LCD_INIT";
    case APP_ERR_ST7789_INIT:
        return "APP_ERR_ST7789_INIT";
    case APP_ERR_AUDIO_I2S_INIT:
        return "APP_ERR_AUDIO_I2S_INIT";
    default:
        return "APP_ERR_UNKNOWN";
    }
}

