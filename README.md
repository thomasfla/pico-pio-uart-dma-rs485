# pico-pio-uart-dma-rs485

Implementing multiple channel RS485 using Raspberry Pi Pico PIO and DMA.
This was inteded for low latency comunication with robotics actuator and sensors.

- For half-duplex RS485: only one device transmits at a time.
- The OE (Output Enable) pin controls RS485 driver direction.
- Supports multiple channels via separate PIO state machines.
- Tested at high baud rates (12.5 Mbps and likely higher).
- Waits for response with a configurable timeout.
- End-of-frame detection based on silence (similar to Modbus).
- Fully offloaded TX and RX: the CPU is not involved during transmission and reception of complete frames and responses.



## Building

Make sure you have the [Pico SDK](https://github.com/raspberrypi/pico-sdk) installed and initialized.

```sh
mkdir build
cd build
cmake ..
make
```

## Flashing

### Option 1: Drag-and-drop UF2

1. Hold the BOOTSEL button on your Pico and connect it to your computer via USB.
2. Release the button. The Pico will appear as a USB drive.
3. Copy the generated `.uf2` file from `build/examples/basic/` to the Pico drive.

### Option 2: Using `picotool`


## How It Works

Each RS485 channel uses a single PIO state machine to handle both reception and transmission in half-duplex. When we want to send a frame, we provide the data, the length of the frame, and the timeout to wait for a response. Everything else is handled automatically by the PIO and DMA.

First, the state machine takes care of transmitting the frame. It enables the driver (OE pin) during the send, then disables it when done. Right after that, it starts listening for a response. If a response comes in, the bytes are written directly into memory using DMA, without the CPU being involved. If there's no response, it waits until the timeout expires.

At any point, we can poll the state of the state machine to check if the transmission and reception are complete. We can also look at the DMA pointer to see how many bytes have been received. This way, the CPU only gets involved before and after the frame exchange.

### Limitations:

- At high baud rates, it is important that the PIO clock divisor is an integer or close to one. We can adjust the system clock to achieve that.

- The PIO runs at 8 times the baud rate. For example, with a 200 MHz system clock, the baud rate is limited to around 25 MHz.

- RX sampling uses non-blocking edge detection in the PIO to allow both timeout tracking and pin monitoring of a Start bit. This introduces a one-instruction jitter in the sampling time.

- Currently, no interrupt is generated after transmission, but it should be easy to add one.

- If the received frame is not a multiple of 32 bits, it will be padded with zeros before being sent to the DMA. We have no way to know the exact frame size, since it will be rounded up to the next multiple of 32 bits.