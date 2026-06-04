#pragma once
#include "driver/i2c_types.h"
typedef struct sgp41 sgp41;

void sgp41_init(i2c_master_bus_handle_t bus_handle, uint32_t speed,
                sgp41 *device);

void sgp41_execute_conditioning(sgp41 *device, uint16_t default_rh_percent,
                                uint16_t default_temp_c_deg);

void sgp41_read_raw_measurements(sgp41 *device, uint16_t rh_percent,
                                 uint16_t temp_c_deg, uint16_t *raw_NOX,
                                 uint8_t *raw_VOC);

uint8_t sgp41_execute_self_test(sgp41 *device);

void sgp41_turn_heater_off(sgp41 *device);
