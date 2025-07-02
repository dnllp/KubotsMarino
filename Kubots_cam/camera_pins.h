#ifndef _CAMERA_PINS_H_
#define _CAMERA_PINS_H_

// Goouu ESP32-S3-CAM specific pin definitions
// Source: Commonly found configurations for Goouu ESP32-S3-CAM with OV2640 or OV3660
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5

#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM    13

// LED de Flash (linterna) - Asegúrate de que tu placa tenga este pin si lo vas a usar
// Si no estás usando un LED de flash, déjalo en -1
#define LED_GPIO_NUM     48 // Típicamente GPIO 48 para el flash LED en Goouu S3-CAM
// #define LED_GPIO_NUM     2 // Si no tienes un LED de flash o no quieres usarlo

#endif // _CAMERA_PINS_H_