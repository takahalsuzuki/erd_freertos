#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "GPIO.hpp"
#include <array>

struct led_task_arg {
    int gpio;
    int delay;
};

void led_task(void *p)
{
    struct led_task_arg *a = (struct led_task_arg *)p;

    gpio_init(a->gpio);
    gpio_set_dir(a->gpio, GPIO_OUT);
    while (true) {
        gpio_put(a->gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(a->delay));
        gpio_put(a->gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(a->delay));
    }
}

int main()
{
    setup_default_uart();
    printf("Start LED blink\n");

    struct led_task_arg arg1 = { 12, 1000 };
    xTaskCreate(led_task, "LED_Task 1", 256, &arg1, 1, NULL);

    struct led_task_arg arg2 = { 13, 2000 };
    xTaskCreate(led_task, "LED_Task 2", 256, &arg2, 1, NULL);

    struct led_task_arg arg3 = { 14, 4000 };
    xTaskCreate(led_task, "LED_Task 3", 256, &arg3, 1, NULL);

    struct led_task_arg arg4 = { 15, 8000 };
    xTaskCreate(led_task, "LED_Task 4", 256, &arg4, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
