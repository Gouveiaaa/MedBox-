// Teste independente do firmware MedBox: scanner I2C para HW-364A.
// Este sketch NAO corrige boot mode 7: o ESP deve aceitar upload antes.
#include <Wire.h>

constexpr uint8_t PINO_SDA = 14; // D6
constexpr uint8_t PINO_SCL = 12; // D5

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("MEDBOX - scanner I2C HW-364A"));
  Wire.begin(PINO_SDA, PINO_SCL);
}

void loop() {
  uint8_t dispositivos = 0;
  for (uint8_t endereco = 1; endereco < 127; endereco++) {
    Wire.beginTransmission(endereco);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("I2C encontrado em 0x"));
      if (endereco < 16) Serial.print('0');
      Serial.println(endereco, HEX);
      dispositivos++;
    }
    yield();
  }
  if (!dispositivos) {
    Serial.println(F("Nenhum I2C encontrado. Confira SDA=14 e SCL=12."));
  } else {
    Serial.println(F("OLED esperado: endereco 0x3C."));
  }
  delay(3000);
}
