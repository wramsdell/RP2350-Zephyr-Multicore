RP2350 Zephyr Multicore
########################

Overview
********

Reference implementation of asymmetric multiprocessing on the RP2350: Core0
(Zephyr) loads and launches independent bare-metal C code onto Core1 at
boot, which blinks the onboard LED. A ``blink rate get|set <ms>`` shell
command on Core0 controls the blink rate via an inter-core mailbox. The
USB port is used as a CDC-ACM serial console/shell instead of a physical
UART.

See `THEORY_OF_OPERATION.md <THEORY_OF_OPERATION.md>`_ for the full source
layout, memory partition, Core1 load sequence, and mailbox protocol.

For an example of building on this multicore plumbing for a real workload,
see `RP2350-Zephyr-Stepper
<https://github.com/wramsdell/RP2350-Zephyr-Stepper>`_, which forks this
project and replaces the LED blink demo with PIO-driven stepper motor
control.

Hardware
********

- Board: Raspberry Pi Pico 2 (``rpi_pico2/rp2350a/m33``)
- Onboard LED (GPIO25), driven entirely by Core1

Building and Flashing
**********************

Using the west workspace at ``~/zephyrproject`` with its venv:

.. code-block:: console

   source ~/zephyrproject/.venv/bin/activate
   west build -b rpi_pico2/rp2350a/m33 -d build

Flash by putting the board in UF2 bootloader mode (hold BOOTSEL while
plugging in) and copying the resulting image:

.. code-block:: console

   cp build/zephyr/zephyr.uf2 /media/<user>/RPI-RP2/

The board will reboot automatically and enumerate a USB CDC-ACM serial
console (``/dev/ttyACM0``).
