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

## Technical overview

The controller reads an optical breath sensor, a soft potentiometer, and a five-way joystick. It maps the breath sensor reading to a 7-bit value from 0 to 127 (128 levels), which controls MIDI volume (Control Change 7). A value of 3 or higher triggers a Note On for the selected root note and chord; before that, the controller sends Note Off for the previously tracked root note and chord. When the breath value falls below 1, it resets the breath-detection state so another note can be triggered; this transition does not itself send Note Off. The soft potentiometer controls pitch bend, while the joystick selects the root note and chord and provides access to the MIDI channel, pitch-bend range, and message resolution settings. MIDI data is sent over the serial port at 31,250 baud to an external synthesizer. An 8x2 LCD with a parallel interface displays breath level, pitch bend, and setup information.

### Power-on calibration

At startup, the controller displays `CAL 5s` and takes breath-sensor readings at 10 ms intervals for five seconds, while displaying `NO BLOW`. It averages these readings and stores the result as the zero-pressure offset. Keep the mouthpiece unblown during calibration so the offset represents the sensor's resting level. Subsequent readings are measured relative to this baseline, then mapped and clamped to the 0-127 breath level. This is a one-point zero calibration, not a multi-point calibration. The offset is sampled again when entering the setup menu. The soft potentiometer is not zero-calibrated at startup; its touch position is tracked when it is first touched.

```mermaid
flowchart LR
    subgraph INPUTS["INPUTS"]
        direction TB
        BREATH["Optical breath sensor"]
        PITCH["Soft potentiometer"]
        JOY["Five-way joystick"]
    end

    subgraph MCU["ARDUINO MINI - ATmega168"]
        direction TB
        ZERO["Power-on zero calibration<br/>Store resting breath reading"]
        PROCESS["Read and process inputs<br/>Breath level: 0-127"]
    end

    MIDI["MIDI serial<br/>31,250 baud"]
    SYNTH["External synthesizer"]
    LCD["8x2 character LCD"]

    BREATH -->|5 s sample average| ZERO
    ZERO -->|baseline offset| PROCESS
    BREATH -->|live analog readings| PROCESS
    PITCH -->|analog| PROCESS
    JOY -->|digital and analog| PROCESS
    PROCESS -->|CC 7, Note On/Off, Pitch Bend| MIDI
    MIDI --> SYNTH
    PROCESS -->|status and levels| LCD

    classDef input fill:#eaf4ff,stroke:#3b82b7,stroke-width:2px,color:#17324d
    classDef controller fill:#eef7ed,stroke:#5b8f52,stroke-width:2px,color:#203b1d
    classDef output fill:#fff3df,stroke:#c58a2b,stroke-width:2px,color:#4a3513
    class BREATH,PITCH,JOY input
    class ZERO,PROCESS controller
    class MIDI,SYNTH,LCD output
```

*Schematics*

<p align="center">
<img width="800" src="doc/assets/images/schematics.jpg">
</p>