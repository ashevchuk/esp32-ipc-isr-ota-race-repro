#include <Arduino.h>
#include <WiFi.h>
#include "esp_ota_ops.h"

static uint32_t s_calls0 = 0;
static uint32_t s_calls1 = 0;

static void do_trigger(uint32_t *counter) {
  const esp_partition_t *other = esp_ota_get_next_update_partition(NULL);
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  esp_err_t err = ESP_FAIL;
  if (other != nullptr) {
    err = esp_ota_get_state_partition(other, &state);
  }
  (*counter)++;
  if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
    Serial.printf("UNEXPECTED err=%d state=%d calls0=%lu calls1=%lu\n",
                  (int)err, (int)state, s_calls0, s_calls1);
  }
}

static void task_core0(void *) {
  for (;;) {
    do_trigger(&s_calls0);
    vTaskDelay(1);
  }
}

static void task_core1(void *) {
  for (;;) {
    do_trigger(&s_calls1);
    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin("placeholder-ssid", "placeholder-pass");

  xTaskCreatePinnedToCore(task_core0, "trig0", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(task_core1, "trig1", 4096, NULL, 2, NULL, 1);
}

void loop() {
  delay(1000);
  Serial.printf("alive calls0=%lu calls1=%lu\n", s_calls0, s_calls1);
}
