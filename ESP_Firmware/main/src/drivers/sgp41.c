#include "./sgp41.h"
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/i2c_types.h"
#include <stdint.h>

// Logging toggle
#define LOG
#ifdef LOG
#define OLOG(x) ESP_LOGI("SGP41 Driver", x)
#define OLOGF(x, y) ESP_LOGI("SGP41 Driver", x, y)
#else
#define OLOG(x) x
#define OLOGF(x, y) x
#endif

struct sgp41 {
  i2c_master_dev_handle_t dev_handle;
};

void sgp41_init(i2c_master_bus_handle_t bus_handle, uint32_t speed,
                sgp41 *device) {
  i2c_device_config_t config = {
      .device_address = 0x59,
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .scl_speed_hz = speed,
      .flags.disable_ack_check = false,
  };

  ESP_ERROR_CHECK(
      i2c_master_bus_add_device(bus_handle, &config, &device->dev_handle));

  OLOG("SGP41 Sensor Init");
}

uint8_t sgp41_crc(const uint8_t *data, uint8_t count) {
  // We init CRC checks with 0xFF (Pg: 13 of SGP41 Datasheet)
  uint8_t crc = 0xFF;
  const uint8_t CRC_POLY = 0x31;

  for (uint8_t current_byte = 0; current_byte < count; current_byte++) {
    crc ^= data[current_byte];
    for (uint8_t bit = 8; bit > 0; bit--) {
      if (crc & 0x80) // Check MSB
        crc = (crc << 1) ^ CRC_POLY;
      else
        crc = (crc << 1);
    }
  }
  return crc;
}
void sgp41_execute_conditioning(sgp41 *device, uint16_t default_rh_percent,
                                uint16_t default_temp_c_deg) {
  uint8_t buf[8] = {0x26, 0x12};
  uint32_t rh = default_rh_percent * 65535;
  uint32_t temp = (default_temp_c_deg + 45) * 65535;
  rh /= 100;
  temp /= 175;
  default_rh_percent = rh;
  default_temp_c_deg = temp;

  buf[2] = default_rh_percent >> 8;
  buf[3] = default_rh_percent;
  buf[4] = sgp41_crc((uint8_t *)&default_rh_percent, 2);
  buf[5] = default_temp_c_deg >> 8;
  buf[6] = default_temp_c_deg;
  buf[7] = sgp41_crc((uint8_t *)&default_temp_c_deg, 2);

  uint8_t rec_buf[3];
  ESP_ERROR_CHECK(i2c_master_transmit(device->dev_handle, buf, 8, 50));

  vTaskDelay(pdMS_TO_TICKS(51));

  ESP_ERROR_CHECK(i2c_master_receive(device->dev_handle, rec_buf, 3, 20));

  OLOGF("Execute Conditioning: Recevied %x",
        ((uint16_t)rec_buf[0] << 8) | rec_buf[1]);
}

void sgp41_read_raw_measurements(sgp41 *device, uint16_t rh_percent,
                                 uint16_t temp_c_deg, uint16_t *raw_NOX,
                                 uint8_t *raw_VOC) {
  uint32_t rh = rh_percent * 65535;
  uint32_t temp = (temp_c_deg + 45) * 65535;
  rh /= 100;
  temp /= 35;
  rh_percent = rh;
  temp_c_deg = temp;

  uint8_t buf[8] = {
      0x26,
      0x19,
      rh_percent >> 8,
      rh_percent,
      sgp41_crc((uint8_t *)&rh_percent, 2),
      temp_c_deg >> 8,
      temp_c_deg,
      sgp41_crc((uint8_t *)&temp_c_deg, 2),
  };

  ESP_ERROR_CHECK(i2c_master_transmit(device->dev_handle, buf, 8, 25));

  vTaskDelay(pdMS_TO_TICKS(51));

  uint8_t rec_buf[6];
  ESP_ERROR_CHECK(i2c_master_receive(device->dev_handle, rec_buf, 6, 25));

  // TODO: Add CRC checking
  *raw_VOC = ((uint16_t)buf[0] << 8) | buf[1];
  *raw_NOX = ((uint16_t)buf[3] << 8) | buf[4];

  OLOGF("Read RAW Measurements: Received %x",
        ((uint32_t)*raw_VOC) << 16 | *raw_NOX);
}

uint8_t sgp41_execute_self_test(sgp41 *device) {
  ESP_ERROR_CHECK(
      i2c_master_transmit(device->dev_handle, (uint8_t *)"\x28\x0E", 2, 25));

  vTaskDelay(pdMS_TO_TICKS(321));

  uint8_t rec_buf[3];
  ESP_ERROR_CHECK(i2c_master_receive(device->dev_handle, rec_buf, 3, 25));

  if (rec_buf[1] & 0x03) {
    OLOGF("Self Test Failed: Received %x", rec_buf[1] & 0x03);
    return 0; // Return 0 if tests failed
  }
  OLOGF("Self Test Passed: Received %x", rec_buf[1] & 0x03);
  return 1;
}

void sgp41_turn_heater_off(sgp41 *device) {
  ESP_ERROR_CHECK(
      i2c_master_transmit(device->dev_handle, (uint8_t *)"\x36\x15", 2, 25));

  OLOG("Turned Off Heater");
}
