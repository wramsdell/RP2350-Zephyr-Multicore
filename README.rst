RP2350 Zephyr Multicore
########################

Overview
********

Zephyr application for the Raspberry Pi Pico 2 (RP2350) that brings up a
Microchip LAN9250 SPI Ethernet controller and runs a DHCPv4 client over it.
The USB port is used as a CDC-ACM serial console/shell instead of a physical
UART.

It also demonstrates asymmetric multiprocessing on the RP2350: Core0
(Zephyr) loads and launches independent bare-metal C code onto Core1 at
boot, which blinks the onboard LED. A ``blink rate get|set <ms>`` shell
command on Core0 controls the blink rate via an inter-core mailbox. See
`THEORY_OF_OPERATION.md <THEORY_OF_OPERATION.md>`_ for the full source
layout, memory partition, Core1 load sequence, and mailbox protocol.

Hardware
********

- Board: Raspberry Pi Pico 2 (``rpi_pico2/rp2350a/m33``)
- LAN9250 Ethernet controller wired to SPI0:

  - CSN: GP17
  - SCK: GP18
  - TX (MOSI): GP19
  - RX (MISO): GP16
  - INT: GP20
  - RESET: GP22 (currently unused — the ``microchip,lan9250`` devicetree
    binding in this Zephyr version doesn't support a ``reset-gpios``
    property; see the overlay for details)

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

Notes
*****

``CONFIG_ENTROPY_RPI_PICO_RNG`` is explicitly disabled in ``prj.conf``. See
the comment there: enabling networking pulls in the RP2350 hardware RNG
driver by default, whose ``pico_rand`` module places an
``.uninitialized_data`` section inside the RAM data-copy region immediately
before ``usbd_context_area``, corrupting the CDC-ACM USB device context on
boot and preventing it from enumerating.
