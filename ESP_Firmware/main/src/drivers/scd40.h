#pragma once
#include "driver/i2c_types.h"
#include "stdint.h"
typedef struct scd40 scd40;

void scd40_init(i2c_master_bus_handle_t bus_handle, uint32_t speed,
                scd40 *device);
void scd40_start_periodic_measurement(scd40 *device);
void scd40_start_low_power_periodic_measurement(scd40 *device);
void scd40_stop_periodic_measurement(scd40 *device);
void scd40_read_measurement(scd40 *device, int32_t *temp_c_deg,
                            int32_t *humidity_rh_percent, uint16_t *co2_ppm);
uint8_t scd40_get_data_ready_status(scd40 *device);
