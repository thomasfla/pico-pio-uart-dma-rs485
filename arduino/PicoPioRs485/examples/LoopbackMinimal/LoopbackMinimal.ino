#include <PicoPioRs485.h>
#include "hardware/clocks.h"

constexpr uint32_t BUS_BAUD = 12000000;
constexpr uint8_t MASTER_TX = 0;
constexpr uint8_t MASTER_OE = 1;
constexpr uint8_t MASTER_RX = 2;
constexpr uint8_t RESP_TX = 3;
constexpr uint8_t RESP_OE = 4;
constexpr uint8_t RESP_RX = 5;

PicoPioRs485 master;
PicoPioRs485 responder;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    delay(10);
  }

  // Running the RP2040 at 200 MHz keeps the PIO clock divisors clean at 12 Mbps.
  set_sys_clock_khz(200000, false);

  master.begin(pio0, 0, MASTER_TX, MASTER_OE, MASTER_RX, BUS_BAUD);
  responder.begin(pio0, 1, RESP_TX, RESP_OE, RESP_RX, BUS_BAUD, &master);

  Serial.println("LoopbackMinimal ready. Wire the RS485 transceivers for master/responder roles.");
}

void loop() {
  static const uint8_t pingPayload[] = {0x10, 0x11, 0x12, 0x13};
  static const uint8_t responsePayload[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

  master.prepareTxPacket(pingPayload, sizeof(pingPayload), 1000);
  responder.prepareTxPacket(responsePayload, sizeof(responsePayload), 1000);

  master.sendPacket();
  master.waitTxDone();
  responder.sendPacket();

  int bytesReceived = master.waitForResponse();
  Serial.print("Master received ");
  Serial.print(bytesReceived);
  Serial.println(" bytes:");
  for (int i = 0; i < bytesReceived; ++i) {
    if (master.rxBuffer()[i] < 16) Serial.print('0');
    Serial.print(master.rxBuffer()[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
  delay(500);
}
