/*

E-SHELL DUE
Data: 17/09/2025

Hardware: ARDUINO MINI
Processor: ATmega168
Display: HITACHI HD44780 8x2
Aspetto:
        no wind
        |||
Comandi: Sensore soffio (WIND), sensore Soft Pot (per Pitch Bend), Joystick 5 vie

*/

#include <Arduino.h>
#include <LiquidCrystal.h>
#include "Constants.h"

// Release
const String release = "R3.4 Y25";
/*
R3.4
Durante la configurazione di nota/cluster si continua a leggere il sof pot e il sensore wind

R3.3
Funzione Note_off(cluster)

R3.2
Led barrier abandoned; NoteOn/Off are automatically generated using the wind sensor.
*/

// State
enum state_names
{
  RUN,
  up,
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
int key_status = 0;       // variabile di stato per pulsante
bool note_off_sent = false;
struct NoteOff
{
  int nota;
  int cluster;
  bool pitch_reset;
} note_off_data;

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
uint8_t nota = 60; // nota base (C3 == 60)
uint8_t nota_0 = 60;
char nota_name[6];                 // nome che compare sul display
char note_nomi[] = "CcDdEFfGgAaB"; // nome delle Note
int cluster = 0;                   // cluster id
int cluster_0 = 0;

// Display
// RS=8, R/W=7, E=6, DB-4=5, DB-5=4, DB-6=3, DB-7=2
LiquidCrystal lcd(8, 7, 6, 5, 4, 3, 2);

unsigned long timer[2];

// Functions
void Midi(uint8_t cmd, uint8_t data1, uint8_t data2);
void Note_on(void);
void Note_off(const NoteOff &data);
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

  // Wind Sensor offset
  vol_value_offset = analogRead(vol_pin);

  // Setup pitch bend range
  Midi_pitch_range(pitch_range / 2); // pitch_range/2 indica il numero di semitoni +/- per un'estensione totale pari a pitch_range

  // Set volume = 0
  Midi(176 + midi, 7, 0);

  note_off_data.nota = 0;
  note_off_data.cluster = 0;
  note_off_data.pitch_reset = false;

  // Stop synth
  All_notes_off();

  Show_page(6); // visualizza note off, volume e pitch bend
}

// *************************************************************** LOOP ****************************************************************

void loop()
{

  // Soft potentiometer (Pitch)
  pitch_sensor_value = analogRead(pitch_pin);

  switch (pitch_sensor_status)
  {
  case 0: // no touch
    if (pitch_sensor_value > 2) // sensore impegnato
    {
      pitch_sensor_status = 1;
      pitch_sensor_value_base = pitch_sensor_value;
      pitch_sensor_value_0 = pitch_sensor_value;
      pitch_value_10bit_base = pitch_value_10bit;

      // Serial.print("pitch_sensor_value: ");
      // Serial.println(pitch_sensor_value);
    }
    break;

  case 1: // touched
    if (pitch_sensor_value <= 2 || abs(pitch_sensor_value - pitch_sensor_value_0) > 20)
    {
      pitch_sensor_status = 0;

      // Serial.print(" * pitch_value_10bit: ");
      // Serial.println(pitch_value_10bit);
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

      // Serial.print(" * Sending pitch_value_10bit: ");
      // Serial.println(pitch_value_10bit);
    }
    break;
  }

  // Soft-pot check
  /*
  if (pitch_sensor_status == 1)
    digitalWrite(13, HIGH); // turn the ledPin on
  else
    digitalWrite(13, LOW); // turn the ledPin off
  */



  // Wind sensor (volume)
  vol_value = constrain(map(analogRead(vol_pin) - vol_value_offset, 0, vol_upthr, 0, 127), 0, 127);

  // Serial.println(vol_value);

  // update display
  if (vol_value != vol_value_0 && shell_state == 0) // visualizza il valore del volume
  {
    Show_page(7); // visualizza volume
  }

  // midi messages
  switch (wind_state)
  {
  case 0: // no wind
    if (vol_value >= 3)
    {
      wind_state = 1;
      Note_off(note_off_data);
      delay(1);
      Note_on();
      note_off_data.nota = nota;
      note_off_data.cluster = cluster;
      note_off_data.pitch_reset = false;

      Midi(176 + midi, 7, vol_value); // volume = 0
    }
    break;

  case 1:
    if (abs(vol_value - vol_value_0) > 5)
    {
      Midi(176 + midi, 7, vol_value); // comando volume change
      vol_value_0 = vol_value;
    }
    if (vol_value < 1)
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
      shell_state = up;
      timer[0] = millis();
      timer[1] = timer[0] + 300;
      note_off_data.nota = nota;
      note_off_data.cluster = cluster;
      note_off_data.pitch_reset = true;

      if (nota < 127)
      {
        ++nota;
      }
      Show_page(4); // visualizza intonazione
    }
    else if (digitalRead(down_pin) == LOW)
    {
      shell_state = DOWN;
      timer[0] = millis();
      timer[1] = timer[0] + 300;
      note_off_data.nota = nota;
      note_off_data.cluster = cluster;
      note_off_data.pitch_reset = true;

      if (nota > 0)
      {
        --nota;
      }
      Show_page(4); // visualizza intonazione
    }
    else if (digitalRead(right_pin) == LOW)
    {
      shell_state = RIGHT;
      timer[0] = millis();
      timer[1] = timer[0] + 300;
      note_off_data.nota = nota;
      note_off_data.cluster = cluster;
      note_off_data.pitch_reset = false;

      if (cluster < (int)(sizeof(chord) / 4 - 1))
      {
        ++cluster;
      }
      Show_page(5); // visualizza schema cluster
    }
    else if (digitalRead(left_pin) == LOW)
    {
      shell_state = LEFT;
      timer[0] = millis();
      timer[1] = timer[0] + 300;
      note_off_data.nota = nota;
      note_off_data.cluster = cluster;
      note_off_data.pitch_reset = false;

      if (cluster > 0)
      {
        --cluster;
      }
      Show_page(5); // visualizza schema cluster
    }
    break;

  case up:
    if (digitalRead(up_pin) == LOW)
    {
      if (millis() > timer[1])
      {
        timer[0] = millis();
        timer[1] = timer[0] + 300;
        if (nota < 127)
        {
          ++nota;
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
      if (millis() > timer[1])
      {
        timer[0] = millis();
        timer[1] = timer[0] + 300;
        if (nota > 0)
        {
          --nota;
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
      if (millis() > timer[1])
      {
        timer[0] = millis();
        timer[1] = timer[0] + 300;
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
      if (millis() > timer[1])
      {
        timer[0] = millis();
        timer[1] = timer[0] + 300;
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

  // Pitch Bend sensitivity
  if (analogRead(central_pin) <= soglia)
  {
    Show_page(9); // visualizza "configurazione"
    All_notes_off();

    // ricalcolo dell'offset del Volume Sensor
    vol_value_offset = analogRead(vol_pin);
    menu = 1;
    delay(1000);
    Show_page(11); // visualizza estensione pitch bend in semitoni
    delay(200);
  }

  /*
  // Pitch bend range
  if (menu == 1)
  {
    Show_page(11); // visualizza estensione pitch bend in semitoni
    delay(200);
  }
  */

  while (menu == 1)
  {
    if (digitalRead(left_pin) == LOW && pitch_range >= 24)
    {
      pitch_range -= 12;
      Midi_pitch_range(pitch_range / 2); // pitch_range/2 indica il numero di semitoni +/- per un'estensione totale pari a pitch_range
      pitch_value_10bit = 512;
      pitch_value_10bit_base = 512;
      Pitch_bend(pitch_value_10bit); // comando pitch bend
      Show_page(11);                 // visualizza estensione pitch bend in semitoni
      delay(200);
    }
    else if (digitalRead(right_pin) == LOW && pitch_range <= 108)
    {
      pitch_range += 12;
      Midi_pitch_range(pitch_range / 2); // pitch_range/2 indica il numero di semitoni +/- per un'estensione totale pari a pitch_range
      pitch_value_10bit = 512;
      pitch_value_10bit_base = 512;
      Pitch_bend(pitch_value_10bit); // comando pitch bend
      Show_page(11);                 // visualizza estensione pitch bend in semitoni
      delay(200);
    }
    else if (digitalRead(down_pin) == LOW)
    {
      menu = 2;
      delay(200);
    }
    else if (digitalRead(up_pin) == LOW)
    {
      menu = 1;
    }
    else if (analogRead(central_pin) <= soglia)
    {
      menu = 0;                     // uscita
      Show_page(10);                // visualizza "done"
      pitch_value_10bit = 512;      // Pitch Bend
      pitch_value_10bit_base = 512; // Pitch Bend precedente
      delay(500);
      Show_page(6); // visualizza volume e pitch
    }
  }

  //%%% 2 MIDI CHANNEL
  if (menu == 2)
  {
    Show_page(2); // visualizza canale MIDI
    delay(200);
  }

  while (menu == 2)
  {
    if (digitalRead(left_pin) == LOW && midi > 0)
    {
      midi -= 1;
      Show_page(2); // visualizza canale MIDI
      delay(200);
    }

    else if (digitalRead(right_pin) == LOW && midi < 15)
    {
      midi += 1;
      Show_page(2); // visualizza canale MIDI
      delay(200);
    }

    else if (digitalRead(down_pin) == LOW)
    {
      menu = 3;
      delay(200);
    }

    else if (digitalRead(up_pin) == LOW)
    {
      menu = 1;
      delay(200);
    }

    else if (analogRead(central_pin) <= soglia)
    {
      menu = 0;                     // uscita
      Show_page(10);                // visualizza "done"
      pitch_value_10bit = 512;      // Pitch Bend
      pitch_value_10bit_base = 512; // Pitch Bend precedente
      delay(1000);
      Show_page(6); // visualizza volume e pitch
    }
  }

  //%%% 3 PITCH BEND RESOLUTION

  if (menu == 3)
  {
    Show_page(3); // visualizza bits Pitch Bend
    delay(200);
  }

  while (menu == 3)
  {
    if (digitalRead(left_pin) == LOW && pitch_bits == 14)
    {
      pitch_bits = 7;
      Show_page(3); // visualizza bits Pitch Bend
      delay(200);
    }

    else if (digitalRead(right_pin) == LOW && pitch_bits == 7)
    {
      pitch_bits = 14;
      Show_page(3); // visualizza bits Pitch Bend
      delay(200);
    }

    else if (digitalRead(up_pin) == LOW)
    {
      menu = 2;
      delay(200);
    }

    else if (digitalRead(down_pin) == LOW)
      menu = 3;

    else if (analogRead(central_pin) <= soglia)
    {
      menu = 0;                     // uscita
      Show_page(10);                // visualizza "done"
      pitch_value_10bit = 512;      // Pitch Bend
      pitch_value_10bit_base = 512; // Pitch Bend precedente
      delay(500);
      Show_page(6); // visualizza volume e pitch
    }
  }
}

void Note_on(void)
{
  Midi(144 + midi, nota, 127); // note on nota 1
  auto i = 0;
  while (i <= 3)
  {
    if (chord[i + 4 * cluster] > 0)
    {
      Midi(144 + midi, nota + chord[i + 4 * cluster], 127);
    }
    ++i;
  }
}

void Note_off(const NoteOff &data)
{
  Midi(128 + midi, data.nota, 2); // note off nota 1
  auto i = 0;
  while (i <= 3)
  {
    if (chord[i + 4 * data.cluster] > 0)
    {
      Midi(128 + midi, data.nota + chord[i + 4 * data.cluster], 2);
    }
    ++i;
  }
  if(data.pitch_reset)
  {
    pitch_value_10bit = 512;
    pitch_value_10bit_base = 512;
    Pitch_bend(pitch_value_10bit);
  }
}

void All_notes_off(void)
{
  for (auto i = 0; i < 128; ++i)
  {
    Midi(128 + midi, i, 40);
  }
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
    Midi(224 + midi, 0, msb); // comando Pitch bend 7 bit
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
    lcd.print("+");
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

  case 7:                // variazione volume
    lcd.setCursor(0, 0); // (colonna, riga)
    Volume_bar();
    lcd.print("        ");
    break;

  case 8:                // variazione pitch
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

  case 12:               // notes off
    lcd.setCursor(0, 0); // (riga(dal basso verso l'alto), colonna (da sx a dx))
    lcd.print(" ");
    break;

  case 13:               // notes on
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