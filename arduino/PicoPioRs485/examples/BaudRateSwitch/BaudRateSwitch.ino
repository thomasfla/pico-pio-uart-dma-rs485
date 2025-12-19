#include <PicoPioRs485.h>
#include <cstdlib>
#include "hardware/clocks.h"

constexpr uint32_t DEFAULT_BUS_BAUD = 6000000;
constexpr uint8_t TX_PIN = 0;
constexpr uint8_t OE_PIN = 1;
constexpr uint8_t RX_PIN = 2;

PicoPioRs485 bus;
char commandBuffer[16];
size_t commandIndex = 0;

void printClockDivisor(const PicoPioRs485 &iface) {
  const RS485_Config &cfg = iface.rawConfig();
  float divisor = static_cast<float>(cfg.pio->sm[cfg.sm].clkdiv) / (1 << 16);
  Serial.print("Current SM divisor: ");
  Serial.println(divisor, 6);
  Serial.println("Aim for a near-integer divisor to minimize UART jitter.");
}

void handleSerialCommand() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (commandIndex > 0) {
        commandBuffer[commandIndex] = '\0';
        float valueMbps = atof(commandBuffer);
        if (valueMbps >= 1.0f && valueMbps <= 20.0f) {
          uint32_t newBaud = static_cast<uint32_t>(valueMbps * 1000000.0f);
          bus.setBaudrate(newBaud);
          Serial.print("Switched to ");
          Serial.print(valueMbps, 2);
          Serial.println(" Mbps");
          printClockDivisor(bus);
        } else {
          Serial.println("Enter a value between 1 and 20 Mbps");
        }
        commandIndex = 0;
      }
    } else if (commandIndex < sizeof(commandBuffer) - 1) {
      commandBuffer[commandIndex++] = c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    delay(10);
  }

  set_sys_clock_khz(200000, false);
  bus.begin(pio0, 0, TX_PIN, OE_PIN, RX_PIN, DEFAULT_BUS_BAUD);

  Serial.println("BaudRateSwitch ready.");
  Serial.println("Type the desired bus speed in Mbps (e.g. 6.25) and press enter.");
  printClockDivisor(bus);
}

void loop() {
  handleSerialCommand();

  static unsigned long lastSend = 0;
  if (millis() - lastSend >= 1000) {
    static const uint8_t payload[] = {0x42, 0x24, 0x00, 0x01};
    if (bus.prepareTxPacket(payload, sizeof(payload), 1200)) {
      bus.sendPacket();
      int rxLen = bus.waitForResponse();
      Serial.print("Bus echoed ");
      Serial.print(rxLen);
      Serial.println(" bytes.");
    }
    lastSend = millis();
  }
}
