#ifndef MOTOR_HOST_MOCK_HEADFILE_H
#define MOTOR_HOST_MOCK_HEADFILE_H

/* Host API stand-ins for the local official headers. No register/PWM emulation. */
#include <stdint.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef void (*callback_function)(void);

#define IO_P32 0x42
#define IO_P52 0x62
#define IO_P74 0x84
#define IO_P75 0x85
#define IO_P76 0x86
#define IO_P77 0x87
#define PWMB_CH1_P74 (1 << 13 | 0 << 11 | 3 << 8 | IO_P74)
#define PWMB_CH2_P75 (1 << 13 | 1 << 11 | 3 << 8 | IO_P75)
#define PWM_DUTY_MAX 10000
#define SYSTEM_CLOCK_96M 96000000UL
#define TIM0_PIT 0
#define TIMER0_IRQn 1
#define GPIO_LOW 0
#define GPIO_HIGH 1
#define GPI 0
#define GPO 1
#define GPI_PULL_UP 3
#define GPO_PUSH_PULL 0x10

void clock_init(uint32 clock);
void interrupt_global_disable(void);
void interrupt_global_enable(void);
void interrupt_set_priority(int irq, uint8 priority);
void gpio_init(int pin, int direction, uint8 level, int mode);
uint8 gpio_get_level(int pin);
void gpio_set_level(int pin, uint8 level);
void pwm_init(int pin, uint32 frequency, uint32 duty);
void pwm_set_duty(int pin, uint32 duty);
void pit_ms_init(int timer, uint32 period_ms, callback_function callback);

#endif
