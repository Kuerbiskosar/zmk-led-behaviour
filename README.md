# tripple led widget for hugin & munin
This repository contains a zmk behaviour made for my hugin & munin keyboard.
the hugin & munin keyboard has two halves (hugin and munin), with 3 led's each.
The hugin & munin keyboards are pro micro compatible (I use a nice!nano v2 for each half). The pins of the led's are expected to be 5,6 and 7 (on both halves, referring to the Arduino labels given [here](https://nicekeyboards.com/docs/nice-nano/pinout-schematic)).
>Note: I guess this is more of a vfx than a behaviour (as the repository name suggests). May I be forgiven.

# How it behaves
I am aiming for the following features:
- [ ] Startup "animation"
  - give a little wave fade animation over the tree led's, when the keyboard is turned on.
- [ ] Power indication
  - show how much power the device has (on startup and after waking up from idle), à-la full power: 3 led's, 60%: 2 led's, 50%: 1 led full, 1 50%, 15% one led fading in and out
  >Note I'll have to test if I like a fading led, or a blinking led better, for the low power state.
- [ ] caps lock, scroll lock, num lock indication
  - when on a specified layer, the led's indicate whether caps lock etc. are on or off.
  >Note Because I always have num lock on, i want the led to glow when num lock is OFF.
  >More relevant note: this behaviour will only work on the central. There is an [issue](https://github.com/zmkfirmware/zmk/issues/3307), open to adress this.
- [ ] bluetooth profile indication
  - when on a specified layer, show to which bluetooth profile the keyboard is connected
  > Note: this behaviour will (likely) only work on the central.
- [ ] disconnected indication
  - if the peripheral is not connected to the central / if the central is not connected to bluetooth, it will show a led animation. (like switch controller led bounce!)
- [ ] Make timeouts / battery percentage before blinking / animation speed configurable


## Usage
See https://zmk.dev/docs/features/modules for general info on how to use modules.

## More Info

For more info on modules, you can read through  through the [Zephyr modules page](https://docs.zephyrproject.org/3.5.0/develop/modules.html) and [ZMK's page on using modules](https://zmk.dev/docs/features/modules). [Zephyr's west manifest page](https://docs.zephyrproject.org/3.5.0/develop/west/manifest.html#west-manifests) may also be of use.
