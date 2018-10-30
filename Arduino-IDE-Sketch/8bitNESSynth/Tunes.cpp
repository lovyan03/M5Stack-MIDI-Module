#include "Tunes.h"

volatile SemaphoreHandle_t Tunes::timerSemaphore;
portMUX_TYPE Tunes::timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t Tunes::isrCounter = 0;
volatile uint32_t Tunes::lastIsrAt = 0;
volatile uint16_t Tunes::osc1 = 0;
volatile uint16_t Tunes::osc2 = 0;
volatile uint16_t Tunes::osc3 = 0;
volatile uint16_t Tunes::d[3] = {0, 0, 0};
volatile uint16_t Tunes::voice[3] = {0, 0, 0};
volatile uint16_t Tunes::bnno[3] = {0, 0, 0};
volatile uint16_t Tunes::counter = 0;
volatile uint8_t Tunes::p1_wave_index = 2;
volatile uint8_t Tunes::duty_point = 0;
volatile uint8_t Tunes::duty_table [4][8] = {
  {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
  {0, 0, 0, 0, 0, 0, 1, 1}, // 25%
  {0, 0, 0, 0, 1, 1, 1, 1}, // 50%
  {1, 1, 1, 1, 1, 1, 0, 0}, // 25% (inv.)
};

int Tunes::PulseValues[4][256];

hw_timer_t* Tunes::timer;
unsigned long Tunes::tones[] = {
  25,
  26,
  27,
  28,
  30,
  32,
  34,
  36,
  38,
  40,
  43,
  45,
  48,
  51,
  54,
  57,
  60,
  64,
  68,
  72,
  76,
  80,
  85,
  90,
  95,
  101,
  107,
  114,
  120,
  127,
  135,
  143,
  152,
  161,
  170,
  180,
  191,
  202,
  214,
  227,
  241,
  255,
  270,
  286,
  303,
  321,
  340,
  360,
  382,
  405,
  429,
  454,
  481,
  510,
  540,
  572,
  606,
  642,
  680,
  721,
  764,
  809,
  857,
  908,
  962,
  1020,
  1080,
  1144,
  1212,
  1284,
  1361,
  1442,
  1528,
  1618,
  1715,
  1817,
  1925,
  2039,
  2160,
  2289,
  2425,
  2569,
  2722,
  2884,
  3055,
  3237,
  3429,
  3633,
  3849,
  4078,
  4320,
  4577,
  4850,
  5138,
  5443,
  5767,
  6110,
  6473,
  6858,
  7266,
  7698,
  8156,
  8641,
  9155,
  9699,
  10276,
  10887,
  11534,
  12220,
  12947,
  13717,
  14532,
  15396,
  16312,
  17282,
  18310,
  19398,
  20552,
  21774,
  23069,
  24440,
  25894,
  27433,
  29065,
  30793,
  32624,
  34564,
  36619
};

void Tunes::pinset(uint8_t p1_p, uint8_t p2_p, uint8_t n_p, uint8_t t_p) {
  p1_pin = p1_p;
  p2_pin = p2_p;
  n_pin = n_p;
  t_pin = t_p;

  sigmaDeltaSetup(p1_channel, 88200);
  sigmaDeltaAttachPin(p1_pin, p1_channel);
  sigmaDeltaWrite(p1_channel, 0);

  sigmaDeltaSetup(p2_channel, 88200);
  sigmaDeltaAttachPin(p2_pin, p2_channel);
  sigmaDeltaWrite(p2_channel, 0);

  sigmaDeltaSetup(n_channel, 88200);
  sigmaDeltaAttachPin(n_pin, n_channel);
  sigmaDeltaWrite(n_channel, 0);

  sigmaDeltaSetup(t_channel, 88200);
  sigmaDeltaAttachPin(t_pin, t_channel);
  sigmaDeltaWrite(t_channel, 0);

}

void Tunes::noteon(uint8_t mch, uint8_t nno, uint8_t vel) {
  voice[mch - 1] = nno;
  d[mch - 1] = (uint16_t)(Tunes::tones[nno]);
  Serial.print("ON");
  Serial.println(voice[0]);
}

void Tunes::noteoff(uint8_t mch, uint8_t nno, uint8_t vel) {
  if (voice[mch - 1] == nno) {
    Serial.print("OFF");
    Serial.println(voice[0]);
    voice[mch - 1] = 0;
    d[mch - 1] = 0;
    bnno[mch -1] = nno;
  }
}

void Tunes::onTimer() {
  portENTER_CRITICAL_ISR(&Tunes::timerMux);
  Tunes::isrCounter++;
  Tunes::lastIsrAt = millis();
  Tunes::osc1 += d[0];
  Tunes::osc2 += d[1];
  Tunes::osc3 += d[2];
  portEXIT_CRITICAL_ISR(&Tunes::timerMux);

  int out = 0;
  out += Tunes::PulseValues[p1_wave_index][(osc1 >> 8)];

  dacWrite(25, (out / 16));
}

void Tunes::init() {

  float ConversionFactor = (2 * PI) / 256;  // convert my 0-255 bits in a circle to radians
  float RadAngle;                           // Angle in Radians
  for (int p_select = 0; p_select < 4; p_select ++) {
    for (int MyAngle = 0; MyAngle < 256; MyAngle++) {
      Tunes::PulseValues[p_select][MyAngle] = duty_table[p_select][MyAngle / 32] < 1 ? 0 : 255;
      Serial.print(PulseValues[p_select][MyAngle]);
    }
    Serial.println(p_select);
  }
  Tunes::timerSemaphore = xSemaphoreCreateBinary();

  Tunes::timer = timerBegin(0, 80, true); // /80 prescale = 1us = 1/1000000s = 1MHz
  timerAttachInterrupt(Tunes::timer, &Tunes::onTimer, true);
  timerAlarmWrite(Tunes::timer, 45, true);
  timerAlarmEnable(Tunes::timer);
}

void Tunes::pause() {
  timerAlarmDisable(Tunes::timer);
}
void Tunes::resume() {
  timerAlarmEnable(Tunes::timer);
}

void Tunes::run() {
  // get semaphore to use GPIO registers (maybe?
  if (xSemaphoreTake(Tunes::timerSemaphore, 0) == pdTRUE) {
    uint32_t isrCount = 0, isrTime = 0;
    portENTER_CRITICAL(&Tunes::timerMux);
    // ==== critical section begin ====
    isrCount = Tunes::isrCounter;
    isrTime = Tunes::lastIsrAt;

    // ==== critical section end ====
    portEXIT_CRITICAL(&Tunes::timerMux);

  }
  // below script is safe?
}