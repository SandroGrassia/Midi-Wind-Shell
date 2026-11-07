# Midi-Wind-Shell

<p align="center">
<img width="800" src="doc/assets/images/shell.jpg">
</p>

This wind MIDI controller is housed inside a shell. It was designed and built in 2011 using an Arduino Mini 04. Other microcontrollers may also work, provided they have at least two analog inputs and twelve digital GPIO pins.
It can send clusters of up to four notes to an external synthesizer.

Breath pressure controls the volume (MIDI CC 7), and the mouthpiece is mounted on top of the shell.
<p align="center">
<img width="400" src="doc/assets/images/mouth.jpg">
</p>

The breath sensor is an optical design made from an LED, a photoresistor, and a dark barrier. Breath moves the barrier, changing how much light from the LED reaches the photoresistor.

<p align="center">
<img width="400" src="doc/assets/images/wind_1.jpg">
</p>

<p align="center">
<img width="400" src="doc/assets/images/wind_2.jpg">
</p>

Inside the shell, a soft potentiometer provides pitch modulation.
<p align="center">
<img width="400" src="doc/assets/images/pitch.jpg">
</p>

A simple 8×2 character LCD with a parallel interface displays the breath level and pitch-bend position.

<p align="center">
<img width="400" src="doc/assets/images/display.jpg">
</p>

A five-way joystick selects the root note and note cluster. It also lets you configure the MIDI channel, pitch-bend range, and pitch-bend message format (MSB and LSB, or MSB only).

<p align="center">
<img width="400" src="doc/assets/images/joystick.jpg">
</p>

*Schematics*

<p align="center">
<img width="800" src="doc/assets/images/schematics.jpg">
</p>