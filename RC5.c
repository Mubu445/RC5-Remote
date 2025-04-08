#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include <soc/timer_group_struct.h>
#include "RC5.h"
#include <driver/gptimer.h>
#include "esp_log.h"

/* Define RC5 protocol timing constraints in microseconds */
#define SHORT_MIN1 444  /* 444 microseconds */
#define SHORT_MAX1 1333 /* 1333 microseconds */
#define LONG_MIN1 1334  /* 1334 microseconds */
#define LONG_MAX1 2222  /* 2222 microseconds */

/* Define GPIO and Timer */
#define RC5_GPIO_PIN GPIO_NUM_23 // Example GPIO pin for receiving RC5 data
#define TIMER_GROUP TIMER_GROUP_0
#define TIMER_IDX TIMER_0

const uint8_t trans[4] = {0x01, 0x91, 0x9b, 0xfb};
volatile uint16_t command;
volatile uint8_t ccounter;
volatile uint8_t has_new;
State state = STATE_BEGIN;

gptimer_handle_t gptimer1 = NULL; // Global variable for the GPTimer handle

/* Timer interrupt handler */
void IRAM_ATTR rc5_timer_isr_callback(void *arg)
{
    uint64_t delay;
    // Retrieve the timer counter value
    gptimer_get_raw_count(gptimer1, &delay);
    // Determine the event type based on GPIO input
    volatile uint8_t event = gpio_get_level(RC5_GPIO_PIN) ? 2 : 0;

    if (delay > LONG_MIN1 && delay < LONG_MAX1)
    {
        event += 4;
    }
    else if (delay < SHORT_MIN1 || delay > SHORT_MAX1)
    {
        RC5_Reset();
        // return;  // Continue timer
    }

    if (state == STATE_BEGIN)
    {
        ccounter--;
        command |= 1 << ccounter;
        state = STATE_MID1;
        gptimer_set_raw_count(gptimer1, 0); // Reset timer counter
        return;                            // Continue timer
    }

    State newstate = (trans[state] >> event) & 0x03;

    if (newstate == state || state > STATE_START0)
    {
        RC5_Reset();
        return; // Continue timer
    }

    state = newstate;

    if (state == STATE_MID0)
    {
        ccounter--;
    }
    else if (state == STATE_MID1)
    {
        ccounter--;
        command |= 1 << ccounter;
    }

    if (ccounter == 0 && (state == STATE_START1 || state == STATE_MID0))
    {
        state = STATE_END;
        has_new = 1;
        gpio_intr_disable(RC5_GPIO_PIN);
    }

    // Reset the timer counter
    gptimer_set_raw_count(gptimer1, 0);
    // Continue timer
}

/* RC5 Init Function */
void RC5_Init()
{
    // Configure GPIO
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << RC5_GPIO_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Initialize GPTimer
    gptimer_config_t timer_config1 = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1us per tick
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config1, &gptimer1));

    // Set up alarm configuration
    // gptimer_alarm_config_t alarm_config = {
    //     .alarm_count = 0,  // Alarm at count value 0
    //     .reload_count = 0,
    //     .flags.auto_reload_on_alarm = false,
    // };
    // gptimer_set_alarm_action(gptimer, &alarm_config);
    ESP_ERROR_CHECK(gptimer_enable(gptimer1));
    ESP_ERROR_CHECK(gptimer_start(gptimer1));
    // Install GPIO interrupt service and add handler
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(RC5_GPIO_PIN, rc5_timer_isr_callback, NULL);
    RC5_Reset();
}

void RC5_Reset()
{
    has_new = 0;
    ccounter = 14;
    command = 0;
    state = STATE_BEGIN;

    // Reset Timer Counter
    // gptimer_stop(gptimer);
    // gptimer_set_raw_count(gptimer, 0);
    // gptimer_start(gptimer);

    // Re-enable interrupt
    gpio_intr_enable(RC5_GPIO_PIN);
}
uint8_t RC5_NewCommandReceived(uint16_t *new_command)
{
    if (has_new)
    {
        *new_command = command;
    }

    return has_new;
}