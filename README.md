# usb_powermeter_firmware
# Note: This is a work in progress, so not entirely easy to get to work, but if you know your way around STM processors and a bit of Arduino you should be able to get it to work (I did afterall). Cool head recommended.

An attempt to make a better firmware for a chinese usb powermeter.

The device can measure from 100kHz to 10GHz, -60dBm to 0dBm. It comes in a neat form-factor with USB-C and a single SMA connector.

The performance of this thing was never gonna be anything to write home about, but its a cool little software project I made for fun that i'd like to share.

The factory supplied firmware streams data over regular USB UART and streams the converted power in a pretty straight-forward manner, but the supplied PC-side software leaves alot to be desired in terms of features, and the protocol is not that easy to understand.

## Hardware
The SMA connector goes into an 8-pin detector device with a few surrounding components. I believe this is an Analog Devices AD8317, datasheet here: https://www.analog.com/media/en/technical-documentation/data-sheets/ad8317.pdf

The designers seem to have followed the example schematic in datasheet figure 36. Resistor R5 (R1 in datasheet schematic) is 52.3 Ohm (E96 series).

The output from the detector goes through a series resistor and straight into an STM32F401CC MCU. The MCU has direct USB support so that goes directly out through the wall. A single LDO for the MCU and detector in the corner and a few breakouts pin headers which are not populated.

## Bootloader
The STM32F401CC has a hard bootloader in the processor, which can be triggered at powerup by pulling BOOT0 pin high. That pin is connected with pulldown-resistor R2 on the board. By pulling it up with 5V from the USB connector(the pin is 5V tolerant, I checked) the device can be put into bootloader mode (I used a piece of wire between the lower left pin header pin and the MCU side of R2).

I used the ST supplied firmware loading tool STM32CubeProgrammer to work on the flash when the MCU is in bootloading mode.

I have patched the excellent STM32 HID bootloader from Serasidis to work with the STM32F401CC, check the Github repo here: https://github.com/iamaiy/STM32_HID_Bootloader Beware that the makefiles down in bootloader F4 are sketchy so far.

The bootloader works by sending a soft reboot command, followed by some usb commands to keep the MCU from booting into userland.

When you install the bootloader, remember to upload something which has support for the HID bootloader for the bootloader to boot at 0x8004000. I have uploaded a "Hello World" example for that purpose here: https://drive.google.com/file/d/19KSJot1h6KMrcqswOiS8wQ07hAWFRFDQ/view?usp=sharing

## Arduino settings for STM32F401CC with HID bootloader
* Board: "Generic STM32F4 series"
* Board part number: "BlackPill F401CC"
* U(S)ART support: "Enabled (generic 'Serial')"
* USB support (if available): "CDC (generic 'Serial' supersede U(S)ART)"
* USB speed (if available): "Low/Full Speed"
* Upload method: "HID Bootloader 2.2"
#### Note: be careful with the board settings in the Arduino GUI. The USB USART must be enabled, or you will get locked out and have to upload hello world again with STM32CubeProgrammer again

## Design ideas
What I'd like is to have some good control over the thing. The ADC sampling for instance, add some timestamping and pre-accumulation of upstream data for a start. And of course, SCPI command interface!

I chose to go with Arduino as IDE for this project. With the STM32-series being to exactly well supported compared to other boards in that realm, it still presented good enough of a challenge to be fun.
To get the device to work in the Arduino environment, the STM32duino project provides everything we need, as support for STM32F401CC already exists since someone worked up the support for it on a Blackpill variant with that MCU. See https://github.com/stm32duino/Arduino_Core_STM32/wiki/Getting-Started on how to get STM32duino installed.

There are at least two Arduino SCPI interpreters available, I went for the Vrekrer SCPI Parser for this project. You can either fetch it from github or add it from the Arduino library manager.

One of the features I wanted to implement and experiment with is pre-accumulation. By this I mean to repeatedly sample the ADC and accumulate its value in software to get a wider numerical resolution of the output data value.

With the MCU ADC we can get a 12-bit ADC values (0-4095), so the straight-forward way to do pre-accumulation of ADC data would be to accumulate 16 values to get something with theoretical range of 0 to 65535. 

However, the analog signal out from the detector chip does not go all the way rail-to-rail (for example see AD8317 figure 3-8). Since this is the case, we can accumulate more samples and stay in the uint16_t data range without overflow. After a bit of fiddling, I found that we can accumulate 30 samples and still have some margin left.

The ADC sampling is triggered/handled by MCU Timer 3, which is set to 30000 Hz default, giving us a default output sample rate of 1kHz/kSample. The solution is not as neat as I had hoped, but it will do for a first version.

Post-accumulation of the acquired uint16_t samples to uint32_t to improve sample fidelity or reduce datarate.

The output data format of converted samples is:
"P: xxxxxx T: yyyyyy\n"

Where the data after "P:" is the power, given in the output units set. The data after "T:" is the delta time in microseconds since the previous sample (for time-stamping without rollover of Arduino micros() function).

## Firmware Features:
* Pre-accumulation of samples, N=30
* Post-accumulation of samples, default 0
* Timer-driven ADC trigger, 30kHz default
* Continuous and single trigger, Continuous default
* Serial output data push - This is almost certainly not SCPI compliant, but hey, its fun when you watch the numbers flowing on a terminal or in a real-time graph
* SI unit and RAW data value output format


## SCPI command tree:
* *IDN? - Query instrument identifications string
* SYStem:TEMPerature? - Query the MCU on-board temperature sensor
* SYStem:VOLTage? - Query the MCU on-board voltage reference (buggy)
* ACQuire:INTerval - Set ADC acquisition interval (units: microseconds)
* ACQuire:INTerval? - Query ADC acquisition interval (units: microseconds)
* ACQuire:FREQuency - Set ADC acquisition frequency (units: Hertz)
* ACQuire:Frequency? - Query ADC acquisition frequency (units: Hertz)
* ACQuire:NAVG - Set number of post-accumulation samples (units: samples)
* ACQuire:NAVG? - Query number of post-accumulation samples (units: samples)
* CALibration:FREQuency - Placeholder for frequency/power calibration
* CALibration:FREQuency? - Placeholder for frequency/power calibration
* TRIGger:CONTinuous - Set continuous triggering (0=Off, 1=On)
* TRIGger:CONTinuous? - Query continuous triggering
* TRIGger:SINGLE - Set single trigger (Continuous off)
* TRIGger:SINGLE? - Query single trigger
* TRIGger:IMMediate - Send a software trigger event immediately
* TRIGger:STOP - Stop on-going acquisition
* DATA:UNIT - Set output units, supported: dBm, mW, Volt, RAW
* DATA:UNIT? - Query output units (String response, dBm, mW, Volt, RAW)
* DATA:FETCH? - Fetch current sample
* DATA:PUSH - Set data upstream push, data is pushed out when acquired (0=Off, 1=On)

## To-do:
* Frequency calibration table support
* Analog level trigger for measurement
* N-length acquisition support
