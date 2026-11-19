# HAB5-OBC-Flight-Code
 
This is the flight code for the HAB5 OBC.
 
This software is intended to run on the STM32F411CEU6 MCU.
 
## Setup - VSCode & STM32Cube Extension
- Open the cloned project
- Use the STM32Cube Extension "Set Up STM32Cube project(s)" tool, configure STM32F411CEU6 and GCC
- Command Palette "Developer: Reload Window" if needed
- Select Debug or Release build configuration when prompted (likely debug if doing development)
- Command Palette "CMake: Delete Cache and Reconfigure"
- Should build with "CMake: Build" build task

## Pinout

Pin assignments for the HAB5-OBC board (STM32F411CEU6, UFQFPN-48). Configure these in the `.ioc` file; drivers only receive peripheral handles and never set up pins themselves.

| Pin | Function | Notes |
|---|---|---|
| PA0 | ADC1_IN0 | LMT84LPM temperature sensor |
| PA1 | ADC1_IN1 | NTC divider |
| PA2 | EXTRA_GPIO_0 | Spare |
| PA3 | GPIO_EXTI3 | IMU INT2 |
| PA4 | SPI1_NSS | |
| PA5 | SPI1_SCK | |
| PA6 | SPI1_MISO | |
| PA7 | SPI1_MOSI | |
| PA8 | LED3 | |
| PA9 | USART1_TX | |
| PA10 | USART1_RX | |
| PA11 | LED0 | |
| PA12 | LED1 | |
| PA13 | SWDIO | Debug, do not reassign |
| PA14 | SWCLK | Debug, do not reassign |
| PA15 | LED2 | |
| PB0 | GPIO_EXTI0 | IMU INT1 |
| PB1 | LED4 | |
| PB2 | LED5 | |
| PB3 | I2C2_SDA | AF9, open-drain (see notes) |
| PB4 | GPIO_EXTI4 | |
| PB5 | GPIO_EXTI5 | |
| PB6 | I2C1_SCL | |
| PB7 | I2C1_SDA | |
| PB8 | LED6 | |
| PB9 | LED7 | |
| PB10 | I2C2_SCL | AF4 |
| PB12 | SPI2_NSS | |
| PB13 | SPI2_SCK | |
| PB14 | SPI2_MISO | |
| PB15 | SPI2_MOSI | |
| PC13 | EXTRA_GPIO_1 | Spare, low drive (see notes) |
| PH0 | RCC_OSC_IN | |
| PH1 | RCC_OSC_OUT | |
| NRST | NRST | |

**Notes**
- **I2C2** is used by the IMU, which plugs into female headers as a breakout board.
- **PB3** resets as JTAG `JTDO`/`TRACESWO`. It must be set to AF9 (I2C2_SDA) in CubeMX, which means SWO trace is unavailable. SWD and semihosting still work.
- **PC13** is in the backup domain: max ~3 mA sink, 2 MHz, and it can't source current. Don't drive LEDs or loads from it.
- **EXTI lines** are shared across ports by pin number (e.g. PA3 and PB3 both use EXTI3). PB3 is I2C, so it doesn't conflict with PA3. Before adding a new interrupt pin, check that its line number isn't already taken (0, 3, 4, 5 are in use).

## Adding a New Component
 
Each component (sensor, data logger, radio, etc.) is a driver made of a header (`.h`) and an implementation (`.c`) file. Its update function runs from a task in the scheduler. Use `foo.h`, `foo.c` and the `TEMPLATE` comments in `main.c` as a guide. Copy them and rename `foo`/`FOO` to your component's name.
 
### 1. Consider your component
Before writing code, work out:
- **What it does**: what it reads or writes, and how often (this sets the scheduler task interval).
- **Which interface it uses**: `I2C1`, `I2C2`, `SPI1`, `SPI2`, UART, ADC, or something else.
- **Which pins that interface uses**, and the device's address (for I2C) or chip-select pin (for SPI).
- **The registers it needs** from its datasheet: ID register, config registers, data registers, data format.
### Look for an existing driver first
Before writing a driver from scratch, check whether one already exists. A tested driver saves time and avoids subtle bugs in register setup and data conversion.
 
**Where to look:**
- **The manufacturer.** Many sensor makers publish official C drivers on GitHub (e.g. Bosch's `BMxxx_SensorAPI` repos, ST's `STMems_Standard_C_drivers`). These are usually the best option.
- **Breakout board sellers** (Adafruit, SparkFun). Their libraries are often written for Arduino (C++, `Wire.h`) and need porting, but they're useful as a reference for register setup and conversion maths.
- **GitHub / general search** for `<part number> STM32 HAL` or `<part number> driver C`.
**What to check before using one:**
- **Plain C, no Arduino or other framework dependencies.** If it uses `Wire`, `SPI.begin()`, `delay()`, or C++ classes, you'll have to port it.
- **Platform-independent design.** The best drivers let you supply your own read/write/delay functions (function pointers). You then implement those with `HAL_I2C_Mem_Read`, `HAL_SPI_TransmitReceive`, etc.
- **No `malloc`** and no long blocking delays in the regular read path. The scheduler is cooperative, so a driver that waits 100 ms for a conversion stalls everything else.
- **Licence.** Check that it allows use in this project (MIT, BSD and Apache are generally fine). Keep the original licence header in the files.
- **Maintained and used.** Recent commits, open issues answered, other people using it.
**Integrating it:**
- Put the vendor files in their own folder (e.g. `Drivers/BMP3_SensorAPI/`) rather than mixing them into `Core/`, so it's clear what's ours and what isn't.
- Still write a small `xxx.h`/`xxx.c` wrapper that follows the `foo` template (`XXX_Init`, `XXX_Update`, getters) and calls into the vendor driver. That keeps `main.c` and the scheduler the same for every component.
- In `CMakeLists.txt`, add the vendor `.c` files to `target_sources` **and** add their folder to `target_include_directories`, or the compiler won't find their headers.
- Note in the wrapper where the driver came from (URL and version or commit).
### 2. Configure the interface in CubeMX (if it's new)
If your component is on an interface that's already enabled (e.g. a second sensor on `I2C1`), skip this step. Wire it to the same pins and give it its own address.
 
Otherwise:
- Open the `.ioc` file in CubeMX, enable the peripheral, and assign its pins.
- Regenerate the code. This creates the handle (e.g. `hi2c2`), its `MX_..._Init()` function in `main.c`, and the pin setup in `stm32f4xx_hal_msp.c`.
Your driver never configures pins itself. It is only given the handle (e.g. `&hi2c1`), and CubeMX has already tied that handle to its pins.
 
> **Only write code between `USER CODE BEGIN` / `USER CODE END` comments** in CubeMX-generated files (`main.c`, `stm32f4xx_it.c`, etc.). Anything outside those blocks is deleted when the code is regenerated.
 
### 3. Add the header and C files
- Copy `foo.h` into `Core/Inc/` and `foo.c` into `Core/Src/`, renamed for your component.
- Every component needs at least:
  - `XXX_Init()`: runs once at startup, checks the device responds, configures it. Returns `HAL_OK` or `HAL_ERROR`.
  - `XXX_Update()` (or another main action): does one read/write and returns quickly. It must not block, because the scheduler is cooperative and a slow task delays every other task.
  - Getters for any results other code needs. They return cached values and don't touch the bus.
- Fill in the `TODO` sections: register map, config writes, data conversion.
### 4. Add the necessary code in `main.c`
Follow the `TEMPLATE` comments. All of it goes in USER CODE blocks:
 
| USER CODE block | What to add |
|---|---|
| `Includes` | `#include "xxx.h"` |
| `PV` | `XXX_HandleTypeDef xxx;` |
| `PFP` | `static void Task_XXX(void);` |
| `2` | `XXX_Init(...)`, then `Scheduler_AddTask(Task_XXX, interval_ms, "xxx")` only if init succeeded |
| `4` | `static void Task_XXX(void) { XXX_Update(&xxx); }` |
 
### 5. Update CMake
This is a CMake project, so new `.c` files are **not** picked up automatically. Add yours to `target_sources` in the top-level `CMakeLists.txt`:
 
```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/bmp280.c
    Core/Src/scheduler.c
    Core/Src/xxx.c
)
```
 
If you forget this step, the build fails at the link step with `undefined reference to 'XXX_Init'`.
 
After editing `CMakeLists.txt`, reconfigure if VS Code doesn't do it automatically (Command Palette → "CMake: Configure"), then build.
 
### Debug output
`printf` goes through semihosting (`rdimon`). It only produces output while a debugger is attached with semihosting enabled, and it may halt the board when no debugger is connected.