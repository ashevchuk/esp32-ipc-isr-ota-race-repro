# esp32-ipc-isr-ota-race-repro

Minimal reproduction for [espressif/esp-idf#19487](https://github.com/espressif/esp-idf/issues/19487):
on ESP32 (original silicon, dual core), calling `esp_ota_get_state_partition()`
on the non-running OTA slot from tasks on both cores, while Arduino's WiFi
stack is active, crashes within about a second on ESP-IDF v6.1.1. It worked
fine on v6.1.0.

Two tasks, one pinned to each core, each loop `esp_ota_get_state_partition()`
on the partition returned by `esp_ota_get_next_update_partition(NULL)`.
`WiFi.begin()` is called with placeholder (non-existent) credentials — it
never associates, but the Arduino WiFi/network task stack still comes up and
keeps retrying. That's enough: removing the `WiFi.begin()` call entirely
(same two tasks, same partition table) ran 6000+ calls per core over 14s
with no crash.

## Build

```
source $IDF_PATH/export.sh
idf.py build
```

If this is the first build, the component manager fetches `arduino-esp32`
into `managed_components/`, which isn't tracked here. Its own `ESP_I2S.cpp`
trips a pre-existing, unrelated `-Werror=missing-field-initializers` on
ESP-IDF 6.1.x (not specific to this bug — same issue the parent project
documents). If the build fails on that, append this line to
`managed_components/espressif__arduino-esp32/CMakeLists.txt` and rebuild:

```cmake
target_compile_options(${COMPONENT_TARGET} PRIVATE -Wno-error=missing-field-initializers)
```

## Flash and observe

```
idf.py -p <port> flash
idf.py -p <port> monitor
```

Expect a hard reset within ~1 second:

```
rst:0x8 (TG1WDT_SYS_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
W (70) boot.esp32: PRO CPU has been reset by WDT
W (75) boot.esp32: WDT rst info: PRO CPU PC=0x40081ca5
W (79) boot.esp32: WDT rst info: APP CPU PC=0x400d491e
```

Both PCs land in `esp_system/port/panic_handler.c` (core0 at the first line
of `panicHandler()`, core1 in the "pause the non-offending core" branch for
an `INTWDT_CPU0` cause) — core0 tripped the interrupt watchdog, most likely
stuck in one of `esp_ipc_isr.c`'s own busy-wait loops. No usable coredump is
written for this particular crash shape (it's a hardware watchdog reset, not
a software panic), so this reading is from the two PCs alone.

Tested on ESP32-D0WD (revision v1.1), ESP-IDF v6.1.1, `arduino-esp32 3.3.11`.
