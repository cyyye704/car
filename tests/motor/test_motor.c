/* Tests the real firmware source, rather than a translated state machine.
 * Each scenario is a separate process, so C static initialization is genuine.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "zf_common_headfile.h"

static jmp_buf initialization_complete;
static callback_function timer_callback;
static uint32 motor_duty[2];
static uint32 timer_period;
static unsigned tick_number;
static unsigned starts, stops;
static unsigned high_samples, low_samples;
static unsigned checks;
static unsigned button_level = GPIO_HIGH;
static unsigned initialization_mode;
static unsigned interrupts_enabled;
static unsigned pwm_initializations, gpio_initializations;
static unsigned priority_seen;
static unsigned release_qualified;
static unsigned led_level = GPIO_HIGH;
static const char *scenario = "arguments";

#define CHECK(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr, "FAIL %s tick=%u line=%d: %s\n", \
            scenario, tick_number, __LINE__, #condition); exit(1); \
} } while (0)

#define main firmware_main
#include "../../STC32G144K246_100Pin_Library/Example/Coreboard_Demo/E14_motor_test/user/main.c"
#undef main

/* Follow the configured percentage, rather than freezing tests at one speed.
 * PWM_DUTY_MAX is 10000 in this project; divide first for an independent
 * integer expectation. Hardware speed/current are outside this host test.
 */
#define EXPECTED_RUN_DUTY ((uint32)MOTOR_DUTY_PERCENT * (PWM_DUTY_MAX / 100UL))

static unsigned output_running(void)
{
    /* Call only at callback boundaries: writes to channels are sequential. */
    CHECK(motor_duty[0] == motor_duty[1]);
    CHECK(motor_duty[0] == 0 || motor_duty[0] == EXPECTED_RUN_DUTY);
    return motor_duty[0] != 0;
}

void clock_init(uint32 clock)
{
    CHECK(initialization_mode);
    CHECK(clock == SYSTEM_CLOCK_96M);
}

void interrupt_global_disable(void) { interrupts_enabled = 0; }

void interrupt_global_enable(void)
{
    CHECK(initialization_mode);
    CHECK(pwm_initializations == 2);
    CHECK(gpio_initializations == 6);
    CHECK(timer_callback != NULL && timer_period == 5);
    CHECK(priority_seen);
    CHECK(output_running() == 0);
    interrupts_enabled = 1;
    longjmp(initialization_complete, 1);
}

void interrupt_set_priority(int irq, uint8 priority)
{
    CHECK(!interrupts_enabled);
    CHECK(irq == TIMER0_IRQn && priority == 3);
    priority_seen = 1;
}

void gpio_init(int pin, int direction, uint8 level, int mode)
{
    CHECK(initialization_mode && !interrupts_enabled);
    if (pin == IO_P32) {
        CHECK(direction == GPI && level == GPIO_HIGH && mode == GPI_PULL_UP);
    } else {
        CHECK(direction == GPO && mode == GPO_PUSH_PULL);
        /* This car needs M1/P76 LOW and M2/P77 HIGH for forward travel. */
        CHECK(level == ((pin == IO_P52 || pin == IO_P77) ? GPIO_HIGH : GPIO_LOW));
    }
    ++gpio_initializations;
}

uint8 gpio_get_level(int pin)
{
    CHECK(pin == IO_P32);
    return (uint8)button_level;
}

void gpio_set_level(int pin, uint8 level)
{
    CHECK(pin == IO_P52);
    CHECK(level <= GPIO_HIGH);
    led_level = level;
}

void pwm_init(int pin, uint32 frequency, uint32 duty)
{
    CHECK(initialization_mode && !interrupts_enabled);
    CHECK(pin == PWMB_CH1_P74 || pin == PWMB_CH2_P75);
    CHECK(frequency == 17000 && duty == 0);
    ++pwm_initializations;
}

void pwm_set_duty(int pin, uint32 duty)
{
    unsigned index = (pin == PWMB_CH1_P74) ? 0 : 1;
    CHECK(pin == PWMB_CH1_P74 || pin == PWMB_CH2_P75);
    CHECK(duty == 0 || duty == EXPECTED_RUN_DUTY);
    motor_duty[index] = duty;
    /* One channel can change first; equality is required after the callback. */
}

void pit_ms_init(int timer, uint32 period_ms, callback_function callback)
{
    CHECK(initialization_mode && !interrupts_enabled);
    CHECK(timer == TIM0_PIT);
    timer_period = period_ms;
    timer_callback = callback;
}

static void sample(unsigned pressed)
{
    unsigned previous = output_running();
    unsigned current, should_start, should_stop;
    ++tick_number;
    button_level = pressed ? GPIO_LOW : GPIO_HIGH;
    if (pressed) {
        ++low_samples;
        high_samples = 0;
    } else {
        low_samples = 0;
        if (tick_number > 200) {
            ++high_samples;
            if (high_samples >= 20) release_qualified = 1;
        } else {
            high_samples = 0;
        }
    }
    should_start = !previous && release_qualified && pressed && low_samples >= 4;
    should_stop = previous && release_qualified && pressed;
    timer_callback();
    current = output_running();
    if (should_start) CHECK(current);
    if (should_stop) CHECK(!current);
    if (!previous && current) {
        CHECK(should_start);
        CHECK(tick_number >= 224);
        ++starts;
    } else if (previous && !current) {
        CHECK(should_stop);
        ++stops;
    }
    if (previous != current) {
        release_qualified = 0;
        high_samples = 0;
        low_samples = 0;
    }
    if (current) CHECK(led_level == GPIO_LOW);
    if (!current && !release_qualified) CHECK(led_level == GPIO_HIGH);
}

static void samples(unsigned pressed, unsigned count)
{
    while (count--) sample(pressed);
}

static void ready(void)
{
    samples(0, 220); /* 1000 ms boot lockout + 100 ms fresh release. */
    CHECK(starts == 0 && output_running() == 0);
}

static void start_first(void)
{
    ready();
    samples(1, 4);
    CHECK(starts == 1 && output_running() && motor_duty[0] == EXPECTED_RUN_DUTY);
}

int main(int argc, char **argv)
{
    unsigned i;
    uint32 rng = 0x32A5144U;
    CHECK(argc >= 2);
    scenario = argv[1];
    initialization_mode = 1;
    if (!setjmp(initialization_complete)) firmware_main();
    initialization_mode = 0;

    if (!strcmp(scenario, "boot-held")) {
        samples(1, 2000);
        CHECK(starts == 0);
    } else if (!strcmp(scenario, "boot-lockout")) {
        samples(0, 196);
        samples(1, 4);
        CHECK(starts == 0);
        samples(0, 19);
        samples(1, 20);
        CHECK(starts == 0);
        samples(0, 20);
        samples(1, 4);
        CHECK(starts == 1 && output_running());
    } else if (!strcmp(scenario, "release-required")) {
        samples(1, 300);
        samples(0, 19);
        samples(1, 100);
        CHECK(starts == 0);
        samples(0, 20);
        samples(1, 4);
        CHECK(starts == 1 && output_running());
    } else if (!strcmp(scenario, "button-bounce")) {
        ready();
        for (i = 0; i < 100; ++i) {
            samples(1, 3);
            samples(0, 1);
        }
        CHECK(starts == 0);
        samples(1, 4);
        CHECK(starts == 1 && output_running());
    } else if (!strcmp(scenario, "both-channels-configured-duty")) {
        start_first();
        CHECK(motor_duty[0] == EXPECTED_RUN_DUTY && motor_duty[1] == EXPECTED_RUN_DUTY);
    } else if (!strcmp(scenario, "release-keeps-running")) {
        start_first();
        samples(0, 12001); /* More than 60 seconds, with no 300 ms timeout. */
        CHECK(starts == 1 && stops == 0 && output_running());
        sample(1); /* After a valid release, the next press stops immediately. */
        CHECK(stops == 1 && !output_running());
    } else if (!strcmp(scenario, "start-held-no-retrigger")) {
        start_first();
        samples(1, 12001);
        CHECK(starts == 1 && stops == 0 && output_running());
    } else if (!strcmp(scenario, "stop-held-no-restart")) {
        start_first();
        samples(0, 20);
        sample(1);
        CHECK(stops == 1 && !output_running());
        samples(1, 2000);
        CHECK(starts == 1 && stops == 1 && !output_running());
    } else if (!strcmp(scenario, "release-before-stop")) {
        start_first();
        for (i = 0; i < 100; ++i) {
            samples(0, 19);
            samples(1, 20);
        }
        CHECK(starts == 1 && stops == 0 && output_running());
        samples(0, 20);
        sample(1);
        CHECK(stops == 1 && !output_running());
    } else if (!strcmp(scenario, "release-before-restart")) {
        start_first();
        samples(0, 20);
        sample(1);
        for (i = 0; i < 100; ++i) {
            samples(0, 19);
            samples(1, 20);
        }
        CHECK(starts == 1 && stops == 1 && !output_running());
        samples(0, 20);
        samples(1, 3);
        CHECK(starts == 1);
        sample(1);
        CHECK(starts == 2 && output_running());
    } else if (!strcmp(scenario, "stop-press-bounce")) {
        start_first();
        samples(0, 20);
        sample(1);
        CHECK(stops == 1 && !output_running());
        for (i = 0; i < 100; ++i) {
            samples(0, 1);
            samples(1, 1);
        }
        CHECK(starts == 1 && stops == 1);
    } else if (!strcmp(scenario, "no-old-cooldown")) {
        start_first();
        samples(0, 20);
        sample(1);
        samples(0, 20);
        samples(1, 4);
        CHECK(starts == 2 && stops == 1 && output_running());
    } else if (!strcmp(scenario, "repeated-start-stop")) {
        ready();
        for (i = 0; i < 100; ++i) {
            samples(1, 4);
            CHECK(output_running());
            samples(0, 20);
            sample(1);
            CHECK(!output_running());
            samples(0, 20);
        }
        CHECK(starts == 100 && stops == 100);
    } else if (!strcmp(scenario, "led-ready-heartbeat")) {
        unsigned on = 0, off = 0;
        ready();
        for (i = 0; i < 200; ++i) {
            sample(0);
            if (led_level == GPIO_LOW) ++on; else ++off;
        }
        CHECK(on == 20 && off == 180);
        samples(1, 4);
        CHECK(output_running() && led_level == GPIO_LOW);
        samples(0, 20);
        sample(1);
        CHECK(!output_running() && led_level == GPIO_HIGH);
    } else if (!strcmp(scenario, "random-inputs")) {
        unsigned run, pressed, run_length;
        if (argc > 2) rng = (uint32)strtoul(argv[2], NULL, 0);
        for (run = 0; run < 10000; ++run) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            pressed = rng & 1U;
            run_length = 1 + ((rng >> 1) % 300U);
            samples(pressed, run_length);
        }
        CHECK(starts > 100 && stops > 100);
    } else {
        fprintf(stderr, "Unknown scenario: %s\n", scenario);
        return 2;
    }
    printf("PASS %-27s ticks=%u starts=%u stops=%u checks=%u\n",
           scenario, tick_number, starts, stops, checks);
    return 0;
}
