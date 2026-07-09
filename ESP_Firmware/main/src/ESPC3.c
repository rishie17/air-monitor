#include "Measurement_packet.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/i2c_types.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "soc/clk_tree_defs.h"
#include "soc/gpio_num.h"
#include <driver_scd40.h>
#include <driver_sgp41.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <nvs_flash.h>
#include <ssd1306.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *BLE_Adv = "BTHOME_ADV";
RTC_FAST_ATTR Measurement data;
// 0 for display not woken up, 1 for display woken up
uint8_t display_status = 0;
TaskHandle_t ble_handle;
TaskHandle_t main_handle;
TaskHandle_t display_handle;
uint8_t ble_addr_type;

void start_adv(void *pvParams) {
  int errCode;
  struct ble_gap_adv_params adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  adv_params.itvl_max = 850;
  adv_params.itvl_min = 550;

  struct ble_hs_adv_fields adv_fields;
  memset(&adv_fields, 0, sizeof(adv_fields));
  adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  uint8_t advbuff[] = {// BTHOME UUID
                       0xD2, 0xFC,
                       // BTHOME V2
                       0x40,
                       // CO2 PPM
                       0x12, 0x00, 0x00,
                       // Humidity RH %
                       0x2E, 0x00,
                       // PM 2.5
                       0x0D, 0x00, 0x00,
                       // PM 10.0
                       0x0E, 0x00, 0x00,
                       // Temperature Deg C
                       0x57, 0x00,
                       // TVOC
                       0x13, 0x00, 0x00,
                       // NOX
                       0x3D, 0x00, 0x00};
  adv_fields.svc_data_uuid16 = (const uint8_t *)advbuff;
  adv_fields.svc_data_uuid16_len = sizeof(advbuff);

  while (1) {
    // Wait for data to be ready
    xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);

    // Copy over the data from meas_data to advbuff, in little endian
    memcpy(advbuff + 4, &data.co2_ppm, sizeof(data.co2_ppm));
    memcpy(advbuff + 7, &data.RH_percent, sizeof(data.RH_percent));
    memcpy(advbuff + 9, &data.pm2p5_ug_m3, sizeof(data.pm2p5_ug_m3));
    memcpy(advbuff + 12, &data.pm10_ug_m3, sizeof(data.pm10_ug_m3));
    memcpy(advbuff + 15, &data.temp_deg_c, sizeof(data.temp_deg_c));
    memcpy(advbuff + 17, &data.tvoc_ppm, sizeof(data.tvoc_ppm));
    memcpy(advbuff + 20, &data.nox_ppm, sizeof(data.nox_ppm));

    errCode = ble_gap_adv_set_fields(&adv_fields);
    if (errCode != ESP_OK) {
      ESP_LOGE(
          BLE_Adv,
          "FAILED: To set the advertisement Fields. Exiting TASK with rc = %d",
          errCode);
      vTaskDelete(NULL);
    }

    errCode = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                                &adv_params, NULL, NULL);
    if (errCode != ESP_OK) {
      ESP_LOGE(
          BLE_Adv,
          "FAILED : Starting the BLE GAP advertisement Server with rc = %d",
          errCode);
      vTaskDelete(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
    ble_gap_adv_stop();
    xTaskNotifyGive(main_handle);
  }
}

// This task should be woken up after it's requirements:
// 1) SSD1306 driver init along with the handle
void displayData(void *pvParams) {
  ssd1306_handle_t d = (ssd1306_handle_t)pvParams;
  while (1) {
    xTaskNotifyWait(0, 0, 0, portMAX_DELAY);
    ssd1306_clear(d);
    // Header Bar
    ssd1306_draw_text_scaled(d, 0, 0, "!Air", true, 2);
    ssd1306_draw_text_scaled(d, 58, 0, "AQI", true, 2);
    // TODO: Calculate AQI once BMV is resolved along with PM10 and PM2.5
    ssd1306_draw_text_scaled(d, 94, 0, "XXX", true, 2);
    ssd1306_draw_text(d, 0, 16, "  CO2:", true);
    ssd1306_draw_text(d, 0, 16 + 10, "PM2.5:", true);
    ssd1306_draw_text(d, 0, 16 + 20, " PM10:", true);
    ssd1306_draw_text(d, 0, 16 + 30, "   \"C:", true);
    ssd1306_draw_text(d, 0, 16 + 40, "   RH:", true);

    // The following block if text printing are only supposed to handle 4 digits
    char buf[6];
    sprintf(buf, "%hu", data.co2_ppm);
    ssd1306_draw_text(d, 36, 16, buf, true);         // CO2
    ssd1306_draw_text(d, 36, 16 + 10, "XXXX", true); // PM2.5
    ssd1306_draw_text(d, 36, 16 + 20, "XXXX", true); // PM10
    sprintf(buf, "%hd", data.temp_deg_c);
    ssd1306_draw_text(d, 36, 16 + 30, buf, true); // Temp Deg C
    sprintf(buf, "%u", data.RH_percent);
    ssd1306_draw_text(d, 36, 16 + 40, buf, true); // RH

    ssd1306_draw_text(d, 64, 16, "TVOC:", true);
    ssd1306_draw_text(d, 64, 16 + 10, " NOX:", true);

    // The following display bots
    sprintf(buf, "%hu", data.tvoc_ppm);
    ssd1306_draw_text(d, 64 + 30, 16, buf, true);
    sprintf(buf, "%hu", data.nox_ppm);
    ssd1306_draw_text(d, 64 + 30, 16 + 10, buf, true);

    ssd1306_draw_rect(d, 64, 16 + 20, 60, 26, true);

    ssd1306_display(d);
    ssd1306_display_wakeup(d);
  }
  vTaskDelete(NULL);
}

// The button only works once, i.e. the display will stay on for a fixed amount
// of time, and quit. Pressing the button multiple times will not extend the
// device wake period
// TODO: Have to spend sometime to check if extending the display time is worth
// it since we'll be blocking other tasks and Measurement broadcasts will be on
// hold
void wakeup_display(void *pv) {
  if (!display_status) {
    xTaskNotifyGive(display_handle);
    display_status = 1;
  }
}

void config_display() {
  uint32_t wakeup_causes = esp_sleep_get_wakeup_causes();
  if (wakeup_causes & BIT(ESP_SLEEP_WAKEUP_GPIO)) {
    // TODO: Wake up the display task
    xTaskNotifyGive(display_handle);
    display_status = 1;
  } else {
    gpio_config(&(gpio_config_t){
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << GPIO_NUM_3,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    });
    gpio_install_isr_service(0);
    // TODO: Add the display wakeup ISR
    gpio_isr_handler_add(GPIO_NUM_3, wakeup_display, NULL);
  }
}

void onsync(void) {
  ble_hs_id_infer_auto(0, &ble_addr_type);
  xTaskCreate(start_adv, "BLE advertisement", 4096, NULL, 4, &ble_handle);
}

void host_task(void *param) { nimble_port_run(); }

void app_main(void) {
  main_handle = xTaskGetCurrentTaskHandle();
  nvs_flash_init();
  nimble_port_init();
  ble_hs_cfg.sync_cb = onsync;
  nimble_port_freertos_init(host_task);

  i2c_master_bus_config_t bus_conf = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .flags.enable_internal_pullup = false,
      .glitch_ignore_cnt = 7,
      .i2c_port = I2C_NUM_0,
      .scl_io_num = GPIO_NUM_4,
      .sda_io_num = GPIO_NUM_5,
  };

  i2c_master_bus_handle_t bus_handle;
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_conf, &bus_handle));

  scd40 scd40;
  scd40_init(bus_handle, 400000, &scd40);
  sgp41 sgp41;
  sgp41_init(bus_handle, 400000, &sgp41);
  ssd1306_handle_t ssd1306;
  ssd1306_new_i2c(
      &(ssd1306_config_t){
          .bus = SSD1306_I2C,
          .height = 64,
          .width = 128,
          .iface.i2c =
              (ssd1306_i2c_cfg_t){
                  .addr = 0x3C,
                  .port = I2C_NUM_0,
                  .rst_gpio = GPIO_NUM_NC,
              },
      },
      &ssd1306);
  xTaskCreate(displayData, "OLED Display", 8192 * 2, ssd1306, 2,
              &display_handle);
  config_display();

  scd40_stop_periodic_measurement(&scd40);
  vTaskDelay(pdMS_TO_TICKS(500));
  // scd40_reinit(&scd40);
  scd40_start_periodic_measurement(&scd40);
  // We need 5s until the first packet of data arrives
  // So, We move to conditioning the SGP41 for 5s
  for (int i = 0; i < 5; i++) {
    sgp41_execute_conditioning(&sgp41, 50, 25);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  // The SGP41 is still in conditioning, and we have a 4.75s deadline to send a
  // read command
  // Dummy Call to transition SGP41 to Measurement Mode.
  sgp41_read_raw_measurements(&sgp41, 50, 25, &data.nox_ppm, &data.tvoc_ppm);
  while (!scd40_get_data_ready_status(&scd40)) {
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  scd40_read_measurement(&scd40, &data.temp_deg_c, &data.RH_percent,
                         &data.co2_ppm);
  sgp41_read_raw_measurements(&sgp41, data.RH_percent, data.temp_deg_c,
                              &data.nox_ppm, &data.tvoc_ppm);
  scd40_stop_periodic_measurement(&scd40);
  sgp41_turn_heater_off(&sgp41);

  // TODO:  I2C swap and BMV080 Data
  // NOTE: Left out BMV sensor reading functions since they'll just interfere
  // with errors at runtime for this commit/release

  // Signal BLE that data is ready
  xTaskNotifyGive(ble_handle);

  // Wait for advertisement to complete
  xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);

  // TODO: Configure additional deepsleep and wake up pins and parameters
  // 10s sleep time
  esp_sleep_enable_timer_wakeup(1000000ULL * 10);
  esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(1ULL << GPIO_NUM_3,
                                                      ESP_GPIO_WAKEUP_GPIO_LOW);

  if (display_status) {
    // 5s delay to keep the display open
    vTaskDelay(pdMS_TO_TICKS(5000));
    ssd1306_display_sleep(ssd1306);
  }
  esp_deep_sleep_start();
}
