#include <Arduino.h>

int PIN_START = A1; // PIN: 15
int PIN_CORSIE = A2; // PIN: 16
int PIN_LO_SCATTO = A3; // PIN: 17
int PIN_MOLLE = A4; // PIN: 18
int PIN_RUOTE = A5; // PIN: 19
int PIN_GIRELLA = 13;
int PIN_SPINTA = 12;

int score;
int lastUpdatedScore;
int molla, start, corsie, ruote, scatto;
bool inGame = false;
int life = 3;

void setupPin() {
  //PIN INPUT -- LETTURA
  pinMode(PIN_START, INPUT);
  digitalWrite(PIN_START, HIGH);
  pinMode(PIN_CORSIE, INPUT);
  digitalWrite(PIN_CORSIE, HIGH);
  pinMode(PIN_LO_SCATTO, INPUT);
  digitalWrite(PIN_LO_SCATTO, HIGH);
  pinMode(PIN_MOLLE, INPUT);
  digitalWrite(PIN_MOLLE, HIGH);
  pinMode(PIN_RUOTE, INPUT);
  digitalWrite(PIN_RUOTE, HIGH);
  //PIN OUTPUT -- SCRITTURA
  pinMode(PIN_GIRELLA, OUTPUT);
  pinMode(PIN_SPINTA, OUTPUT);
}

//-- Restituisce se il sensore dato è stato attivato (state == 0)
bool sensorIsActive(int PIN, int state, String name) {
  if(state == 0){
    Serial.println(name);
    Serial.println(state);
    delay(55); //Attendo prima di fare altro controllo
    digitalWrite(PIN, HIGH); //ATTIVO il SENSORE
    return true;
  }else{
    return false;
  }
}

//-- Accende o Spegne PIN --> value = "h" (Acceso) / value = "l" (Spento)
void sendVolt(int PIN, String value = "l") {
  if(value == "h"){
    digitalWrite(PIN, HIGH);
  }else{
    digitalWrite(PIN, LOW);
  }
}

//-- Restituisce il valore PIN del sensore e lo RIATIIVA
int sensorActive(int PIN, int state, String name) {
  
  if(state == 0){ //Il SENSORE è DISATTIVO
    //Se il pin è SCATTO, gestisco SPINTA
    if (PIN == 17){
      sendVolt(PIN_SPINTA, "h"); //Accendo bobina
      delay(55); //Il tempo è necessario per caricare la bobina e dare forza al magnete. Più di 55 non cambia nulla
      sendVolt(PIN_SPINTA, "l"); //Spengo bobina
    }
    delay(55); //Attendo prima di fare altro controllo
    digitalWrite(PIN, HIGH); //ATTIVO il SENSORE
    return PIN;
  }else{
    return 0;  
  }
}

//-- Restituisce il valore in punteggio dei vari sensori colpiti
int assignCounter(int type) {
  switch(type){
    case 16:
      return 40; // CORSIE
    case 17:
      return 15; // SCATTO
    case 18:
      return 25; // MOLLE
    case 19:
      return 15; // RUOTE
    case 0:
     return 0; // NESSUNO COLPITO
  }
  return 0;
}

/*Legge i valori dei sensori - NON IN USO
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
  
}*/

//-- Controlla se la pallina è pronta per essere giocata
bool ballReady() {
  start = digitalRead(PIN_START);
  if (sensorIsActive(PIN_START, start, "start")){
    return true;
  }else{
    return false;
  }
}

//Semplice verifica e gestione dello stato di gioco
void manageGameStatus(bool status){
  if (!status){  //NON SONO IN GAME
    //Se la pallina è su START, non sono in game e lo score è 0, allora posso iniziare
    if(ballReady()){
      inGame = true; //Sono all'inizio della mia partita
      lastUpdatedScore = 0;
      score = 0;
      life = 3;
    }
  }else{ //SONO IN GAME
    //La pallina è su START
    if(ballReady()){
      //Dall'ultima volta che sono stato sullo start ho fatto dei punti e ho ancora le vite.
      if(lastUpdatedScore != score && life > 0){
        inGame = true;
        life -= 1; //Ho perso una vita
        lastUpdatedScore = score; //Segno l'ultimo score fatto da quando ho perso una vita
        Serial.print("HAI PERSO 1 VITA, ne hai ancora ");
        Serial.print(life);        
        delay(3000); //Attendo 3 secondi prima di fare altro
      }
      //Se non ho fatto punti e sono sullo start, non mi toglie una vita.    
      if(lastUpdatedScore == score && life > 0){
        inGame = true;
        delay(1000); //Attendo 3 secondi prima di fare altro
      }
      //Se ho finito le vite
      if(life == 0){
        inGame = false; // Hai perso la partita
        Serial.println("HAI PERSO!! Il tuo score: ");  
        Serial.println(score);
        sendVolt(PIN_GIRELLA);
        delay(3000); //Attendo 3 secondi prima di fare altro
      }  
    }    
  }
}

void gameLoop() {

  //Gestisco il sensore di MOLLA
  molla = digitalRead(PIN_MOLLE);
  int pinActiveMolla = sensorActive(PIN_MOLLE, molla, "molla");
  score += assignCounter(pinActiveMolla);

  //Gestisco il sensore di RUOTE
  ruote = digitalRead(PIN_RUOTE);
  int pinActiveRuote = sensorActive(PIN_RUOTE, ruote, "ruote");
  score += assignCounter(pinActiveRuote);

  //Gestisco il sensore di CORSIE
  corsie = digitalRead(PIN_CORSIE);
  int pinActiveCorsie = sensorActive(PIN_CORSIE, corsie, "corsie");
  score += assignCounter(pinActiveCorsie);

  //Gestisco il sensore di SCATTO
  scatto = digitalRead(PIN_LO_SCATTO);
  int pinActiveScatto = sensorActive(PIN_LO_SCATTO, scatto, "scatto");
  score += assignCounter(pinActiveScatto);
}

void setup() {
  Serial.begin(9600);
  setupPin();
  //Spengo subito il pin di SPINTA
  sendVolt(PIN_SPINTA, "l");
  Serial.println("init flipper");

}

void loop() {

  // readData();
  int tempTotal = score;
  
  if (inGame){
    //Loop che controlla tutti i sensori e il gioco
    gameLoop();
    //Dopo il gameloop ottengo un nuovo score se ho colpito qualcosa, altrimenti no. Quindi l'ultimo punteggio aggiornato è quello in cui temp e score sono uguali
    delay(5); //delay di 5ms per non lasciare a full clock
    
    if(score > 0 && tempTotal != score){   
      if(score >= 300 && score < 500) {
        sendVolt(PIN_GIRELLA, "h");
      }
      if(score >= 500){
        sendVolt(PIN_GIRELLA);
      }
      Serial.println(score);
    }
  }else{
      delay(1000); //Ogni 1s ricontrollo lo stato del gioco se non sono in gioco.
    }
  manageGameStatus(inGame);
    
}
