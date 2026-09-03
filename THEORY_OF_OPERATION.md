# Theory of Operation: Core1 bare-metal blink demo

This document describes how Core0 (Zephyr) and Core1 (bare-metal) coexist on
the RP2350 in this project: the source layout, the memory partition, how
Core1's firmware gets built and loaded, and the wire protocol the two cores
use to talk to each other.

## Source structure

```
CMakeLists.txt                 Top-level app build; wires core1's build in
cmake/core1.cmake              Cross-compiles + embeds the core1 image
prj.conf                       Core0 (Zephyr) Kconfig
rpi_pico2_rp2350a_m33.overlay  Devicetree: SRAM split, mbox, USB console

src/                           Core0 (Zephyr) application sources
  main.c                       Calls core1_launch() + blink_mbox_init()
  core1_launch.h / .c          PSM/SIO launch handshake that starts core1
  core1_blob.c                 const uint8_t[] holding core1's compiled image
                                (via #include <core1_blob.bin.inc>, generated
                                at build time - see "Build & load" below)
  blink_shell.c / .h           mbox client + the `blink rate get|set` shell
                                commands

core1/                         Bare-metal core1 image sources (NOT Zephyr)
  linker.ld                    Flat linker script; must match the SRAM
                                carve-out in the devicetree overlay
  vectors.c                    16-entry Cortex-M33 vector table
  start.c                      Reset handler: zero .bss, call core1_main()
  core1_main.c                 GPIO25 bring-up, blink loop, mailbox polling
  regs.h                       Minimal raw register definitions (see below
                                for why this doesn't use the Pico SDK's
                                hardware/structs headers)
  mailbox_proto.h              Wire protocol shared by both cores' code
```

Everything under `core1/` is compiled completely separately from the Zephyr
application — no Zephyr headers, no RTOS, no C library beyond a couple of
freestanding-safe standard headers (`<stdint.h>`). It is cross-compiled by
`cmake/core1.cmake` using the same Zephyr SDK toolchain, but as a bare
freestanding ELF/binary, and only its *output bytes* end up inside the
Zephyr (Core0) image, via `src/core1_blob.c`.

### Why `core1/regs.h` doesn't use the Pico SDK's `hardware/structs/*.h`

The obvious approach would be to include the vendored Pico SDK's
`hardware/structs/sio.h` etc., like the Core0-side code does. That doesn't
work for Core1's build: those headers pull in `hardware/address_mapped.h`,
which includes `pico.h`, which in this Zephyr checkout chains into
Zephyr-only shim headers
(`zephyr/modules/hal_rpi_pico/pico/config_autogen.h` →
`<zephyr/toolchain.h>` and friends) that simply don't exist outside a normal
Zephyr compilation unit.

The plain `hardware/regs/*.h` headers, by contrast, are pure, dependency-free
macro files (register offsets and bitmasks only, `#include`ing nothing but
each other). `core1/regs.h` includes those directly and defines a handful of
`REG()`/`REG_SET()`/`REG_CLR()` helpers on top, giving Core1 register access
without dragging in any Zephyr or Pico-SDK-runtime machinery. Every
offset/bitmask used was cross-checked against the real generated headers at
`modules/hal/rpi_pico/src/rp2350/hardware_regs/include/hardware/regs/`.

Core0's code (`src/core1_launch.c`) *does* use the full `hardware/structs/*.h`
headers, because it's a normal part of the Zephyr build and already has the
whole Zephyr include environment available — no issue there.

## Memory partition

Zephyr models RP2350's 520KB of SRAM as a single flat `sram0` devicetree
node (`0x20000000`-`0x20082000`), with no notion of the chip's individual
SRAM banks. `CONFIG_SRAM_SIZE`/`CONFIG_SRAM_BASE_ADDRESS` are Kconfig
defaults derived directly from that node's `reg` property, and both the ARM
linker script and the RP2350 MPU region builder consume those Kconfig values
directly. So the devicetree overlay simply shrinks `&sram0`'s `reg`:

```dts
&sram0 {
    reg = <0x20000000 DT_SIZE_K(392)>;
};
```

This confines Zephyr's linker layout *and* its MPU SRAM region to the bottom
392KB (`0x20000000`-`0x20062000`), leaving the top 128KB
(`0x20062000`-`0x20082000`, two 64KB banks) completely outside anything
Zephyr's build system or runtime memory protection touches. `core1/linker.ld`
targets exactly that region:

```
MEMORY { CORE1_RAM (rwx) : ORIGIN = 0x20062000, LENGTH = 128K }
```

within which the two 64KB banks are used as:
- Bank at `0x20062000`: vector table + `.text`/`.rodata`/`.data`
- Bank at `0x20072000`: `.bss` + stack (stack grows down from the top of
  SRAM, `__core1_stack_top`)

Neither core's Zephyr devicetree/Kconfig ever references GPIO25 (the onboard
LED) — Core1 owns that pin exclusively via its own per-core SIO block,
entirely independent of Zephyr's GPIO driver.

## Build & load: how Core1's code gets onto the chip

There is exactly one build (`west build`) and one flash (one `zephyr.uf2`).
Core1's firmware is embedded inside Core0's image and copied into place by
Core0 at runtime:

1. **Cross-compile** (`cmake/core1.cmake`): the three `core1/*.c` sources are
   compiled and linked against `core1/linker.ld` with the same
   `arm-zephyr-eabi-gcc`/`objcopy` Core0 uses (resolved automatically by
   `find_package(Zephyr REQUIRED)`), but with `-ffreestanding -nostdlib
   -nostartfiles` and none of Zephyr's normal compile flags — producing
   `core1.elf`, then `core1.bin` (a raw flat binary; link address == load
   address, so no relocation is needed).
2. **Embed**: Zephyr's own `generate_inc_file_for_target()` CMake helper
   (the same mechanism used elsewhere in the tree, e.g.
   `zephyr/modules/hal_infineon/cat1cm0p/CMakeLists.txt`, for embedding a
   co-processor firmware blob) turns `core1.bin` into a generated
   `core1_blob.bin.inc` — a plain comma-separated byte list.
   `src/core1_blob.c` `#include`s it into a `const uint8_t core1_blob[]`
   array, which becomes part of the ordinary Core0 Zephyr binary and thus
   part of `zephyr.uf2`.
3. **Load at boot** (`core1_launch()` in `src/core1_launch.c`, called near
   the top of Core0's `main()`): `memcpy()`s `core1_blob` verbatim into
   `0x20062000`, reads the initial stack pointer and entry point directly
   out of the copied vector table's first two words (so Core0 never needs
   any build-time symbol coupling to Core1's internals), then performs the
   RP2350 bootrom's documented core1 launch handshake.

### The launch handshake

This reimplements, directly against the raw `PSM`/`SIO` registers, the same
protocol the Pico SDK's `pico_multicore` library uses
(`multicore_reset_core1()` + `multicore_launch_core1_raw()` in
`modules/hal/rpi_pico/.../pico_multicore/multicore.c` — read as a reference,
not linked into this build, so Zephyr's CMake stays untouched by
`pico_multicore`'s own dependencies):

1. **Reset Core1**: set `PSM_FRCE_OFF_PROC1` (via the atomic set/clear
   register-alias addresses) to force Core1 into reset, then clear it to
   release. Core1's own boot ROM drains its inbound FIFO and pushes a single
   `0` word to confirm it's alive; Core0 waits for that word.
2. **Push the launch sequence**: six words —
   `{0, 0, 1, vector_table, sp, entry}` — one at a time over the SIO FIFO.
   After each word, Core0 waits for Core1 to echo the *same* value back over
   the FIFO before advancing to the next; any mismatch restarts the sequence
   from the beginning (`seq = 0`). This is the RP2350 bootrom's own
   documented handshake for bringing up a second core at an arbitrary
   entry point — the two leading zero words exist to flush/synchronize the
   FIFO state before the real payload.
3. Once the sequence completes, Core1 is executing its own reset handler
   independently, with its own stack, running entirely outside Zephyr.

## Inter-core communication: the mailbox

Core0 and Core1 exchange a single 32-bit word per direction over the
RP2350's SIO inter-core FIFOs — separate hardware FIFOs per direction, but
each core addresses "its" TX/RX side via the same register names
(`fifo_wr`/`fifo_rd`/`fifo_st`), so the hardware handles the crossover.

### Two implementations of the same wire format

- **Core0 side**: uses Zephyr's existing `mbox` driver for this SoC
  (`drivers/mbox/mbox_rpi_pico.c`), enabled via `&mbox { status = "okay"; }`
  in the overlay and `CONFIG_MBOX=y` in `prj.conf`. This driver is a thin,
  IRQ-driven wrapper: `mbox_send()` writes `*(uint32_t *)msg->data` straight
  to `sio_hw->fifo_wr`; the RX ISR reads `sio_hw->fifo_rd` and hands it to a
  registered callback. Zero framing beyond the raw 32-bit payload.
- **Core1 side**: being bare-metal, it can't use the Zephyr mbox subsystem —
  it pokes the identical underlying `SIO_FIFO_WR`/`SIO_FIFO_RD`/`SIO_FIFO_ST`
  registers directly (via `core1/regs.h`), polling `SIO_FIFO_ST_VLD_BITS`
  (RX has data) from inside its blink-delay loop, so it stays responsive
  without needing its own interrupt handling.

Because both sides speak the same zero-framing raw-word protocol, they
interoperate without sharing any driver code.

### Message format (`core1/mailbox_proto.h`)

A single 32-bit word, shared verbatim by both sides' source:

| Bits | Meaning |
|---|---|
| 31 | `0` = GET request (Core0→Core1) or a reply (Core1→Core0); `1` = SET request (Core0→Core1) |
| 30:0 | For a SET request: the new rate in milliseconds. For a reply: the *current* rate in milliseconds. |

Core0 always sends exactly one request word and then waits for exactly one
reply word. Core1 always replies with the current rate — after applying the
update, if the request was a SET — so Core0 decodes every reply identically
regardless of what it asked. `blink_shell.c`'s `blink_roundtrip()` makes this
synchronous from the shell command's point of view: it sends the request,
then blocks on a semaphore (given by the mbox RX callback) with a 1-second
timeout, so `blink rate get`/`blink rate set <ms>` are ordinary,
immediate-feeling shell commands even though the actual reply arrives
asynchronously via IRQ.

Sequencing note: `core1_launch()` runs before `blink_mbox_init()` enables the
mbox IRQ, so there's no need to save/restore the SIO FIFO IRQ enable state
around the launch handshake (Core1 isn't listening for mailbox traffic
until after it's already running).

### Core1's blink loop

Core1 toggles GPIO25 via `SIO_GPIO_OUT_XOR`, then busy-polls a
microsecond-resolution delay (driven by `TIMER0`, whose reset Core1 releases
itself on startup — Zephyr's own kernel tick uses the Cortex-M SysTick, not
TIMER0, so TIMER0 is otherwise completely unused and left in reset) for the
current rate. On *every* iteration of that delay loop it also checks for a
pending FIFO message, so a rate change takes effect within roughly one
polling tick rather than only at the end of whatever the *previous*
(possibly much longer) blink period was.
