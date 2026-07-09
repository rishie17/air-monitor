file(GLOB SRC_FILES "./src/*")
idf_component_register(SRCS ${SRC_FILES}
  INCLUDE_DIRS "." REQUIRES esp_driver_gpio bt nvs_flash esp_driver_i2c
  driver_ssd1306 driver_sgp41 driver_scd40 driver_bmv080)
