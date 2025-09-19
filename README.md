# Midi-Wind-Shell

<p align="center">
<img width="500" src="/doc/assets/images/shell_small.jpg")
</p>

It's a breath midi controller installed in a shell. This project was designed and realized in 2010.




LILLA is an audio sampler based on Teensy 4.1, designed and assembled in Italy.


*Foundamentals*

LILLA is polyphonic (16 voices), multitimbral, and multi-midi audio sampler; LILLA allows to play either imported or self-recorded audio files (16bit/44.1KHz), or live audio stream (when used as Live Sampler) applying various playing mode, adding ADSR envelope, changing length and pitch, and using digital effects.
Four different modes:
- **Performance mode**: play using "patches" formed by 1 to 8 sounds (layered or indipendent);
- **Sampler mode**: record, save and export to micro-SD the incoming audio;
- **Live Sampler**: play the incoming audio stream, using a temporary/volatile memory as a virtual tape loop;
- **Midi Loop**: a 6 tracks midi-looper.


*Connections*
- USB-B for power supply, firmware update
- 3.5mm jack midi in
- 3.5mm jack midi thru
- 3.5mm jack stereo line in: dynamic microphones/line 2 channels input with analog gain (level) control
- 3.5mm jack stereo line out: 3.3Vpp 2 channels audio output
- 3.5mm jack lo-fi stereo monitor: 3.3Vpp 2 channels audio output
- micro-SD socket