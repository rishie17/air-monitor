#pragma once
#include "driver/i2c_types.h"
#include "stdint.h"
#include <esp_err.h>
struct scd40 {
  i2c_master_dev_handle_t dev_handle;
};

typedef struct scd40 scd40;

esp_err_t scd40_init(i2c_master_bus_handle_t bus_handle, uint32_t speed,
                     scd40 *device);
esp_err_t scd40_start_periodic_measurement(scd40 *device);
esp_err_t scd40_start_low_power_periodic_measurement(scd40 *device);
esp_err_t scd40_stop_periodic_measurement(scd40 *device);

/*
 * Returns esp_err_t for handling I2C issues
 * Make sure the Sensor is placed in any measurement mode
 * else, you'll receive a NACK from the sensor, and error from
 * this function along with marking all measurements "0"
 */
esp_err_t scd40_read_measurement(scd40 *device, int8_t *temp_c_deg,
                                 uint8_t *humidity_rh_percent,
                                 uint16_t *co2_ppm);
/*
 * Returns non-zero if the data is ready.
 * Can be called in a loop for polling
 * All errors when interfacing with the sensor are ignored in this command
 */
uint8_t scd40_get_data_ready_status(scd40 *device);

/*
 * Returns esp_err_t for handling I2C issues
 * It's ideal to call this when only the uC power cycles,
 * leaving the state of the sensor unknown to the uC
 */
esp_err_t scd40_reinit(scd40 *device);
