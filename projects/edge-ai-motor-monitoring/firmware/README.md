# Firmware: STM32H573 · FreeRTOS · X-CUBE-AI

The application code that runs the sensing, the feature extraction, the inference and the web dashboard on an **STM32H573I-DK** board (Cortex-M33 at 250 MHz).

**▶ [Interactive firmware map](https://heet4226.github.io/EDGE-AI-FOR-ANOMALY-DETECTION/edge-ai-motor-monitoring/firmware.html):** a live timing simulation of the tasks, buffers and data flow.

## Tasks

| Task | Period | Job |
|---|---|---|
| Motor | 5 ms | PWM ramp and direction control (BTS7960 H-bridge) |
| Current | 5 ms | INA260 current and voltage over I²C (DMA) |
| Vibration | 5 ms | LIS3DH accelerometer over SPI (DMA) |
| Temperature | ≈ 800 ms | DS18B20 over 1-Wire |
| Data processing | 15 ms | builds one 14-feature vector and keeps a 30-vector sliding window |
| AI inference | every 10th vector (150 ms) | runs the network, combines its 3 outputs into one decision, streams CSV over UART |
| Ethernet | event-driven | lwIP HTTP server for the live dashboard (`GET /data`) |

The time from power-on to the first decision is about 1.2 s, which is what it takes to fill the first window.

## Folder contents

```
firmware/
├── Core/Src/main.c, app_freertos.c     clock, peripherals and task creation
├── Threads/motor/                      PWM ramp and direction
├── Threads/current_sensor/             INA260 driver and electrical features
├── Threads/vibration_sensor/           LIS3DH driver and vibration statistics
├── Threads/temp_sensor/                DS18B20 driver and temperature slope
├── Threads/RTI/                        real-time inference glue
├── Threads/Ethernet/lwip/              HTTP server, SSI handler and the dashboard page (webpage.c)
├── X-CUBE-AI/App/app_x-cube-ai.c       model runtime integration
└── motor_monitor.ioc                   STM32CubeMX configuration
```

## Not included

- **Vendor code** (STM32 HAL, CMSIS, FreeRTOS kernel, lwIP stack, X-CUBE-AI runtime). Regenerate it by opening `motor_monitor.ioc` in STM32CubeMX or STM32CubeIDE.
- **The generated network files** (`dann_hybrid*.c/h`) and the normalization and threshold header (`motor_model_params.h`). Regenerate them from the exported `.tflite` model of [`models/19_mlp_hybrid`](../models/19_mlp_hybrid/) with X-CUBE-AI. The notebook writes the parameter header.
