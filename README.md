# 📡 RC5 Remote Control for ESP32 (ESP-IDF)

This project enables your ESP32 to receive and decode **RC5 infrared remote control** signals using the **ESP-IDF framework**. It includes a FreeRTOS task that continuously polls for RC5 commands and acts on them based on a defined command map.



## ⚙️ Features

- 🧠 Decodes RC5 signals (toggle, address, command bits)
- 🎛 Sets software flags based on command values
- 📦 Easy integration with any ESP-IDF project
- ⏱ Periodic polling using FreeRTOS

##Use
'\
void check_sw(uint8_t keybyte, uint8_t toggle)
{
    if (prev_toggle == toggle)
    {
        if (prev_keybyte == keybyte)
        {
            counter++;
            if (counter < 2)
                keybyte = 0xff;
        }
        else
        {
            prev_keybyte = keybyte;
            counter = 0;
        }
        prev_toggle = toggle;
    }
    else
    {
        prev_toggle = toggle;
        prev_keybyte = keybyte;
        counter = 0;
    }
    // ESP_LOGI(TAG, "counter=%d, keybyte=%d", counter, keybyte);
    //  PORTA ^= (1<<PORTA6); //debug
    switch (keybyte)
    {
    // inverted 4 data bits
    case 56:
        flag_sw_prog = 1;
        break; // 200*5=1000ms, 1sec
    case 59:
        flag_sw_menu = 1;
        break;
    case 37:
        flag_sw_set = 1;
        break;

    case 32:
        flag_sw_up = 1;
        break; // 50*5=250ms, 1/4sec
    case 33:
        flag_sw_up = 1;
        break;

    case 16:
        flag_sw_dn = 1;
        break;
    case 17:
        flag_sw_dn = 1;
        break;

    case 12:
        flag_sw_exit = 1;
        break;

        //..... extended

    case 5:
        flag_sw_time = 1;
        break;
    case 3:
        flag_sw_hijri = 1;
        break;
    case 9:
        flag_sw_city = 1;
        break;
    case 7:
        flag_sw_iqama = 1;
        break;
    default:
        break;
    }
}
'\
'\
void rc5_task(void *pvParameters)
{
    uint16_t command;
    uint8_t toggle;
    uint8_t address;
    uint8_t cmdnum;
    uint8_t blink_counter = 0;
    const TickType_t xFrequency = pdMS_TO_TICKS(250);
    TickType_t xLastWakeTime = xTaskGetTickCount(); // Initialize xLastWakeTime to the current tick count

    while (1)
    {
        if (prog_flag || menu_flag)
        {
            blink_counter++;
            if (blink_counter == 2)
            {
                curr_blink_flag = !curr_blink_flag;
                blink_counter = 0;
            }
            timeout_counter++;
            if (timeout_counter >= 247)
            {
                timeout_counter = 0;
                flag_sw_exit = 1;
            }
        }

        /* Poll for new RC5 command */
        if (RC5_NewCommandReceived(&command))
        {
            /* Reset RC5 lib so the next command can be decoded */
            RC5_Reset();

            /* Toggle the LED */
            // gpio_set_level(LED_GPIO_PIN, !gpio_get_level(LED_GPIO_PIN));

            /* Process the RC5 command */
            if (RC5_GetStartBits(command) != 3)
            {
                ESP_LOGE(TAG, "Invalid Start Bits!");
            }

            toggle = RC5_GetToggleBit(command);
            address = RC5_GetAddressBits(command);
            cmdnum = RC5_GetCommandBits(command);
            check_sw(cmdnum, toggle);
            // ESP_LOGI(TAG, "Received RC5 command: toggle=%d, address=%d, command=%d", toggle, address, cmdnum);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
'\
'\
void app_main(void)
{
     /* Create a FreeRTOS task to handle RC5 decoding */
    RC5_Init();
    xTaskCreate(&rc5_task, "rc5_task", 4096, NULL, 10, NULL);

}
'\
