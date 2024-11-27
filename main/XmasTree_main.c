/* SD card and FAT filesystem example.
   This example uses SPI peripheral to communicate with SD card.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h" 
#include "driver/ledc.h"
#include "esp_log.h"
#include "App.h"
#include "SdCard.h"


// Adjust this to the correct pin for the red LED 
void SetErrorLed(bool on) 
{ 
    // Set the LED pin as an output 
    gpio_set_direction(RED_LED_PIN, GPIO_MODE_OUTPUT); 
    
    // Turn the LED on (assuming HIGH turns it on, adjust if necessary) 
    gpio_set_level(RED_LED_PIN, on ? 1 : 0); 
}

void app_main(void)
{
    esp_err_t ret;

    InitializeSdCard();

    // SetErrorLed(true);
    ret = LoadCommands();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open 'sequence.txt' file to control the lights.");
        return;
    }


    DeinitializeSdCard();
}
