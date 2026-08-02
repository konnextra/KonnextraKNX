# Hardware {#hardware}

> **This page is not finished.** The wiring details are still being written up. What is below
> is correct, it is just not the whole picture yet.

## What you connect

The library talks to a **transceiver**, a chip that handles the electrical side of the KNX bus,
over an ordinary serial port. It does not touch the bus itself, and it does not care which
transceiver you use as long as it speaks the TP-UART2 command set:

- the **%Konnextra Bridge**, an STKNX breakout board that presents that interface,
- or a plain TP-UART2 module.

Three wires between your board and the transceiver:

| | |
|---|---|
| **RX** | transceiver out, board in |
| **TX** | board out, transceiver in |
| **GND** | shared ground, not optional |

The transceiver takes its power from the KNX bus. Your board does not supply it.

Getting these three right is most of the job. Swapped RX and TX, or a missing ground, is the
usual reason a fresh build sends nothing and receives nothing. @ref troubleshooting has the
symptoms.

## Line settings

`begin()` opens the port at **19200 baud, 8E1**. Those settings are not negotiable, KNX does
not work at anything else, which is also why a software serial that cannot produce even parity
is not an option.

Which port that is, and how to name a different one, is on @ref boards. On ESP32 you will
usually want to move the pins to where you wired them, which is also described there.

There is **no reset line**. Everything between board and transceiver goes over the serial pins,
so a soft reset that goes unanswered is simply a failed `begin()`.

## The reference board

Development happens on a **Seeed XIAO ESP32-C6**, wired to the Bridge on **D7 (RX)** and
**D6 (TX)**. That is this board's wiring, not a library constant. Nothing in the library
mentions a pin number.

## Still to come

Bus supply and current draw, signal levels, the Bridge's pinout and mounting, and a wiring
diagram.
