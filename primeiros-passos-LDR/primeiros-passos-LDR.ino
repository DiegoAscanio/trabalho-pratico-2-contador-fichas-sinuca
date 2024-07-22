/*
  ESP8266 Blink by Simon Peter
  Blink the blue LED on the ESP-01 module
  This example code is in the public domain

  The blue LED on the ESP-01 module is connected to GPIO1
  (which is also the TXD pin; so we cannot use Serial.print() at the same time)

  Note that this sketch uses LED_BUILTIN to find the pin with the internal LED
*/

bool status = false;
int fichas = 0;

void setup() {
  pinMode(2, INPUT_PULLUP);     // Initialize the LED_BUILTIN pin as an output
  Serial.begin(115200);
}

// the loop function runs over and over again forever
void loop() {
  status = digitalRead(2);
  delay(1000);
  if (status) {
    fichas ++;
    Serial.println(fichas);
    delay(2000);
  }
}
