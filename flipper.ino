
int PIN_START = A1;
int PIN_CORSIE = A2;
int PIN_LO_SCATTO = A3;
int PIN_MOLLE = A4;
int PIN_RUOTE = A5;
int PIN_GIRELLA = 13;

int total = 0;
int molla, start, corsie, ruote, scatto;
int counter_molla, counter_ruote;

void setupPin() {
  pinMode(PIN_START, INPUT);
  pinMode(PIN_CORSIE, INPUT);
  pinMode(PIN_LO_SCATTO, INPUT);
  pinMode(PIN_MOLLE, INPUT);
  pinMode(PIN_RUOTE, INPUT);
  pinMode(PIN_GIRELLA, OUTPUT);
}

void sensorIsActive(int PIN, int state, String name) {
  if(state == 0){
    Serial.println(name);
    digitalWrite(PIN, HIGH);
    delay(55);
  }
}

int sensorActive(int PIN, int state, String name) {
  if(state == 0){
    // Serial.println(PIN);
    digitalWrite(PIN, HIGH);
    delay(55);
    return PIN;
  }else{
    return 0;  
  }
}

int assignCounter(int type) {
  switch(type){
    case 16:
      return 50;
    case 18:
      return 10;
    case 19:
      return 5;
    case 0:
     return 0;
  }
}

void sendVolt(int PIN, String type) {
  if(type == "h"){
    digitalWrite(PIN, HIGH);
  }else{
    digitalWrite(PIN, LOW);
  }
}

void readData() {
  
  molla = digitalRead(PIN_MOLLE);
  sensorIsActive(PIN_MOLLE, molla, "molla");
  corsie = digitalRead(PIN_CORSIE);
  sensorIsActive(PIN_CORSIE, corsie, "corsie");
  start = digitalRead(PIN_START);
  sensorIsActive(PIN_START, start, "start");
  ruote = digitalRead(PIN_RUOTE);
  sensorIsActive(PIN_RUOTE, ruote, "ruote");
  scatto = digitalRead(PIN_LO_SCATTO);
  sensorIsActive(PIN_LO_SCATTO, scatto, "LO SCATTO");
  
}

void gameLoop() {
  molla = digitalRead(PIN_MOLLE);
  int pinActiveMolla = sensorActive(PIN_MOLLE, molla, "molla");
  total += assignCounter(pinActiveMolla);
  ruote = digitalRead(PIN_RUOTE);
  int pinActiveRuote = sensorActive(PIN_RUOTE, ruote, "ruote");
  total += assignCounter(pinActiveRuote);
  corsie = digitalRead(PIN_CORSIE);
  int pinActiveCorsie = sensorActive(PIN_CORSIE, corsie, "corsie");
  total =+ assignCounter(pinActiveCorsie);
  scatto = digitalRead(PIN_LO_SCATTO);
  int pinActiveScatto = sensorActive(PIN_LO_SCATTO, scatto, "scatto");
  total += assignCounter(pinActiveScatto);
}

void setup() {
  Serial.begin(9600);
  setupPin();
}

void loop() {

  // readData();

  int tempTotal = total;
  gameLoop();
  
  if(total > 0 && tempTotal != total){   
    if(total >= 100 && total < 180) {
    
      sendVolt(PIN_GIRELLA, "h");
    }
    if(total >= 180){
      sendVolt(PIN_GIRELLA, "l");
    }
    Serial.println(total);
  }
  
}
