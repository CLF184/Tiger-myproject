#include "camera_module.h"

#include "esp_camera.h"

#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

#include <Arduino.h>
#include <cstring>

namespace camera_module {

static transmit_fn_t g_transmit = nullptr;
static bool g_camera_inited = false;
static SemaphoreHandle_t g_camera_mutex = nullptr;

static void ensure_camera_mutex() {
  if (!g_camera_mutex) {
    g_camera_mutex = xSemaphoreCreateMutex();
  }
}

void set_transmit(transmit_fn_t fn) {
  g_transmit = fn;
}

static void send_photo_task(void* pvParameters) {
  (void)pvParameters;

  if (!g_transmit) {
    vTaskDelete(nullptr);
    return;
  }

  ensure_camera_mutex();
  if (!g_camera_mutex) {
    vTaskDelete(nullptr);
    return;
  }

  if (xSemaphoreTake(g_camera_mutex, portMAX_DELAY) != pdTRUE) {
    vTaskDelete(nullptr);
    return;
  }

  // Lazy init: only initialize the camera when a capture is requested.
  if (!g_camera_inited) {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_VGA;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", err);
      xSemaphoreGive(g_camera_mutex);
      vTaskDelete(nullptr);
      return;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
      s->set_brightness(s, 1);
      s->set_saturation(s, 0);
    }
    g_camera_inited = true;
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    if (g_camera_inited) {
      esp_camera_deinit();
      g_camera_inited = false;
    }
    xSemaphoreGive(g_camera_mutex);
    vTaskDelete(nullptr);
    return;
  }

  g_transmit(reinterpret_cast<const char*>(fb->buf), static_cast<int>(fb->len), CAMERA_END);

  esp_camera_fb_return(fb);

  // Deinit after capture to stop background acquisition and avoid EOF-OVF spam.
  if (g_camera_inited) {
    esp_camera_deinit();
    g_camera_inited = false;
  }

  xSemaphoreGive(g_camera_mutex);
  vTaskDelete(nullptr);
}

void capture_command(const char* command) {
  (void)command;
  xTaskCreate(send_photo_task, "sendPhoto", 32768, nullptr, 1, nullptr);
}

void set_frame_size_command(const char* command) {
  sensor_t* s = esp_camera_sensor_get();
  if (!s) {
    return;
  }

  framesize_t frameSize;
  if (strcmp(command, "SETFRAMESIZE 96X96") == 0) {
    frameSize = FRAMESIZE_96X96;
  } else if (strcmp(command, "SETFRAMESIZE QQVGA") == 0) {
    frameSize = FRAMESIZE_QQVGA;
  } else if (strcmp(command, "SETFRAMESIZE QCIF") == 0) {
    frameSize = FRAMESIZE_QCIF;
  } else if (strcmp(command, "SETFRAMESIZE HQVGA") == 0) {
    frameSize = FRAMESIZE_HQVGA;
  } else if (strcmp(command, "SETFRAMESIZE 240X240") == 0) {
    frameSize = FRAMESIZE_240X240;
  } else if (strcmp(command, "SETFRAMESIZE QVGA") == 0) {
    frameSize = FRAMESIZE_QVGA;
  } else if (strcmp(command, "SETFRAMESIZE CIF") == 0) {
    frameSize = FRAMESIZE_CIF;
  } else if (strcmp(command, "SETFRAMESIZE HVGA") == 0) {
    frameSize = FRAMESIZE_HVGA;
  } else if (strcmp(command, "SETFRAMESIZE VGA") == 0) {
    frameSize = FRAMESIZE_VGA;
  } else if (strcmp(command, "SETFRAMESIZE SVGA") == 0) {
    frameSize = FRAMESIZE_SVGA;
  } else if (strcmp(command, "SETFRAMESIZE XGA") == 0) {
    frameSize = FRAMESIZE_XGA;
  } else if (strcmp(command, "SETFRAMESIZE HD") == 0) {
    frameSize = FRAMESIZE_HD;
  } else if (strcmp(command, "SETFRAMESIZE SXGA") == 0) {
    frameSize = FRAMESIZE_SXGA;
  } else if (strcmp(command, "SETFRAMESIZE UXGA") == 0) {
    frameSize = FRAMESIZE_UXGA;
  } else {
    return;
  }

  s->set_framesize(s, frameSize);
  Serial.printf("Frame size set to %s\n", command + 12);
}

void init() {
  // Kept for compatibility, but capture now does lazy init/deinit.
}

} // namespace camera_module
