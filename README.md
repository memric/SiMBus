# Simple ModBus

Minimal ModBus Client/Master code for MCU.
Implements Modbus RTU client on baremetal or OS systems and Modbus TCP on FreeRTOS and LwIP.

## How to use

- Add *simple_modbus_conf.h* configuration file to your project. Use *simple_modbus_conf_template.h* as template.
- Generate register map and gerister functions files with `RegGen.py` script in RegGen folder.
- Include generated files into your project build.
