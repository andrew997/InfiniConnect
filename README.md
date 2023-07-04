# InfiniConnect: Wireless Connectivity for Carrier Infinity/Bryant Evolution Systems

## Introduction

This project was created after a rat chewed through the HVAC wiring in my home.  Running a new thermostat wire to the outdoor unit would have been
expensive and time consuming (punching holes through walls to fish wire, drywall patching/texturing/painting, etc.)

InfiniConnect uses [Silicon Labs'](https://www.silabs.com/) proprietary radio stack, [Connect](https://www.silabs.com/developers/flex-sdk-connect-networking-stack), to adapt the serial protocol used by Carrier's Infinity HVAC system to wireless.  This eliminates the need to run copper wire between the Thermostat and the outdoor unit.

Credits to the [infinitude project](https://github.com/nebulous/infinitude/) for providing much of the technical background necessary to understand
Carrier's protocol.

## Prerequisites

- Two [EFR32xG23 +14 dBm Pro Kits](https://www.silabs.com/development-tools/wireless/efr32xg23-pro-kit-14-dbm?tab=overview), although the more inexpensive [FG23-DK2600A](https://www.silabs.com/development-tools/wireless/proprietary/efr32fg23-868-915-mhz-14-dbm-dev-kit?tab=overview) is also an option
- [Simplicity Studio v5 development environment](https://www.silabs.com/developers/simplicity-studio)
- Two RS485-to-TTL converter boards [such as these](https://www.amazon.com/ALMOCN-Adapter-Module-Converter-Indicator/dp/B08XLT21S6)
- A power supply capable of supplying either 5V or 3.3V from 240VAC power

## Background

The Carrier Infinity / Bryant Evolution HVAC system uses a proprietary packet-based serial protocol over existing low voltage household HVAC wiring, using
the RS485 standard and running at 38400 baud.  Data packets are exchanged between the Thermostat, Furnace and the outdoor unit/compressor. The packets are 
made of a one byte header, a payload of up to 255 bytes and a 2 byte checksum.

InfiniConnect uses two EFR32FG23 Wireless SoCs: One at the furnace and one at the outdoor compressor.  It snoops the RS485 bus at either end
and packages the Carrier Infinity packets for transmission to the other side using Silicon Labs' Connect wireless stack.  The example provided uses
Connect's 915 MHz 2Mbps/250kbps DSSS-OQPSK PHY, giving it good range/propagation characteristics and making it robust against interference.  Transmit
power is set to 10 dBm.

InfiniConnect uses the Connect stack in direct mode.  The furnace transceiver's node ID is hard-coded to 0x0001.  The compressor node ID is 0x0002.
The PAN ID is also hard coded.  Connect's security is enabled.  The security key is hard coded and can be modified in `app_init.h`.  The hard coding
of the network parameters is intended to ensure simplicity such that the two devices "just work" when connected for the first time.

Since the Infinity packets can be over 255 bytes - larger than the maximum allowed Connect packet - the Infinity packets are broken up into
multiple, smaller chunks for wireless transmission.  The receiver waits until the final packet of the sequence is received over the air
before transmitting the reassembled Infinity packet over RS485.  The entire Infinity packet is transmitted as-is and otherwise not modified.


## Hardware

Insert Pictures here...

## Observations

The Carrier serial protocol is quite chatty; in other words, even when the system is turned off/inactive, there is frequent traffic
on the RS485 bus between the thermostat, furnace and compressor.  To reduce the amount of wireless traffic, the transceiver at the
furnace end is programmed to filter out all traffic from the transmitter unless it's intended for the outdoor unit.  This is accomplished
by looking at the first byte of the packet payload - the destination address of the packet.  If it's 0x52 - the compressor - the packet
is transmitted.  If not, it's ignored.  Packets originating at the compressor are never filtered.  Even with packet filtering, there is
still a significant amount of wireless traffic generated.  Since Connect's security relies on a frame counter which is periodically 
written to NVM3 (once every 1000 packets transmitted), the NVM3 component's instance size was increased to 245760 bytes - nearly the
entire remaining area of flash.  This overprivisoned space means wear leveling of flash writes is spread out across a much wider 
area of flash than necessary.  It should last decades - longer than the HVAC system itself will likely last.


