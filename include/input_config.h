#ifndef __INPUT_CONFIG_H__
#define __INPUT_CONFIG_H__

/* 输入服务统一扫描周期。20ms 对人机输入足够快，也方便按键去抖。 */
#define INPUT_SCAN_PERIOD_MS          20U

/* 摇杆方向阈值使用“进入/退出”两套阈值形成迟滞。
 * 目的：避免摇杆处于临界值附近时在 CENTER 和方向之间反复抖动。 */
#define JOY_X_LEFT_ENTER_THRESHOLD    1200U
#define JOY_X_LEFT_EXIT_THRESHOLD     1450U
#define JOY_X_RIGHT_ENTER_THRESHOLD   2500U
#define JOY_X_RIGHT_EXIT_THRESHOLD    2250U
#define JOY_Y_DOWN_ENTER_THRESHOLD    1400U
#define JOY_Y_DOWN_EXIT_THRESHOLD     1650U
#define JOY_Y_UP_ENTER_THRESHOLD      2700U
#define JOY_Y_UP_EXIT_THRESHOLD       2450U

/* 若实际接线/模块方向与逻辑相反，可在此翻转轴方向（1=翻转，0=不翻转）。 */
#define JOY_X_INVERT_DIRECTION        1U
#define JOY_Y_INVERT_DIRECTION        0U

/* 连续 3 次扫描状态一致才确认按键状态，约等于 60ms 去抖。 */
#define INPUT_KEY_DEBOUNCE_COUNT      3U

/* 菜单长按重复：先等待 500ms，再每 180ms 产生一次 repeat 事件。 */
#define INPUT_REPEAT_START_MS         500U
#define INPUT_REPEAT_INTERVAL_MS      180U

#endif /* __INPUT_CONFIG_H__ */
