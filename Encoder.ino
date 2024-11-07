#include <lvgl.h>
#include <TFT_eSPI.h>
#include "CST820.h"
#include <Arduino.h>
#include "ui.h"
#include "AiEsp32RotaryEncoder.h"
#include <WiFi.h>
#include "rig_control.h"

#define ROTARY_ENCODER_A_PIN 35
#define ROTARY_ENCODER_B_PIN 22
#define ROTARY_ENCODER_BUTTON_PIN -1
#define ROTARY_ENCODER_STEPS 4

int frequencyStep = 100;
const char *ssid = "*****";  //Enter your WIFI info
const char *password = "*****";
const char* rig_ip = "192.168.1.119";// Enter your rig IP
const uint16_t rig_port = 4532;

int pulseThreshold = 10;
int pulseCounter = 0;

RigMode modes[] = {USB, LSB, CW, CWR, DIGI, AM};
int modeIndex = 0;  // Start at the first mode
int frequencySteps[] = {10, 100, 500, 1000};
int stepIndex = 0;  // Start at the first step
RigVFO currentVFO = VFOA;

WiFiClient rigClient;
unsigned long lastQueryTime = 0;
const unsigned long queryInterval = 100;

float currentFrequency = 0.0;

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);
bool localFrequencyUpdated = false;

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;
#define I2C_SDA 33
#define I2C_SCL 32
#define TP_RST 25
#define TP_INT 21

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

TFT_eSPI tft = TFT_eSPI();
CST820 touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  bool touched;
  uint8_t gesture;
  uint16_t touchX, touchY;
  touched = touch.getTouch(&touchX, &touchY, &gesture);

  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = 240 - touchX;
    data->point.y = 320 - touchY;
  }
}

void socketTask(void *parameter) {
  int cycleCounter = 0;

  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Reconnecting...");
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("WiFi reconnected.");
    }

    if (!rigClient.connected()) {
      Serial.println("Disconnected from rig, attempting to reconnect...");
      bool connected = rigClient.connect(rig_ip, rig_port);
      delay(200);
      if (connected) {
        Serial.println("Reconnected to rig");
      } else {
        Serial.println("Reconnection failed, retrying...");
        delay(1000);
      }
      continue;
    }

    unsigned long currentMillis = millis();
    if (currentMillis - lastQueryTime >= queryInterval) {
      lastQueryTime = currentMillis;
      while (rigClient.available()) {
        rigClient.read();
      }

      if (localFrequencyUpdated) {
        set_freq(currentFrequency);
        localFrequencyUpdated = false;
      } else {
        if (cycleCounter >= 5) {
          get_freq(&currentFrequency);
          cycleCounter = 0;
        } else {
          cycleCounter++;
        }
      }
    }
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setAcceleration(250);

  if (!rigClient.connect(rig_ip, rig_port)) {
    Serial.println("Failed to connect to rig");
  } else {
    Serial.println("Connected to rig");
  }

  xTaskCreate(socketTask, "Socket Task", 4096, NULL, 1, NULL);

  lv_init();
  tft.begin();
  tft.setRotation(1);
  tft.initDMA();
  touch.begin();

  buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 160, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 160, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (buf1 == NULL) {
    Serial.println("Buffer allocation failed!");
    while (true);
  }

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * 160);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  ui_init();
  lv_obj_add_event_cb(ui_Button2, button2_event_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Button5, button5_event_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_Button1, button1_event_handler, LV_EVENT_CLICKED, NULL);
  Serial.println("Setup done");
  tft.startWrite();
}

void loop() {
  lv_timer_handler();

  if (rotaryEncoder.encoderChanged()) {
    int32_t encoderDelta = rotaryEncoder.readEncoder();
    pulseCounter += abs(encoderDelta);

    if (pulseCounter >= pulseThreshold) {
      int direction = (encoderDelta > 0) ? 1 : -1;

      currentFrequency += direction * frequencyStep;
      float remainder = fmod(currentFrequency, frequencyStep);
      if (remainder != 0) {
        currentFrequency += (direction > 0) ? (frequencyStep - remainder) : -remainder;
      }

      pulseCounter = 0;
      rotaryEncoder.setEncoderValue(0);
      localFrequencyUpdated = true;
    }
  }

  int frequencyInt = static_cast<int>(currentFrequency);
  String frequencyStr = String(frequencyInt);
  lv_label_set_text(ui_Label1, frequencyStr.c_str());

  delay(5);
}

void button2_event_handler(lv_event_t *e) {
    modeIndex = (modeIndex + 1) % (sizeof(modes) / sizeof(modes[0]));
    set_mode(modes[modeIndex], 3000);
    delay(200);
}

void button5_event_handler(lv_event_t *e) {
    stepIndex = (stepIndex + 1) % (sizeof(frequencySteps) / sizeof(frequencySteps[0]));
    frequencyStep = frequencySteps[stepIndex];
    char labelText[20];
    snprintf(labelText, sizeof(labelText), "%d Hz", frequencyStep);
    lv_label_set_text(ui_Label5, labelText);
}

void button1_event_handler(lv_event_t *e) {
    currentVFO = (currentVFO == VFOA) ? VFOB : VFOA;
    set_vfo(currentVFO);
}
