# Midi-Wind-Shell

<p align="center">
<img width="800" src="/doc/assets/images/shell.jpg")
</p>

It's a wind midi controller installed in a shell. This project was designed and realized in 2011 using an Arduino Mini 04; probably any microcontroller with at least 2 analog and 12 digital GPIOs can be used.
Up to 4 notes (cluster) can be sent simultanously to an external synth.

The wind shell uses breath for volume control (CC7); the mouthpiece is attached on top of the shell.
<p align="center">
<img width="400" src="/doc/assets/images/mouth.jpg")
</p>

The open wind sensor is made with an LED, a photoresistor, and a dark barrier moved by breath, which modulates the amount of light from the LED that reaches the photoresistor.

<p align="center">
<img width="400" src="/doc/assets/images/wind_1.jpg")
</p>

<p align="center">
<img width="400" src="/doc/assets/images/wind_2.jpg")
</p>

Inside the shell, a soft potentiometer is used for pitch modulation.
<p align="center">
<img width="400" src="/doc/assets/images/pitch.jpg")
</p>

A simple 8x2 LCD character display with parallel bus is used to show breath level and pitch bend position.

<p align="center">
<img width="400" src="/doc/assets/images/display.jpg")
</p>

A 5-degrees switch joystick is used for selecting the foundamental note and the cluster; it also allows the configuration of the midi channel, the pitch bend depth, the pitch bend message (MSB-LSB or MSB only).

<p align="center">
<img width="400" src="/doc/assets/images/joystick.jpg")
</p>

*Schematics*

<p align="center">
<img width="800" src="/doc/assets/images/schematics.jpg")
</p>