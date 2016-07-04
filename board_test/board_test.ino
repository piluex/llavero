const byte ledPin = 10;
const byte interruptPin = 7;
//volatile byte state = LOW;

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), blink, CHANGE);
}

void loop() {

}

void blink() {
  digitalWrite(ledPin,digitalRead(interruptPin));
}
