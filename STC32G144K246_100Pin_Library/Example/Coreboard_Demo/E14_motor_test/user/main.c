/*********************************************************************************************************************
* STC32G144K Opensourec Library 即（STC32G144K 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2025 SEEKFREE 逐飞科技
*
* 本文件是STC32G144K开源库的一部分
*
* STC32G144K 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
* 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
*
* 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
* 甚至没有隐含的适销性或适合特定用途的保证
* 更多细节请参见 GPL
*
* 您应该在收到本开源库的同时收到一份 GPL 的副本
* 如果没有，请参阅<https://www.gnu.org/licenses/>
*
* 额外注明：
* 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
* 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
* 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
*
* 文件名称          
* 公司名称          成都逐飞科技有限公司
* 版本信息          查看 libraries/doc 文件夹内 version 文件 版本说明
* 开发环境          MDK FOR C251
* 适用平台          STC32G144K
* 店铺链接          https://seekfree.taobao.com/
*
* 修改记录
* 日期              作者           备注
* 2025-11-20        大W            first version
********************************************************************************************************************/

#include "zf_common_headfile.h"

/* Local two-motor button control, derived from the official E01 LED example.
 * DRV8701E dual V1.3: GND -> GND; NC open;
 * PWM1 -> P74, DIR1 -> P76, PWM2 -> P75, DIR2 -> P77.
 * Keep the driver OFF during reset, programming and PWM initialization:
 * the official pwm_init() briefly sets its GPIO high before PWM takes over.
 * Runtime USB CDC is intentionally not started. USB still powers the board;
 * P32 + RST enters the chip's HID bootloader for programming.
 */
#define MOTOR1_PWM          PWMB_CH1_P74
#define MOTOR2_PWM          PWMB_CH2_P75
#define MOTOR1_DIR          IO_P76
#define MOTOR2_DIR          IO_P77
#define TEST_BUTTON         IO_P32
#define TEST_LED            IO_P52
#define MOTOR_PWM_HZ        17000UL
/* 调速参数：两轮使用同一个占空比百分数；30表示30%，不是转速值。 */
#define MOTOR_DUTY_PERCENT  30
#define MOTOR_RUN_DUTY      (PWM_DUTY_MAX * 1UL * MOTOR_DUTY_PERCENT / 100UL)
#if (MOTOR_DUTY_PERCENT < 1) || (MOTOR_DUTY_PERCENT > 100)
#error MOTOR_DUTY_PERCENT must be between 1 and 100
#endif
#define TICK_MS             5
#define PRESS_TICKS         (20 / TICK_MS)
#define RELEASE_TICKS       (100 / TICK_MS)
#define BOOT_TICKS          (1000 / TICK_MS)

/* Only the Timer0 callback reads/writes these variables and changes PWM.
 * There are no blocking delays, prints or USB calls in that callback.
 */
static uint8 motor_running = 0;
static uint8 button_armed = 0;
static uint8 press_ticks = 0;
static uint8 release_ticks = 0;
static uint16 boot_ticks = BOOT_TICKS;
static uint16 heartbeat_ticks = 0;

/* 两轮启停：同一回调内分别写两路PWM。方向电平在main中固定。 */
static void motor_set_running(uint8 running)
{
    uint32 duty = running ? MOTOR_RUN_DUTY : 0;
    pwm_set_duty(MOTOR1_PWM, duty);
    pwm_set_duty(MOTOR2_PWM, duty);
    motor_running = running;
    button_armed = 0;
    press_ticks = 0;
    release_ticks = 0;
}

/* P32按键控制：按一次持续运行，再按一次停止；长按只操作一次。
 * 启动/停止后，必须先连续松开100ms才能接受下一次按下。
 * 本版本按用户要求持续运行，没有原先的300ms自动停止。
 */
void motor_tick(void)
{
    uint8 pressed = (gpio_get_level(TEST_BUTTON) == GPIO_LOW);

    if (boot_ticks != 0)
    {
        --boot_ticks;
        button_armed = 0;
        press_ticks = 0;
        release_ticks = 0;
    }
    else if (!pressed)
    {
        press_ticks = 0;
        if (release_ticks < RELEASE_TICKS)
        {
            ++release_ticks;
        }
        if (release_ticks == RELEASE_TICKS)
        {
            button_armed = 1;
        }
    }
    else
    {
        release_ticks = 0;
        if (button_armed != 0)
        {
            if (motor_running != 0)
            {
                /* 停止优先：第二次按下在下一个5ms采样时停止两轮。 */
                motor_set_running(0);
            }
            else
            {
                ++press_ticks;
                if (press_ticks >= PRESS_TICKS)
                {
                    motor_set_running(1);
                }
            }
        }
    }

    /* 蓝灯：运行时常亮；停止且按键就绪时每秒短亮；其余状态熄灭。
     * If the MCU resets/hangs, use the driver's physical OFF switch.
     */
    if (motor_running != 0)
    {
        heartbeat_ticks = 0;
        gpio_set_level(TEST_LED, GPIO_LOW);
    }
    else if (button_armed != 0)
    {
        if (++heartbeat_ticks >= (1000 / TICK_MS))
        {
            heartbeat_ticks = 0;
        }
        gpio_set_level(TEST_LED, (heartbeat_ticks < (100 / TICK_MS)) ? GPIO_LOW : GPIO_HIGH);
    }
    else
    {
        heartbeat_ticks = 0;
        gpio_set_level(TEST_LED, GPIO_HIGH);
    }
}

void main(void)
{
    clock_init(SYSTEM_CLOCK_96M);
    interrupt_global_disable();

    gpio_init(MOTOR1_DIR, GPO, GPIO_LOW, GPO_PUSH_PULL);
    /* User's wheel test: M1 LOW drives forward; M2 LOW drives backward.
     * Reverse M2 only; keep this level fixed while the PWM test runs.
     */
    gpio_init(MOTOR2_DIR, GPO, GPIO_HIGH, GPO_PUSH_PULL);
    gpio_init(IO_P74, GPO, GPIO_LOW, GPO_PUSH_PULL);
    gpio_init(IO_P75, GPO, GPIO_LOW, GPO_PUSH_PULL);
    gpio_init(TEST_BUTTON, GPI, GPIO_HIGH, GPI_PULL_UP);
    gpio_init(TEST_LED, GPO, GPIO_HIGH, GPO_PUSH_PULL);

    /* Both channels share the PWMB counter and must use the same frequency.
     * Initialize with driver OFF. DIR1 stays LOW and DIR2 stays HIGH.
     */
    pwm_init(MOTOR1_PWM, MOTOR_PWM_HZ, 0);
    pwm_init(MOTOR2_PWM, MOTOR_PWM_HZ, 0);
    pit_ms_init(TIM0_PIT, TICK_MS, motor_tick);
    interrupt_set_priority(TIMER0_IRQn, 3);
    interrupt_global_enable();

    while (1)
    {
        /* Input, timeout and output updates all run in motor_tick(). */
    }
}

