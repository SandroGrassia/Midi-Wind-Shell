# Midi-Wind-Shell

<p align="center">
<img width="400" src="/doc/assets/images/shell_small.jpg")
</p>

It's a wind midi controller installed in a shell. This project was designed and realized in 2010. Probably any microcontroller with, at least 1 ADC and 9 digital GPIO can be used.

Basically, the wind shell uses breath for volume control (CC7) and a soft potentiometer for pitch-bend change.
Up to 4 notes (cluster) can be sent simultanously. A simple 8x2 LCD character display with parallel bus is used to show breath level and pitch bend position.

<p align="center">
<img width="400" src="/doc/assets/images/mouth.jpg")
</p>

<p align="center">
<img width="400" src="/doc/assets/images/pitch.jpg")
</p>

A 5-degrees switch joystick is used for selecting the foundamental note and the cluster; it also allows the configuration of the midi channel, the pitch bend depth, the pitch bend message (MSB-LSB or MSB only).


*Schematics*

<p align="center">
<img width="800" src="/doc/assets/images/schematics.jpg")
</p>