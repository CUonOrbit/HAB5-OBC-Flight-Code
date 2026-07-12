# HAB5-OBC-Flight-Code

This is the flightcode for the HAB5 OBC.

This software is intented to run on the STM32F411CEU6 MCU.

## Setup - VSCode & STM32Cube Extension
- Open the cloned project
- Use the STM32Cube Extension "Set Up STM32Cube project(s)" tool, configure STM32F411CEU6 and GCC
- Command Palette "Developer: Reload Window" if needed
- Select Debug or Release build configureation when prompted (likely debug if doing development)
- Command Palette "CMake: Delete Cache and Reconfigure"
- Should build with "CMake: Build" build task
