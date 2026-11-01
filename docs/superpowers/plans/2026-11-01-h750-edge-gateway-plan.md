# H750 Edge Gateway Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task. The user approved the design and explicitly requested execution on 2026-11-01; proceed inline without another approval gate.

**Goal:** Deliver a locally buildable and flashed Polaris H750 RGB-screen gateway derived from zgd-dev/-V2.0, publish a clean GitHub repository, and document all remaining external hardware.

**Architecture:** Preserve the upstream LVGL/FreeRTOS/Modbus/MQTT functions, replace board dependencies with the verified Polaris H750 BSP, and separate executable QSPI storage from SPI data storage. Missing sensors and Wi-Fi must remain explicit offline states.

**Tech Stack:** STM32H750XBHx, Keil MDK 5.39 / ARMCC 5.06u6, H7 DFP 4.1.3, HAL, FreeRTOS, LVGL 8, RGB565 800x480.

**Spec:** ../specs/2026-11-01-h750-edge-gateway-design.md

## Global Constraints

- Target directory: `C:/Users/Administrator/Documents/ChatGPT/FAE/edge-gateway-h750`, a separate Git repository; preserve upstream and vendor license notices.
- QSPI code remains at 0x90000000; data Flash logs at 0x01800000..0x01C00000 and cache at 0x01C00000..0x02000000; no whole-chip erase.
- Only ESP8266 Wi-Fi and onboard USART2 RS485; Ethernet disabled. USART3 Wi-Fi, USART1 debug.
- No credentials in source/history; configuration defaults empty and private local overrides ignored.
- Preserve QoS0 compatibility. Consume cached records only after the transport reports success; a failed or uncertain send retains the record. Do not describe this as a cloud persistence acknowledgement or exactly-once delivery.
- Current hardware lacks Wi-Fi/sensors. Real end-to-end acquisition/cloud retransmission stays explicitly unverified.

## Review Focus

- Addresses above 16MB must not wrap: test bounded partition accesses and 4-byte SPI transactions.
- Interrupted writes/restarts must not consume unread cache: host Flash-model tests with partially written records.
- Missing UART peripherals must time out without blocking the GUI: hardware diagnostics and bounded receive tests.
- H7 SysTick/interrupt priority/cache configuration must allow RTOS scheduling and coherent RGB output: build plus runtime heartbeat/display verification.
- Malformed Modbus/MQTT or overflowing ring buffers must not read beyond buffers: protocol tests with short, CRC-invalid and oversized inputs.

## Task 1: H750 board baseline and reproducible build

Files: `Drivers/`, `User/`, `Projects/MDK-ARM/edge_gateway_h750.uvprojx`, `scripts/build.ps1`, `.gitignore`.

- [ ] Copy required H750 HAL/CMSIS/BSP/system sources from the local verified touchscreen example; exclude outputs, user settings and unrelated middleware.
- [ ] Add upstream LVGL and FreeRTOS with their licenses; select an appropriate M7 port.
- [ ] Configure QSPI startup, SDRAM, LTDC framebuffer/cache and the 800x480 screen.
- [ ] Build from the new directory using the existing compiler and H7 support pack; resolve actual errors.

## Task 2: Bounded storage and protocol core

Files: `App/gateway_protocol.[ch]`, `App/gateway_store.[ch]`, `Port/gateway_board.[ch]`, `tests/test_core.py`.

Interfaces: protocol CRC/frame validation, bounded Flash read/write/erase, log append/read, cache peek/ack; peek never consumes.

- [ ] Add failing host tests for CRC, truncated frames, partition edges, cache acknowledgement and interrupted records.
- [ ] Implement these interfaces with a mockable storage backend; run the same logic in host tests and firmware.
- [ ] Bind data storage to onboard SPI2 W25Q256 with four-byte addressing and bounded waits; integrate PCF8574 RS485 direction and USART3 receive ring.
- [ ] Run host tests and perform firmware build; commit this deliverable.

## Task 3: RTOS gateway and RGB UI

Files: `App/gateway_tasks.c`, `App/gateway_ui.c`, `App/gateway_network.c`, `Port/lv_port_h750.c`, `User/FreeRTOSConfig.h`, `config/gateway_config.example.h`.

- [ ] Integrate real sensor polling, LVGL display/touch, MQTT transport/reconnect and offline cache through task-safe queues/interfaces.
- [ ] Adapt main/log pages for 800x480. Show absent sensors/network honestly; never populate synthetic measurements as real data.
- [ ] Add observable task heartbeats, UART diagnostics, conditional watchdog feeding and idle screen/wake handling.
- [ ] Build and run host protocol/storage checks, then commit.

## Task 4: Board verification and Chinese hardware guide

Files: `docs/verification.md`, `docs/hardware-and-wiring.md`, `docs/shopping-list.md`, `README.md`, `THIRD_PARTY_NOTICES.md`.

- [ ] Flash through DAP with internal/QSPI algorithms and require Verify OK.
- [ ] Read runtime display ID/resolution, data Flash ID, storage self-test and task health using serial/debug evidence; obtain user confirmation for physical display/touch where necessary.
- [ ] Fix diagnosed failures and repeat only relevant checks.
- [ ] Record exact observed results and unverified external paths. Write full wiring/BOM including voltage, module adapters, jumpers, termination and already-onboard hardware.

## Task 5: Review and GitHub delivery

- [ ] Perform an independent final code review under the review skill; address material findings and rerun affected checks.
- [ ] Scan tracked files and history for upstream/example/private credentials and generated binaries; retain safe example config only.
- [ ] Use the user's existing Edge GitHub session if available to create `edge-gateway-h750` under the verified account; require user handling of any interactive authentication.
- [ ] Push the clean repository using an authorized authenticated Git path; verify remote source/docs and provide repository plus local firmware paths.
- [ ] Report successful hardware checks separately from features awaiting the purchased modules.
