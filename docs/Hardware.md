# Hardware {#hardware}

> **This page is not finished.** The wiring details are still being written up. What is below
> is correct; it is just not the whole picture yet.

## What you connect

The library talks to a **transceiver** — a chip that handles the electrical side of the KNX
bus — over an ordinary serial port. It does not touch the bus itself, and it does not care
which transceiver you use as long as it speaks the TP-UART2 command set:

- the **%Konnextra Bridge**, an STKNX breakout board that presents that interface,
- or a plain TP-UART2 module.

Three wires between board and transceiver: **RX**, **TX**, and a common ground. The bus side
of the transceiver is powered from the KNX bus.

## The serial port

The port is chosen in your sketch, not by the library. Written without one, the node uses
`Serial1`; name another if the transceiver sits somewhere else:

```cpp
Konnextra knx("1.1.5");            // Serial1
Konnextra knx("1.1.5", Serial2);
```

`begin()` opens it at **19200 baud, 8E1**. Those settings are not negotiable — KNX does not
work at anything else — which is why a software serial that cannot produce even parity is not
an option.

On ESP32 the UART starts on the core's default pins, which are almost certainly not where you
wired it. Assign them before `begin()`:

```cpp
HardwareSerial knxPort(1);
Konnextra      knx("1.1.5", knxPort);

void setup() {
    knxPort.setPins(rxPin, txPin);
    knx.begin();
}
```

@ref boards covers which port each board family offers, and the one board that has none to
spare.

There is **no reset line**. Everything between MCU and transceiver goes over the serial pins,
so a soft reset that goes unanswered is simply a failed `begin()`.

## The reference board

Development happens on a **Seeed XIAO ESP32-C6**, wired to the Bridge on **D7 (RX)** and
**D6 (TX)**. Those are this board's wiring, not a library constant — nothing in the library
mentions a pin number.

## Still to come

Bus supply and current draw, signal levels, the Bridge's pinout and mounting, and a wiring
diagram.
