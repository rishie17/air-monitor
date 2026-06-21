#pragma once
#include <stdint.h>
typedef struct {
  uint16_t co2_ppm;
  uint16_t pm2p5_ug_m3;
  uint16_t pm10_ug_m3;
  uint16_t tvoc_ppm;
  uint16_t aqi;
  uint8_t RH_percent;
  int8_t temp_deg_c;
  // uint16_t voc_index;
  // uint16_t nox_index;
  uint16_t nox_ppm;
} Measurement;
