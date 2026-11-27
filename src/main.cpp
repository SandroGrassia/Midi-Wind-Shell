/*
   LILLA Audio Sampler
   Author: Sandro Grassia (info@lillasampler.it)
   www.lillasampler.com
*/
/*

E-SHELL DUE
Data: 27/11/2026

Hardware: ARDUINO MINI
Processor: ATmega168
Display: HITACHI HD44780 8x2
Aspetto:
        no wind
        |||
Comandi: Sensore soffio (WIND), sensore Soft Pot (per Pitch Bend), Joystick 5 vie

*/

#include "Constants.h"
#include <Arduino.h>
#include <LiquidCrystal.h>

// Release
#include "config.h"

// State
enum state_names
{
    RUN,
    UP,
    DOWN,
    RIGHT,
    LEFT
};
state_names shell_state = RUN;

// Soft potentiometer (Pitch bend)
constexpr int pitch_pin = 5;      // pitch (analog)
int pitch_value_10bit = 512;      // pitch Bend
int pitch_value_10bit_base = 512; // pitch Bend precedente
uint16_t pitch_range = 48;        // estensione totale in semitoni, deve essere pari
uint16_t pitch_bits = 14;         // bit del messaggio Pitch Bend
int pitch_sensor_value = 0;       // 0 <= pitch_sensor_value <= 1023 - valore di tensione rilevato sul sensore soft-touch
int pitch_sensor_value_base = 0;
int pitch_sensor_value_0 = 0; // valore di tensione iniziale rilevato sul sensore soft-touch
int pitch_sensor_status = 0;  // stato del sensore soft-touch: premuto=1, non premuto=0
uint8_t msb = 0;              // byte MSB del Pitch_bend
uint8_t lsb = 0;              // byte LSB del Pitch_bend

// Wind sensor (volume)
constexpr int vol_pin = 4;     // volume (analog)
constexpr int vol_upthr = 800; // soglia superiore del valore in ingresso del volume che da vol=127
int wind_state = 0;
int vol_value = 0;
int vol_value_0 = 0;
int vol_value_offset = 0; // offset del sensore di pressione
// Only Note_on/Note_off own this snapshot; joystick selection is independent.
struct ActiveNotes
{
    uint8_t notes[5];
    uint8_t count;
    uint8_t channel;
} active_notes = {};
bool pitch_reset_pending = false;
int displayed_volume = -1;

// Joystick (setup)
constexpr int soglia = 200;    // soglia analogica per valore HIGH
constexpr int up_pin = 9;      // pulsante Up (digital)
constexpr int down_pin = 10;   // pulsante Down (digital)
constexpr int right_pin = 11;  // pulsante Left (digital)
constexpr int left_pin = 12;   // pulsante Right (digital)
constexpr int central_pin = 0; // pulsante Center (analog)
uint8_t midi = 0;              // canale midi iniziale (0 == canale 1)
uint8_t menu = 0;              // menu' attivo (0 == nessun menu' attivo)

// Note-Cluster
uint8_t nota = 60;                 // nota base (C3 == 60)
char note_nomi[] = "CcDdEFfGgAaB"; // nome delle Note
int cluster = 0;                   // cluster id

// Display
// RS=8, R/W=7, E=6, DB-4=5, DB-5=4, DB-6=3, DB-7=2
LiquidCrystal lcd(8, 7, 6, 5, 4, 3, 2);

unsigned long repeat_started = 0;
bool center_raw = false;
bool center_stable = false;
unsigned long center_changed = 0;
unsigned long menu_repeat = 0;

// Functions
void Midi(uint8_t cmd, uint8_t data1, uint8_t data2);
void Note_on(void);
void Note_off(void);
void Reset_pitch(void);
bool Process_menu(void);
void All_notes_off(void);
void Pitch_bend(uint16_t value_10bit); // comando pitch bend
void Show_page(int page);              // visualizzazioni
void Midi_pitch_range(int semitones);  // comando pitch bend range
void Volume_bar(void);
void Pitch_bar(void);
void Bar(int value, int row);

void setup()
{
    // Setup display
    lcd.begin(8, 2);

    // Show version
    Show_page(1); // E-Shell versione

    lcd.createChar(1, char_10000);
    lcd.createChar(2, char_11000);
    lcd.createChar(3, char_11100);
    lcd.createChar(4, char_11110);
    lcd.createChar(5, char_11111);

    // Setup MIDI baud rate
    Serial.begin(31250);

    // Monitor/test
    // Serial.begin(9600);

    // Digital inputs
    pinMode(9, INPUT);
    digitalWrite(9, HIGH);
    pinMode(10, INPUT);
    digitalWrite(10, HIGH);
    pinMode(11, INPUT);
    digitalWrite(11, HIGH);
    pinMode(12, INPUT);
    digitalWrite(12, HIGH);
    pinMode(13, OUTPUT);

    // Calibrate wind sensor offset from readings taken over five seconds.
    constexpr unsigned long wind_calibration_duration_ms = 5000;
    uint32_t wind_calibration_sum = 0;
    uint16_t wind_calibration_samples = 0;
    const unsigned long wind_calibration_start = millis();

    lcd.clear();
    lcd.print("CAL 5s");
    lcd.setCursor(0, 1);
    lcd.print("NO BLOW");

    while (millis() - wind_calibration_start < wind_calibration_duration_ms)
    {
        wind_calibration_sum += analogRead(vol_pin);
        ++wind_calibration_samples;
        delay(10);
    }

    vol_value_offset = wind_calibration_sum / wind_calibration_samples;

    // Setup pitch bend range
    Midi_pitch_range(pitch_range / 2); // pitch_range/2 indica il numero di semitoni +/- per un'estensione totale pari a pitch_range

    // Set volume = 0
    Midi(176 + midi, 7, 0);

    Reset_pitch();

    // Stop synth
    All_notes_off();

    Show_page(6); // visualizza note off, volume e pitch bend
}

// *************************************************************** LOOP ****************************************************************

void loop()
{
    if (Process_menu())
    {
        return;
    }

    // Soft potentiometer (Pitch)
    pitch_sensor_value = analogRead(pitch_pin);

    switch (pitch_sensor_status)
    {
    case 0:                         // no touch
        if (pitch_sensor_value > 2) // sensore impegnato
        {
            pitch_sensor_status = 1;
            pitch_sensor_value_base = pitch_sensor_value;
            pitch_sensor_value_0 = pitch_sensor_value;
            pitch_value_10bit_base = pitch_value_10bit;
        }
        break;

    case 1: // touched
        if (pitch_sensor_value <= 2 || abs(pitch_sensor_value - pitch_sensor_value_0) > 20)
        {
            pitch_sensor_status = 0;
        }
        else if (abs(pitch_sensor_value - pitch_sensor_value_0) >= 2 && abs(pitch_sensor_value - pitch_sensor_value_0) <= 20) // verifica se la variazione di pitch_sensor_value e' valida
        {
            pitch_value_10bit = constrain(pitch_value_10bit_base + (pitch_sensor_value - pitch_sensor_value_base), 0, 1023);
            pitch_sensor_value_0 = pitch_sensor_value;

            Pitch_bend(pitch_value_10bit);
            if (shell_state == 0)
            {
                Show_page(8); // visualizza pitch bend in semitoni
            }
        }
        break;
    }

    // Wind sensor (volume)
    vol_value = constrain(map(analogRead(vol_pin) - vol_value_offset, 0, vol_upthr, 0, 127), 0, 127);

    // update display
    if (vol_value != displayed_volume && shell_state == RUN) // visualizza il valore del volume
    {
        Show_page(7); // visualizza volume
    }

    // Keep the documented legato behaviour: release old notes at the next attack.
    if (wind_state == 0 && vol_value >= 3)
    {
        Note_off();
        if (pitch_reset_pending)
        {
            Reset_pitch();
            if (shell_state == RUN)
            {
                Show_page(8);
            }
        }
        Midi(176 + midi, 7, vol_value);
        vol_value_0 = vol_value;
        Note_on();
        wind_state = 1;
    }
    else if (wind_state == 1)
    {
        // Always transmit silence, even inside the controller deadband.
        if (vol_value == 0 || abs(vol_value - vol_value_0) > 5)
        {
            Midi(176 + midi, 7, vol_value);
            vol_value_0 = vol_value;
        }
        if (vol_value == 0)
        {
            wind_state = 0;
        }
    }

    // Joystick
    // Note

    switch (shell_state)
    {
    case RUN:
        if (digitalRead(up_pin) == LOW)
        {
            shell_state = UP;
            repeat_started = millis();
            pitch_reset_pending = true;

            if (nota < 127)
            {
                ++nota;
            }
            Show_page(4); // visualizza intonazione
        }
        else if (digitalRead(down_pin) == LOW)
        {
            shell_state = DOWN;
            repeat_started = millis();
            pitch_reset_pending = true;

            if (nota > 0)
            {
                --nota;
            }
            Show_page(4); // visualizza intonazione
        }
        else if (digitalRead(right_pin) == LOW)
        {
            shell_state = RIGHT;
            repeat_started = millis();

            if (cluster < (int)(sizeof(chord) / 4 - 1))
            {
                ++cluster;
            }
            Show_page(5); // visualizza schema cluster
        }
        else if (digitalRead(left_pin) == LOW)
        {
            shell_state = LEFT;
            repeat_started = millis();

            if (cluster > 0)
            {
                --cluster;
            }
            Show_page(5); // visualizza schema cluster
        }
        break;

    case UP:
        if (digitalRead(up_pin) == LOW)
        {
            if (millis() - repeat_started >= 300)
            {
                repeat_started = millis();
                if (nota < 127)
                {
                    ++nota;
                    pitch_reset_pending = true;
                }
                Show_page(4); // visualizza intonazione
            }
        }
        else
        {
            shell_state = RUN;
            Show_page(6);
        }
        break;

    case DOWN:
        if (digitalRead(down_pin) == LOW)
        {
            if (millis() - repeat_started >= 300)
            {
                repeat_started = millis();
                if (nota > 0)
                {
                    --nota;
                    pitch_reset_pending = true;
                }
                Show_page(4); // visualizza intonazione
            }
        }
        else
        {
            shell_state = RUN;
            Show_page(6);
        }
        break;

    case RIGHT:
        if (digitalRead(right_pin) == LOW)
        {
            if (millis() - repeat_started >= 300)
            {
                repeat_started = millis();
                if (cluster < (int)(sizeof(chord) / 4 - 1))
                {
                    ++cluster;
                }
                Show_page(5); // visualizza schema cluster
            }
        }
        else
        {
            shell_state = RUN;
            Show_page(6);
        }
        break;

    case LEFT:
        if (digitalRead(left_pin) == LOW)
        {
            if (millis() - repeat_started >= 300)
            {
                repeat_started = millis();
                if (cluster > 0)
                {
                    --cluster;
                }
                Show_page(5); // visualizza schema cluster
            }
        }
        else
        {
            shell_state = RUN;
            Show_page(6);
        }
        break;
    }
}

void Note_on(void)
{
    Note_off();
    active_notes.channel = midi;
    active_notes.notes[active_notes.count++] = nota;
    Midi(144 + midi, nota, 127);
    for (uint8_t i = 0; i < 4; ++i)
    {
        const uint8_t interval = chord[4 * cluster + i];
        const int note = nota + interval;
        if (interval > 0 && note <= 127)
        {
            active_notes.notes[active_notes.count++] = note;
            Midi(144 + midi, note, 127);
        }
    }
}

void Note_off(void)
{
    for (uint8_t i = 0; i < active_notes.count; ++i)
        Midi(128 + active_notes.channel, active_notes.notes[i], 0);
    active_notes.count = 0;
}

void All_notes_off(void)
{
    Note_off();
    for (int i = 0; i < 128; ++i)
        Midi(128 + midi, i, 0);
    Midi(176 + midi, 7, 0);
    wind_state = 0;
    vol_value = 0;
    vol_value_0 = 0;
}

void Reset_pitch(void)
{
    pitch_value_10bit = 512;
    pitch_value_10bit_base = 512;
    pitch_sensor_status = 0;
    pitch_reset_pending = false;
    Pitch_bend(512);
}

bool Process_menu(void)
{
    const unsigned long now = millis();
    const bool pressed = analogRead(central_pin) <= soglia;
    if (pressed != center_raw)
    {
        center_raw = pressed;
        center_changed = now;
    }
    bool center_click = false;
    if (center_stable != center_raw && now - center_changed >= 30)
    {
        center_stable = center_raw;
        center_click = center_stable;
    }
    if (center_click)
    {
        if (menu == 0)
        {
            All_notes_off();
            Reset_pitch();
            shell_state = RUN;
            menu = 1;
            menu_repeat = now;
            Show_page(11);
        }
        else
        {
            menu = 0;
            Midi_pitch_range(pitch_range / 2);
            Reset_pitch();
            Midi(176 + midi, 7, 0);
            vol_value_0 = 0;
            wind_state = 0;
            shell_state = RUN;
            Show_page(6);
        }
        return true;
    }
    if (menu == 0)
    {
        return false;
    }
    if (now - menu_repeat < 200)
    {
        return true;
    }

    const uint8_t previous_menu = menu;
    bool changed = false;
    if (digitalRead(up_pin) == LOW)
    {
        if (menu > 1)
        {
            --menu;
        }
        changed = true;
    }
    else if (digitalRead(down_pin) == LOW)
    {
        if (menu < 3)
        {
            ++menu;
        }
        changed = true;
    }
    else if (digitalRead(left_pin) == LOW)
    {
        if (menu == 1 && pitch_range >= 24)
        {
            pitch_range -= 12;
        }
        if (menu == 2 && midi > 0)
        {
            --midi;
        }
        if (menu == 3)
        {
            pitch_bits = 7;
        }
        changed = true;
    }
    else if (digitalRead(right_pin) == LOW)
    {
        if (menu == 1 && pitch_range <= 108)
        {
            pitch_range += 12;
        }
        if (menu == 2 && midi < 15)
        {
            ++midi;
        }
        if (menu == 3)
        {
            pitch_bits = 14;
        }
        changed = true;
    }
    if (changed)
    {
        menu_repeat = now;
        if (menu == previous_menu)
        {
            Midi_pitch_range(pitch_range / 2);
            Reset_pitch();
            Midi(176 + midi, 7, 0);
        }
        Show_page(menu == 1 ? 11 : menu == 2 ? 2
                                             : 3);
    }
    return true;
}

void Pitch_bend(uint16_t value_10bit)
{
    msb = value_10bit >> 3;

    if (pitch_bits == 14)
    {
        lsb = value_10bit & 7;
        lsb = lsb << 4;
        Midi(224 + midi, lsb, msb); // comando Pitch bend 14 bit
    }
    else
    {
        Midi(224 + midi, 0, msb); // comando Pitch bend 7 bit
    }
}

// visualizzazioni
void Show_page(int page)
{
    int u;
    int v;
    char N;
    int Ottava;
    switch (page)
    {
    case 1:
        lcd.clear();
        lcd.setCursor(0, 0); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        lcd.print("E-SHELL");
        lcd.setCursor(0, 1);
        lcd.print(release);
        delay(1000);
        break;

    case 2: // canale MIDI
        lcd.clear();
        lcd.setCursor(0, 0); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        lcd.print("midi ch.");
        lcd.setCursor(0, 1); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        lcd.print(midi + 1);
        break;

    case 3: // numero di bit del messaggio pitch bend
        lcd.clear();
        lcd.setCursor(0, 0); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        lcd.print("pitch b.");
        lcd.setCursor(0, 1); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        if (pitch_bits == 7)
        {
            lcd.print("000 MSB");
        }
        else
        {
            lcd.print("LSB MSB");
        }
        break;

    case 4: // tuning
        lcd.clear();
        lcd.print("tune:");
        lcd.setCursor(0, 1); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        N = note_nomi[(nota % 12)];
        if (N > 96)
        { // la nota e' minuscola: e' un diesis (#)
            N -= 32;
            lcd.print(N);
            lcd.print('#');
        }
        else
        {
            lcd.print(N);
        }
        Ottava = (int)((nota) / 12 - 1);
        lcd.print(Ottava);
        break;

    case 5: // cluster
        lcd.clear();
        lcd.print("chord:");
        lcd.print(cluster);
        lcd.setCursor(0, 1); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        u = 0;
        while (u <= 3)
        {
            v = chord[4 * cluster + u];
            lcd.print(v);
            lcd.print(",");
            u++;
        }
        break;

    case 6: // volume e pitch bend
        lcd.clear();
        Show_page(7);
        Show_page(8);
        break;

    case 7:                  // variazione volume
        lcd.setCursor(0, 0); // (colonna, riga)
        Volume_bar();
        displayed_volume = vol_value;
        lcd.print("        ");
        break;

    case 8:                  // variazione pitch
        lcd.setCursor(0, 1); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        Pitch_bar();
        lcd.print("        ");
        break;

    case 9: // inizio setup
        lcd.clear();
        lcd.print("setup");
        break;

    case 10: // fine setup
        lcd.clear();
        lcd.print("done");
        break;

    case 11: // estensione pitch bend
        lcd.clear();
        lcd.print("range:");
        lcd.setCursor(0, 1); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        lcd.print(pitch_range / 12);
        lcd.setCursor(3, 1); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        lcd.print("oct.");
        break;

    case 12:                 // notes off
        lcd.setCursor(0, 0); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        lcd.print(" ");
        break;

    case 13:                 // notes on
        lcd.setCursor(0, 0); // (riga(dal basso verso l'alto), colonna (da sx a dx))
        lcd.print("*");
        break;
    }
}

// Pitch bend range
void Midi_pitch_range(int semitones)
{
    Midi(176 + midi, 101, 0);
    Midi(176 + midi, 100, 0);
    Midi(176 + midi, 6, semitones); // PR indica il numero di semitoni +/- per un'estensione totale pari a 2PR
    Midi(176 + midi, 101, 127);
    Midi(176 + midi, 100, 127);
}

void Volume_bar(void)
{
    int j = static_cast<uint8_t>(vol_value / 3.1f);
    Bar(j, 0);
}

void Pitch_bar(void)
{
    int j = pitch_value_10bit * 0.039f; // j=[0, 39]
    Bar(j, 1);
}

void Midi(uint8_t cmd, uint8_t data1, uint8_t data2)
{
    Serial.write(cmd);   // Serial.print(cmd, BYTE);
    Serial.write(data1); // BYTE
    Serial.write(data2); // BYTE
}

void Bar(int value, int row)
{
    switch (value)
    {
    case 0:
        if (row == 0)
        {
            lcd.print("no wind");
        }
        else
        {
            lcd.print(" ");
        }
        break;
    case 1:
        lcd.write(1);
        break;
    case 2:
        lcd.write(2);
        break;
    case 3:
        lcd.write(3);
        break;
    case 4:
        lcd.write(4);
        break;
    case 5:
        lcd.write(5);
        break;
    case 6:
        lcd.write(5);
        lcd.write(1);
        break;
    case 7:
        lcd.write(5);
        lcd.write(2);
        break;
    case 8:
        lcd.write(5);
        lcd.write(3);
        break;
    case 9:
        lcd.write(5);
        lcd.write(4);
        break;
    case 10:
        lcd.write(5);
        lcd.write(5);
        break;
    case 11:
        lcd.write(5);
        lcd.write(5);
        lcd.write(1);
        break;
    case 12:
        lcd.write(5);
        lcd.write(5);
        lcd.write(2);
        break;
    case 13:
        lcd.write(5);
        lcd.write(5);
        lcd.write(3);
        break;
    case 14:
        lcd.write(5);
        lcd.write(5);
        lcd.write(4);
        break;
    case 15:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        break;
    case 16:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(1);
        break;
    case 17:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(2);
        break;
    case 18:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(3);
        break;
    case 19:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(4);
        break;
    case 20:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        break;
    case 21:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(1);
        break;
    case 22:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(2);
        break;
    case 23:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(3);
        break;
    case 24:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(4);
        break;
    case 25:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        break;
    case 26:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(1);
        break;
    case 27:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(2);
        break;
    case 28:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(3);
        break;
    case 29:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(4);
        break;
    case 30:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        break;
    case 31:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(1);
        break;
    case 32:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(2);
        break;
    case 33:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(3);
        break;
    case 34:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(4);
        break;
    case 35:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        break;
    case 36:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(1);
        break;
    case 37:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(2);
        break;
    case 38:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(3);
        break;
    case 39:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(4);
        break;
    case 40:
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        lcd.write(5);
        break;
    }
}