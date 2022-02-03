# LED matrix driver using RP2040 for HUB75 panels up to 128x128 pixels

<p align="center"><img src="https://pitschu.de/ps_media/pcb RP2040 devboard.jpg" alt="" width="60%"/></p>

<p align="center"><i>Devel board  with 2 HUB75 connectors, level shifters and a Pi Pico</i></p>

## Hardware of an LED matrix with HUB75 connector

LED matrix panels with HUB75 connector are technically very simple. 3 shift registers, each with 64 bits, control 64 RGB LEDs. In an LED matrix with 64x32 LEDs, a 1 out of 32 multiplexer determines which of the 32 LED rows is connected to the shift register and its output drivers. All other 31 LED rows are then not activated and are therefore black. A 64 x 64 LED matrix typically consists of 2 halves, each with 64x62 LEDs. The upper and lower half of the panel are thus independent, they only share the 1 out of 32 multiplexer. However, each half has its own set of 3 shift registers. The following signals are required to control such a panel:

- 3 signal inputs for the 3 upper shift registers (RGB channel 1)
- 3 signal inputs for the 3 lower shift registers (RGB channel 2)
- up to 5 ADR signals inputs for the 1 out of 32 multiplexer driving the LED rows
- 1 CLK input as clock signal for all shift registers
- 1 LATCH input for all shift registers. If the signal goes to HIGH, the inserted data is accepted in the driver outputs of the shift register. There are different types of shift registers: some take over the data immediately when the signal goes HIGH, others need e.g. 3 cycles on the clock line to take over the data.
- 1 OE (output enable) allows switching off the driver outputs of the shift registers in order to control the LEDs dark. This is used to control the overall brightness and to reduce so-called ghosting.

The main challenge now is to control the signals at the correct time. In order to display an image in 24-bit RGB, each of the 3 RGB shift registers must display an 8-bit color channel. However, since the driver outputs can only be switched ON or OFF and therefore cannot process any brightness information, each shift register must be written with different bit patterns a total of 255 times in order to reproduce the 8-bit brightness information. Furthermore, only one line of 32 is displayed at a time. This means that all 32 lines have to be displayed (multiplexed) one after the other in order to show the whole picture. So in order to display the complete image in full color depth, you need 32 lines by 64 pixels per line by 255 passes for the color, so a total of 32 x 64 x 255 = 522420 clocks on the CLK line. In order to get a smooth, flicker-free display, the image should be repeated at least 50 times per second. Overall, we get about 26 million clocks per second, or at least 26MHz shift clock.

## Control of the LED matrix with the RP2040

In order to be able to meet the requirements derived above (delivery of the 2 x 3-bit RGB data with the control signals in a rhythm of 26MHz), suitable hardware is required. This is usually special hardware based on FPGAs, or suitably equipped, very fast processors. The Raspberry Pi Pico RP2040 is one such processor, which is very attractive for DIY due to its low price of less than 1 euro for the chip and less than 4 EUR for the Raspberry Pi Pico development board.

With its programmable IOs (PIO), the RP2040 offers exactly the hardware required for the control. Only one of the 2 existing PIOs is used in this project. 2 of the 4 state machines included in each PIO device generate the required control and data signals for the HUB75 connector.

## Overview of how it works using a 64 x 64 panel as an example

The 14 signal lines of the HUB75 connector are controlled by 2 state machines of a PIO block. The first state machine controls the 6 RGB lines, as well as CLK, LATCH and OE. The 6-bit RGB data for 4 consecutive pixels is supplied with each FIFO word. Information for controlling the OE line is supplied in 2 further bits in order to control the brightness of the panel. A DMA channel supplies the state machine with data, whereby a DMA pass always supplies the complete data for a complete frame (64 pixels x 2 RGB channels x 8 bit planes x 32 lines).
A second state machine controls the 5 address lines of the row multiplexer as well as the LATCH and the OE line. This state machine is also supplied with data via a DMA channel. Both state machines are synchronized with each other via IRQs, so that the first state machine starts when the second state machine drives to the next row address. Conversely, the first state machine informs the second when a pixel line has been completely output.

In order for the two DMA channels mentioned to be able to supply the correct data to the state machines, the corresponding memory areas must be filled with the data of the image to be displayed.
This is done using the `hub75_update()` function in the source file `hub75_BCM.c`. Based on the image to be output, this function calculates the correct bit sequences for controlling the shift register.

## Anti-flicker by BCM modulation
As already described above, several display runs are required to display 8-bit color values. An 8-bit color value can represent 256 different brightnesses and therefore requires 256 runs with HUB75 LED matrices. The normal process for LEDs is PWM modulation. For example, the color value 134 (= 0x86 = 0b10000110) is output as a PWM signal with 134 HIGH units and 122 (= 256 - 134) LOW units. The LED therefore lights up at about 52% of its maximum brightness. This method is very easy and effective to implement, but has one major disadvantage. With a refresh rate of 50Hz, the image will flicker quite a lot, since all the LEDs are on for a period of time during the output of an image and then turn off afterwards. This then repeats itself with the frame rate and leads to flickering.
Here, instead of PWM modulation, so-called BCM modulation can be used. Here, too, it must of course be ensured that for a color value of 134 the LED is off for 122 of 256 time units and on for 134 of 256 time units. However, it is not necessary for the 122 or 134 time units to be in one piece. The BCM process ensures exactly this. The 122 OFF times and the 134 ON times are simply distributed so that they appear alternately as far as possible. The flickering is massively reduced because the LED is not only switched OFF and ON once during the 256 time segments, but much more frequently.

## Using the driver
The actual driver software consists of a handful of functions. These are:
* `void hub75_config(int bpp)` Configure and start the HUB75 driver hardware.\
    The driver supports from 4 up to 8 pixel planes. 
    This function first stops all driver operation currently running and then
    reconfigures all required PIO and DMA devices.

* `int hub75_update(rgb_t* image, uint8_t* overlay)` Update the LED matrix screen buffer.
    This function transfers the given image and overlay into the framebuffer used by the driver to control the PIO and DMA devices. `image`points to the source image consisting of WIDTH x HEIGHT pixels. Each pixel RGB data is stored in an `uint32_t` alias `rgb_t` value. `overlay` points to an overlay image which is displayed 'in front of' the main image. Overlay uses a 16 entry color lookup table.

* `void hub75_set_masterbrightness(int brt)` sets the global brightness of the screen. It ranges between 0 (= darkest) to WIDTH-4 (= bright). 

* `void hub75_set_overlaycolor(int index, rgb_t color)` Sets one color in the overlay color lookup table. Index can range from 1 to 15. Index 0 is used internally for 'do not show an overlay pixel'.

## Build variables
The driver currently supports LED panels of sizes 64x64 and 128x128. 128x128 panels are driven via 2 HUB75 ports, so each port drives a 128x64 sub-panel. Furthermore, the driver supports 2 different hardware versions, which only differ in the GPIO usage. PCB version 1 only supports a 64x64 panel, version 2 also supports the 128x128 panel.

| Build variable      | Setting | Description  |
| :---        |    :----:   |          :--- |
| PCB_LAYOUT_V1      | 1       | Build  for a PCB v1 board   |
| PCB_LAYOUT_V2      | 1       | Build  for a PCB v2 board   |
| HUB75_SIZE   | 4040        | Build for 64 x 64 panel      |
| HUB75_SIZE   | 8080        | Build for 128 x 128 panel      |
| HUB75_BCM | <undef>  | Build a PWM version of driver |
| HUB75_BCM | 1  | Build a BCM version of driver (recommended) |

#
## Driver in action
See the [driver in action](https://youtu.be/A8yXWeLI5ng) in this video showing several display tasks working on one common image buffer controlled by FreeRTOS. Notice that this video shows a B-grade panel with some damaged pixels.




