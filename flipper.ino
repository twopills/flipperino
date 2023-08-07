//@TODO
/*

  - EEPROM --> Assegnazione PUNTEGGI
  - EEPROM --> Stampa PUNTEGGI al termine della partita
  - EEPROM --> Gestione CLASSIFICA PUNTEGGI

*/
#include <LCDWIKI_GUI.h> //Core graphics library
#include <LCDWIKI_KBV.h> //Hardware-specific library
#include <LCDWIKI_TOUCH.h>
//#include "declaration.h"

#define BG_COLOR 0x10E3
#define WHITE 0xF

#include <stdbool.h>
#include <Arduino.h>
#include <EEPROM.h>
#define ACTIVATED 1
#define DEACTIVATED 0
#define START 55
#define CORSIE 56
#define SCATTO 57
#define MOLLE 58
#define RUOTE 59
#define PIN_START A1 // PIN: 55 - VECCHIO: 15
#define PIN_CORSIE A2 // PIN: 56 - VECCHIO: 16
#define PIN_LO_SCATTO A3 // PIN: 57 - VECCHIO: 17
#define PIN_MOLLE A4 // PIN: 58 - VECCHIO: 18
#define PIN_RUOTE A5 // PIN: 59 - VECCHIO: 19
#define PIN_GIRELLA 13
#define PIN_SPINTA 12
#define PROFILES 11

LCDWIKI_KBV my_lcd(NT35510,40,38,39,43,41); //model,cs,cd,wr,rd,reset
LCDWIKI_TOUCH my_touch(53,52,50,51,44); //tcs,tclk,tdout,tdin,tirq

char name[4] = "LMZ";

struct COMBO {
  int time;
  float mult;
  int hit;
  bool active;
};

COMBO Combo_0 = { time: 12000, mult: 1, hit: 0, active: true };
COMBO Combo_1 = { time: 5000, mult: 1.5, hit: 15, active: false };
COMBO Combo_2 = { time: 3000, mult: 1.75, hit: 25, active: false };
COMBO Combo_3 = { time: 2000, mult: 2, hit: 30, active: false };
COMBO Combo_4 = { time: 1250, mult: 3, hit: 34, active: false };

struct GAME {
  int hitCounter, comboHitCounter, lastSensorHit, lastSensorConsecutiveHit;
  unsigned long int lastSensorHitTime, lastComboHitTime, lifeStartTime;
  long int startTime;
  int score, lastLifeScore, lastUpdatedScore;
  bool inGame, comboBreak, spinnerActive;
  int life;   
  int tempLife;
};

GAME Game = { 
  hitCounter: 0, comboHitCounter: 0, 
  lastSensorHit: 0, lastSensorConsecutiveHit: 0, 
  lastSensorHitTime: 0, lastComboHitTime: 0, lifeStartTime: 0, startTime: -1,
  score: 0, lastLifeScore: 0, lastUpdatedScore: 0, 
  inGame: false, comboBreak: false, spinnerActive: false, 
  life: 3,
  tempLife: 0
};

struct PROFILE {
  char name[4]; //Nome in display e identificativo univoco (non possono esserci doppioni)
  unsigned short int highScore, eepromPos; //Punteggio più alto - Posizione in EEPROM
  unsigned short int profileActive; //Check se il profilo è attivo o meno
};
PROFILE activeProfile = { name: "AAA", highScore: 0, eepromPos: 0, profileActive: 0 };
PROFILE Default_profile = { name: "DEF", highScore: 1500, eepromPos: 0, profileActive: 1 };
PROFILE blankProfile = { name: "   ", highScore: 0, eepromPos: 0, profileActive: 255 };
PROFILE StoredProfile; //Variabile per lettura PROFILI SALVATI

#define PROFILE_SIZE sizeof(PROFILE)

struct FLIPPER {
  int molla, start, corsie, ruote, scatto;
  struct PROFILE activeProfile ; //EEPROM ADDRESS del profilo attivo
  int scoreBoard[10];
};
FLIPPER Flipper;

struct EEPROM_M {
  int addressPointer;
};
EEPROM_M EepromManager = {0};

struct SCOREBOARD {
  unsigned short int score;
  char *name;
};
static void insert (SCOREBOARD *sb, char *name, unsigned short int score) {
  sb->name = name;
  sb->score = score;
}
struct SCOREBOARD Scoreboard[10];

struct GAMEDISPLAY {
  bool gameScreenDrawn; //Check se lo schermo di gioco e(con accento) stato aggiornato
  bool pauseMenuDraw; //Check se il draw del menu e(con accento) stato messo in pausa

  bool hitScreenUpdated; //Check se hit counter gia stato aggiornato
  bool spinnerStateUpdated; //
};

GAMEDISPLAY Display = { gameScreenDrawn: false, pauseMenuDraw: false};

int pauseMenuDraw = 0;

String formattedTime() {
  return getMinutes() == 0 ? String(getSecond())+"\"" : String(getMinutes())+"'"+String(getSecond())+"\"";
}

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

// ||--!! METODI SENSORI !!--||
//-- Restituisce se il sensore dato è stato attivato (state == 0)
bool sensorIsActive(int PIN, int state) {
  if(state == DEACTIVATED) { // sensore è stato toccato
    digitalWrite(PIN, HIGH); //ATTIVO il SENSORE
    return true;
  }else{
    return false;
  }
}
//-- Accende o Spegne PIN --> value = ACTIVATED (Acceso) / value = DEACTIVATED (Spento)
void sendVolt(int PIN, int value = DEACTIVATED) {
  if(value == ACTIVATED) {
    digitalWrite(PIN, HIGH);
  }else{
    digitalWrite(PIN, LOW);
  }
}
//-- Restituisce il valore PIN del sensore e lo RIATIIVA
int sensorActive(int PIN, int state) {
  
  if(state == DEACTIVATED) { //Il SENSORE è DISATTIVO
    //Se il pin è SCATTO, gestisco SPINTA
    if (PIN == SCATTO) {
      sendVolt(PIN_SPINTA, ACTIVATED); //Accendo bobina
      delay(1000); //Il tempo è necessario per caricare la bobina e dare forza al magnete. Più di 55 non cambia nulla
      sendVolt(PIN_SPINTA, DEACTIVATED); //Spengo bobina
    }
    delay(20); //Attendo prima di fare altro controllo
    digitalWrite(PIN, HIGH); //ATTIVO il SENSORE
    return PIN;
  }
  return 0;  
  
}

void updateGameScreen(int screenArea, int caseToSkip = -1) {
  const int HIT_NUM = 1;
  const int MULT_NUM = 2;
  const int SCORE_NUM = 3;
  const int SPINNER_STATE = 4;
  const int TIME_NUM = 5;
  const int LIFE_NUM = 6;

  if (screenArea == 10) {
    updateGameScreen(HIT_NUM, caseToSkip);
    updateGameScreen(MULT_NUM, caseToSkip);
    updateGameScreen(SCORE_NUM, caseToSkip);
    updateGameScreen(SPINNER_STATE, caseToSkip);
    updateGameScreen(TIME_NUM, caseToSkip);
    updateGameScreen(LIFE_NUM, caseToSkip);
    return;
  }

  my_lcd.Set_Text_colour(255, 255, 255);
  my_lcd.Set_Draw_color(20, 30, 30);

  switch (screenArea) {
    case HIT_NUM:
      if (caseToSkip != HIT_NUM) {
        my_lcd.Fill_Rectangle(312, 90, 488, 185);
        my_lcd.Set_Text_Size(12); // HIT NUM
        my_lcd.Print_String(String(Game.hitCounter), CENTER, 100);
      }
      break;
    case MULT_NUM:
      if (caseToSkip != MULT_NUM) {
        my_lcd.Fill_Rectangle(312, 383, 488, 436);
        my_lcd.Set_Text_Size(7); // MULT NUM
        my_lcd.Print_String(String(getActiveComboMult()), CENTER, 387);
      }
      break;
    case SCORE_NUM:
      if (caseToSkip != SCORE_NUM) {
        my_lcd.Fill_Rectangle(496, 116, 781, 158);
        my_lcd.Set_Text_Size(3); // SCORE NUM
        my_lcd.Print_String(String(Game.score), getScoreXPosition(Game.score), 125);
      }
      break;
    case SPINNER_STATE:
      if (caseToSkip != SPINNER_STATE) {
        my_lcd.Fill_Rectangle(496, 216, 781, 258);
        my_lcd.Set_Text_Size(3); // SPINNER STATE
        my_lcd.Print_String(Game.spinnerActive ? "ACTIVE" : "UNACTIVE", 615, 225);
      }
      break;
    case TIME_NUM:
      if (caseToSkip != TIME_NUM && Game.life > 0) {
        my_lcd.Fill_Rectangle(496, 316, 781, 358);
        my_lcd.Set_Text_Size(3); // TIME NUM
        my_lcd.Print_String(formattedTime(), 680, 325);
      }
      break;
    case LIFE_NUM:
      if (caseToSkip != LIFE_NUM) {
        my_lcd.Fill_Rectangle(518, 4, 535, 57);
        my_lcd.Set_Text_Size(3); // LIFE NUM
        Serial.println(Game.tempLife);
        my_lcd.Print_String(String(Game.tempLife), 518, 20);
      }
      break;
    default:
      break;
  }
}


// ||--!! METODI STRUCT COMBO !!--|| 
void comboActivator(int comboId, bool status) {  

  Combo_0.active = false;
  Combo_1.active = false;
  Combo_2.active = false;
  Combo_3.active = false;
  Combo_4.active = false;
  
  switch(comboId) {
    case 0:
      Combo_0.active = status;
      break;
    case 1:
      Combo_1.active = status;
      break;
    case 2:
      Combo_2.active = status;
      break;
    case 3:
      Combo_3.active = status;
      break;
    case 4:
      Combo_4.active = status;
      break;
  }
    updateGameScreen(2);
}
float getActiveComboMult() {    
    if(Combo_0.active) {
      return Combo_0.mult;
    } else if(Combo_1.active) {
        return Combo_1.mult;
      } else if(Combo_2.active) {
        return Combo_2.mult;
      } else if(Combo_3.active) {
        return Combo_3.mult;
      } else if(Combo_4.active) {
        return Combo_4.mult;
      }
}
int getActiveComboTime() {
    if(Combo_0.active) {
      return Combo_0.time;
    } else if(Combo_1.active) {
      return Combo_1.time;
    } else if(Combo_2.active) {
      return Combo_2.time;
    } else if(Combo_3.active) {
      return Combo_3.time;
    } else if(Combo_4.active) {
      return Combo_4.time;
    }  
}
int getActiveCombo() {
    if(Combo_0.active) {
      return 0;
    } else if(Combo_1.active) {
      return 1;
    } else if(Combo_2.active) {
      return 2;
    } else if(Combo_3.active) {
      return 3;
    } else if(Combo_4.active) {
      return 4;
    }  
}
int getActiveComboHit() {
    if(Combo_0.active) {
      return Combo_0.hit;
    } else if(Combo_1.active) {
      return Combo_1.hit;
    } else if(Combo_2.active) {
      return Combo_2.hit;
    } else if(Combo_3.active) {
      return Combo_3.hit;
    } else if(Combo_4.active) {
      return Combo_4.hit;
    }  
}

// ||--!! METODI STRUCT GAME !!--||
void upHitCounter() {
  Game.lastSensorConsecutiveHit += 1; //Ho colpito comunque un sensore, quindi lo aumento
  Game.hitCounter += 1; //Aumento il counter generale
  Game.comboHitCounter += 1;
}
//-- Restituisce il valore in punteggio dei vari sensori colpiti
void assignScore(int pinActivated) {

  int baseScore = 0;
  // Variabili di flag
  bool calculateScore = false;
  bool isHitTime = true;
  bool skipHitCounterCheck = false;

  switch(pinActivated) {
    case CORSIE:
      baseScore = 40; // CORSIE
      break;
    case SCATTO:
      baseScore = 30; // SCATTO
      break;
    case MOLLE:
      baseScore = 20; // MOLLE
      break;
    case RUOTE:
      baseScore = 15; // RUOTE
      break;
    case 0:
     baseScore = 0; // NESSUNO COLPITO
     break;
  }
  
  if (pinActivated != DEACTIVATED) {
    Game.spinnerActive = true;
    if(Game.startTime == -1 && pinActivated != START) {
        Game.startTime = millis();
        Game.lifeStartTime = millis();
    }
    if (Game.lastSensorHitTime == 0 ) { // && pinActivated != Game.lastSensorHit) { //Il time non è stato settato e ho colpito un sensore e non sto colpendo nulla (non ho ancora colpito NESSUN sensore dall'inizio della partita)
      upHitCounter();
    } else if (Game.lastSensorHitTime != 0 && pinActivated != Game.lastSensorHit) { //Il time è stato settato e ho colpito un sensore differente e non sto colpendo nulla //Assegno il tempo dell'ultimo sensore colpito
      Game.lastSensorConsecutiveHit = 0; //Riazzero il counter di hit consecutivi
      upHitCounter();
    } else if (pinActivated == Game.lastSensorHit && (millis()-Game.lastSensorHitTime >= 500) && Game.lastSensorConsecutiveHit < 3) {
      upHitCounter();
    }
    calculateScore = true;
    Game.lastSensorHitTime = millis(); //Assegno il tempo a cui l'ultimo sensore è stato colpito
    Game.lastComboHitTime = millis()  ;
    Game.lastSensorHit = pinActivated;
  }

  if ((millis()-Game.lastSensorHitTime) > Combo_0.time && Game.lastSensorHitTime != 0) { //Finisco il tempo massimo per contare gli hit
    isHitTime = false; //Fuori tempo per le HIT
  }

  //Ho ottenuto un combo break?
  //Se la combo ottenuta con il combo break è 0 (getActiveComboTime == Combo_0.time)
    //Continuo normalmente, perchè è come se fossi ripartito dall'inizio
  //Se la combo è > 0 (getActiveComboTime > Combo_0.time)
    //Tengo attiva la combo impostata
    //Evito gli altri controlli, eccetto quello della combo break
  if(Game.comboBreak) {
    if(getActiveComboTime() != Combo_0.time) {
      comboActivator(getActiveCombo(), true);
      skipHitCounterCheck = true;
    }
  }

  //Se skippo il check dell'hitCounter (nella parte sotto), allora controllo se sono arrivato al livello della combo
  if (skipHitCounterCheck) {
    if(Game.comboHitCounter > getActiveComboHit() && (millis()-Game.lastComboHitTime) < getActiveComboTime()) {
      skipHitCounterCheck = false;
      Game.comboBreak = false;
    }
  }

  //Se sono ancora in tempo per la sequenza di HIT
  if(isHitTime) {
    //Controllo se sono oltre il tempo massimo della combo
    if ((millis()-Game.lastComboHitTime) > getActiveComboTime()) { //Combo Break      
      // Scalo di uno ad uno le combo
      if(getActiveComboTime() == Combo_1.time) {
        comboActivator(0, true);
      } else if(getActiveComboTime() == Combo_2.time) {
        comboActivator(1, true);
      } else if(getActiveComboTime() == Combo_3.time) {
        comboActivator(2, true);
      } else if(getActiveComboTime() == Combo_4.time) {
        comboActivator(3, true);
      }
      Game.lastComboHitTime = millis();
      if (!Game.comboBreak)
        Game.comboHitCounter = 0; //Non azzero il counter se sono in una combo break
      Game.comboBreak = true;      
      Serial.println("//-- !! COMBO HIT BREAK !! --\\\\");  
    } else if ((millis()-Game.lastComboHitTime) < getActiveComboTime() && pinActivated != 0 && !skipHitCounterCheck) {
        //Applico i moltiplicatori in base a quanti hit ho fatto
        if(Game.comboHitCounter < Combo_1.hit) {
          comboActivator(0, true);             
        } else if (Game.comboHitCounter >= Combo_4.hit) {      
          comboActivator(4, true);
        } else if (Game.comboHitCounter >= Combo_3.hit) {      
          comboActivator(3, true);
        } else if (Game.comboHitCounter >= Combo_2.hit ) {            
          comboActivator(2, true);
        } else if (Game.comboHitCounter >= Combo_1.hit) {            
          comboActivator(1, true);
        }
        Game.lastComboHitTime = millis();
      }
  } else {
    comboActivator(0, true); //Annullo tutte le combo
    Game.hitCounter = 0;
    Game.comboHitCounter = 0;
    Game.lastSensorHit = 0;
    Game.lastComboHitTime = 0;
    updateGameScreen(1);     
  }

  //Se devo calcolare lo score, eseguo il calcolo
  if (calculateScore) {
    Game.score += (baseScore*getActiveComboMult()); //Aggiungo il punteggio per il sensore colpito insieme al moltiplicatore
    updateGameScreen(4);
    updateGameScreen(1);
    updateGameScreen(3);
  }

  //Se ho tot hit attivi, avvio lo spinner
  if(Game.spinnerActive) {
    sendVolt(PIN_GIRELLA, ACTIVATED);
  } else  {
    sendVolt(PIN_GIRELLA, DEACTIVATED);
  } 
}
//Semplice verifica e gestione dello stato di gioco
void manageGameStatus() {

    //La pallina è su START
    if(ballReady()) {
      Game.spinnerActive = false;
      updateGameScreen(4);
      updateGameScreen(6);
      //Dall'ultima volta che sono stato sullo start ho fatto dei punti e ho ancora le vite.
      if(Game.lastUpdatedScore != Game.score && Game.tempLife > 0 && (Game.score-Game.lastLifeScore) > 45) {
        Game.tempLife -= 1; //Ho perso una vita
        Game.lastLifeScore = Game.score; //Aggiorno il punteggio dell'ultima vita  
        Game.lastUpdatedScore = Game.score; //Segno l'ultimo score fatto da quando ho perso una vita
        Game.hitCounter = 0; //Azzero l'hit counter perchè ho perso una vita
        Game.comboHitCounter = 0; 
        Game.lastSensorHit = 0; //Azzero l'ultimo sensore colpito perchè sono in posizione di partenza
        Game.lastSensorHitTime = millis()+10000; //Segnalo che ho perso una vita e devo azzerare i counter
        Game.lifeStartTime = millis();
        Game.lastSensorConsecutiveHit = 0;     
        Serial.print(" !!! HAI PERSO 1 VITA, ne hai ancora ");
        Serial.print(Game.tempLife);        
        Serial.println(" !!!");
        Serial.println("Entro da GAME STATUS LIFE--");
        updateGameScreen(10);
      }
      //Se ho finito le vite
      if(Game.tempLife == 0) {
        Game.inGame = false; // Hai perso la partita
        Game.lastLifeScore = 0; //Azzero il punteggio dell'ultima vita  
        Serial.print(" !!! HAI PERSO!! Il tuo score: ");  
        Serial.print(Game.score);
        Serial.print(" // TEMPO: ");
        Serial.print((millis()-Game.startTime)/1000);
        Serial.print("s");
        Serial.println(" !!!");
        sendVolt(PIN_GIRELLA, DEACTIVATED);
        Game.startTime = -1;
        saveHighScore(Game.score); //Salvo se ho ottenuto il punteggio record
        updateScoreBoard(); //Aggiorno la ScoreBoard
        scoreBoardSort();
        printScoreBoard(); //Stampo la ScoreBoard
        Serial.println("Entro da GAME STATUS LIFE 0");
        updateGameScreen(10);
        Game.lastUpdatedScore = 0;
        Game.score = 0;
        Display.gameScreenDrawn = false;
        pauseMenuDraw = 0;
        Game.tempLife = Game.life;
        my_lcd.Fill_Screen(BG_COLOR);
      }
    }
}

// ||--!! METODI STRUCT FLIPPER !!--||
//-- Controlla se la pallina è pronta per essere giocata
bool ballReady() {
  Flipper.start = digitalRead(PIN_START);
  if (sensorIsActive(PIN_START, Flipper.start)) {
    return true;
  }else{
    return false;
  }
}
void serialPrintScore(int tempTotal) {
  if(Game.score > 0 && tempTotal != Game.score) { 
      Serial.print("\\ Punteggio: ");
      Serial.print(Game.score);
      Serial.print(" \\ HIT: ");
      Serial.print(Game.hitCounter);
      Serial.print(" \\ COMBO HIT: ");
      Serial.print(Game.comboHitCounter);
      Serial.print(" \\ MULT: ");
      Serial.print(getActiveComboMult());
      Serial.print(" \\ COMBO TIME: ");
      Serial.println(getActiveComboTime());
      updateGameScreen(3);
      updateGameScreen(1);
    }
}

// ||--!! METODI STRUCT PROFILE !!--||
//Ritorina il PROFILE correntemente attivo (profileActive == 1)
struct PROFILE getActiveProfile() {
  
  for (int adrs = 0; adrs <= PROFILE_SIZE*PROFILES; adrs+=PROFILE_SIZE) {
    if(EEPROM.get(adrs, StoredProfile).profileActive == 1)
      return EEPROM.get(adrs, StoredProfile);
    else if (EEPROM.get(adrs, StoredProfile).profileActive == 255)
      return EEPROM.get(adrs, StoredProfile);
  }  
}
//Ritorina il PROFILE in base al valore "name"
struct PROFILE getProfileByName(char name[4]) {
  for (int adrs = 0; adrs <= PROFILE_SIZE*PROFILES; adrs+=PROFILE_SIZE) {
    if(strcmp(EEPROM.get(adrs, StoredProfile).name, name))
      return EEPROM.get(adrs, StoredProfile);
  }  
}
//Resetta i profili attivi e non a 0
void resetProfileActive() { 
  for (int adrs = 0; adrs <= PROFILE_SIZE*PROFILES; adrs+=PROFILE_SIZE) {
    EEPROM.get(adrs, StoredProfile);
    if (StoredProfile.profileActive != 255) {
      StoredProfile.profileActive = 0;
      EEPROM.put(adrs, StoredProfile);
    }
  }    
}
//Cra un nuovo profilo e lo posiziona nel primo spazio disponibile sulla EEPROM
void newProfile() {
  //Trovo il primo spazio non occupato
  Serial.println("=== CREA PROFILO ===");
  int _eepromPos = findEmptyBlock();
  String nameString = Serial.readString();
  char _name[4];

  resetProfileActive();
  
  Serial.flush();
  Serial.print("INSERISCI IL NOME (3 CARATTERI): ");
  while (Serial.available() == 0) {}
  nameString = Serial.readString();

  for (int i = 0; i <= 2; i++)
    _name[i] = nameString[i];
  _name[3] = '\0';

  Serial.println(_name);

  nameString = Serial.readString();
  Serial.flush();  

  PROFILE newProfile;
  for (int i = 0; i <= 3; i++)
    newProfile.name[i] = _name[i];
  newProfile.highScore = 0;
  newProfile.eepromPos = _eepromPos;
  newProfile.profileActive = 1;
  EEPROM.put(newProfile.eepromPos, newProfile);
  Flipper.activeProfile = newProfile;

  Serial.println("=== PROFILO CREATO! ===");
  mainMenu();
}
//Seleziona e attiva il profilo scelto
void selectProfile() {

  Serial.println("=== SELEZIONA PROFILO ===");
  int choiceSelect = Serial.parseInt();
  int choiceAddress;
  String exit = Serial.readString();

  for (int adrs = 0; adrs <= PROFILE_SIZE*PROFILES; adrs+=PROFILE_SIZE) {
    if(EEPROM.get(adrs, StoredProfile).profileActive != 255) {
      Serial.print(adrs/PROFILE_SIZE+1);
      Serial.print(". ");
      if (StoredProfile.profileActive == 1)
        Serial.print("ATTIVO: ");
      Serial.println(StoredProfile.name);
    }
  }

  while (Serial.available() == 0) {}

  choiceSelect = Serial.parseInt();
  choiceAddress = (choiceSelect - 1) * PROFILE_SIZE;
  Serial.print("HAI SCELTO: ");
  Serial.println(choiceSelect);
  choiceSelect = Serial.parseInt();

  resetProfileActive();
  EEPROM.get(choiceAddress, StoredProfile);
  StoredProfile.profileActive = 1;
  EEPROM.put(choiceAddress, StoredProfile);
  Flipper.activeProfile = StoredProfile;
  Serial.println("----- NUOVA SELEZIONE -----");

  for (int adrs = 0; adrs <= PROFILE_SIZE*PROFILES; adrs+=PROFILE_SIZE) {
    if(EEPROM.get(adrs, StoredProfile).profileActive != 255) {
      Serial.print(adrs/PROFILE_SIZE+1);
      Serial.print(". ");
      if (StoredProfile.profileActive == 1)
        Serial.print("ATTIVO: ");
      Serial.println(StoredProfile.name);
    }
  }

  Serial.println("=== SELEZIONE CORRETTA? y/n ===");
  while (Serial.available() == 0) {}

  exit = Serial.readString();
  exit.toUpperCase();

  if(exit[0] == 'N') {
    selectProfile();
  } else {
    mainMenu();
  }  

}
//Carica il profilo di Default
void loadDefaultProfile() {
  EEPROM.put(Default_profile.eepromPos, Default_profile);
  Flipper.activeProfile = Default_profile;
}
//Controlla il profilo correntemente attivo
void checkProfile() {
   //Fino a quando non ho un profilo attivo
  if (Flipper.activeProfile.profileActive == 0) { 
    Flipper.activeProfile = getActiveProfile(); //Cerco il profilo attivo nella EEPROM

    if(Flipper.activeProfile.profileActive == 255) {
      Serial.println("=== NON ESISTONO PROFILI --> CREANE UNO ===");
      mainMenu();
    }
  }
}
//Controlla se esiste un profilo con lo stesso "name"
bool checkIfExistSameName(char name[4]) {
  for (int adrs = 0; adrs <= PROFILE_SIZE*PROFILES; adrs+=PROFILE_SIZE) {
    EEPROM.get(adrs, StoredProfile);
    if(strcmp(StoredProfile.name, name)) return true;
  } return false;
}
//Cerca la posizione in EEPROM in cui è libera la posizione
int findEmptyBlock() {
  for (int adrs = 0; adrs <= PROFILE_SIZE*PROFILES; adrs+=PROFILE_SIZE) {
    EEPROM.get(adrs, StoredProfile);
    if(StoredProfile.profileActive == 255)
      return adrs;
  }
}
void saveHighScore(int score) {
  
  if(Flipper.activeProfile.highScore < score) {
    EEPROM.get(Flipper.activeProfile.eepromPos, StoredProfile);
    StoredProfile.highScore = score;
    EEPROM.put(Flipper.activeProfile.eepromPos, StoredProfile);
    Flipper.activeProfile = StoredProfile;
    
    Serial.print("!!! COMPLIMENTI ");
    Serial.print(Flipper.activeProfile.name);
    Serial.println(", NUOVO PUNTEGGIO RECORD PERSONALE !!!");
  } else {
    Serial.print("!!! RECORD PERSONALE: ");
    Serial.print(Flipper.activeProfile.highScore);
    Serial.print(" !!!");
  }
}

// ||--!! METODI EEPROM !!--||
//Stampa tutti i valori della EEPROM per i PROFILI
void debugEEPROM() {
  
  for (int adrs = 0; adrs <= PROFILE_SIZE*PROFILES; adrs+=PROFILE_SIZE) {
    EEPROM.get(adrs, StoredProfile);
    Serial.print("Name: ");
    Serial.print(StoredProfile.name);
    Serial.print(" / EEPROM pos: ");
    Serial.print(StoredProfile.eepromPos);
    Serial.print(" / Active: ");
    if (StoredProfile.profileActive == 255)
      Serial.print("NON ESISTE");
    else Serial.print(StoredProfile.profileActive);
    Serial.print(" / HighScore: ");
    Serial.println(StoredProfile.highScore);
  }
}
//Azzera i valori della EEPROM e assegna dei PROFILE "blankProfile"
void flushEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);    
  }
  for (int i = 0; i <= PROFILE_SIZE*PROFILES; i+=10) {
    blankProfile.eepromPos = i;
    if (i==0)
      EEPROM.put(i, blankProfile);
    EEPROM.put(i, blankProfile);
  }
}

//Stampa la ScoreBoard
void printScoreBoard() {
  for(SCOREBOARD scoreB : Scoreboard) {
    Serial.println(scoreB.name);
  }
}

int lastUpdatedGameTime = 0;
void updateGameTimeDraw() {
  if ((millis() - lastUpdatedGameTime) > 999) {
    lastUpdatedGameTime = millis();
    updateGameScreen(5);
  }  
}

// ||--!! METODI LOOP !!--||
//Loop di gioco
void gameLoop(int tempTotal) {

  int pinActive = 0;
  //Gestisco il sensore di MOLLA
  Flipper.molla = digitalRead(PIN_MOLLE);
  int pinActiveMolla = sensorActive(PIN_MOLLE, Flipper.molla);
  if (pinActiveMolla != DEACTIVATED)
    pinActive = pinActiveMolla;

  //Gestisco il sensore di RUOTE
  Flipper.ruote = digitalRead(PIN_RUOTE);
  int pinActiveRuote = sensorActive(PIN_RUOTE, Flipper.ruote);
  if (pinActiveRuote != DEACTIVATED)
    pinActive = pinActiveRuote;

  //Gestisco il sensore di CORSIE
  Flipper.corsie = digitalRead(PIN_CORSIE);
  int pinActiveCorsie = sensorActive(PIN_CORSIE, Flipper.corsie);
  if (pinActiveCorsie != DEACTIVATED)
    pinActive = pinActiveCorsie;

  //Gestisco il sensore di SCATTO
  Flipper.scatto = digitalRead(PIN_LO_SCATTO);
  int pinActiveScatto = sensorActive(PIN_LO_SCATTO, Flipper.scatto);
  if (pinActiveScatto != DEACTIVATED)
    pinActive = pinActiveScatto;
  
  assignScore(pinActive);

  serialPrintScore(tempTotal);
}
void mainMenu() {
  int choice = managerTouchScreen();
  
  // int choice = -1;
  // choice = Serial.parseInt();

//  Serial.println("=== MENU ===");
//  Serial.print("// PROFILO ATTIVO: ");
//  Serial.println(Flipper.activeProfile.name);
//  Serial.println("Premi 1. Per Giocare");
//  Serial.println("Premi 2. Per Cambiare profilo");
//  Serial.println("Premi 3. Per Creare un profilo");
//  Serial.println("Premi 4. Per vedere la Scoreboard");

  // while (Serial.available() == 0) {}

  // Serial.flush();
  // choice = Serial.parseInt();

  switch(choice) {
    case 1: 
      if (Flipper.activeProfile.profileActive == 1) {
        Game.tempLife = Game.life;
        Game.inGame = true;
        Serial.print("///== GIOCATORE: ");
        Serial.print(Flipper.activeProfile.name);
        Serial.println(" ==\\\\\\");
      } else
        Serial.println("!!! NESSUN PROFILO ATTIVO !!!");
      break;              
    case 2: selectProfile(); break;
    case 3: newProfile(); break; 
    case 4: printScoreBoard(); break;
    default: break;    
  }
}

// ||--!! METODI STRUCT SCOREBOARD !!--||
//Bool compare dello Score

//Sort della ScoreBoard
int compare_two_events(SCOREBOARD *a, SCOREBOARD *b) {
    if(a->score < b->score) return -1;
    if(a->score > b->score) return 1;
    return 0;
}
void scoreBoardSort() {
  qsort(Scoreboard, 10, sizeof(SCOREBOARD), compare_two_events);
}
//Aggiorna la ScoreBoard
void updateScoreBoard() {
  Serial.println("Aggiorno la scoreboard");
  for(SCOREBOARD scoreB : Scoreboard) {
    //insert(&Scoreboard[i], scoreB.name, scoreB.score);
  }
}

//Inizializza la ScoreBoard
void initializeScoreBoard() {
  for (int i = 0; i < 10; i++) {
    EEPROM.get((i*PROFILE_SIZE), StoredProfile);
    insert(&Scoreboard[i], StoredProfile.name, StoredProfile.highScore);
  } 
  scoreBoardSort();
  updateScoreBoard();
}
//Azzera la ScoreBoard
void flushScoreBoard() {

}

int getScoreXPosition(int score){
  int pixel = 20;
  int N;
  int distance = 50;
  if(score < 99999){
    N = 4;
  }
  if(score < 9999){
    N = 3;
  }
  if(score < 999){
    N = 2;
  }
  if(score < 99){
    N = 1;
  }
  if(score == 0){
    distance = 45;
  }
  return my_lcd.Get_Display_Width()-(distance+pixel*N);
}
void drawGameScreen(void){
  
  my_lcd.Fill_Screen(BG_COLOR);
    
  my_lcd.Set_Draw_color(255, 255, 255);
  // generico sotto
  my_lcd.Draw_Rectangle(15, 61, 785, 465);
  // centrale
  my_lcd.Draw_Rectangle(308, 0, 492, 465);
  // rettangolo centralissimo
  my_lcd.Draw_Rectangle(308, 268, 492, 325);
  // sotto sx
  my_lcd.Draw_Rectangle(15, 112, 308, 162);
  // sotto dx
  my_lcd.Draw_Rectangle(492, 112, 785, 162);
  // sotto 2 dx
  my_lcd.Draw_Rectangle(492, 212, 785, 262);
  // sotto 3 dx
  my_lcd.Draw_Rectangle(492, 262, 785, 312);
  // sotto 4 dx
  my_lcd.Draw_Rectangle(492, 312, 785, 362);
  
  // sopra sx riempito
  my_lcd.Fill_Rectangle(15, 62, 308, 112);
  // sopra dx riempito
  my_lcd.Fill_Rectangle(492, 62, 785, 112);
  // sotto dx
  my_lcd.Fill_Rectangle(492, 162, 785, 212);
  // sotto dx
  my_lcd.Fill_Rectangle(492, 262, 785, 312);
  // NOME PROFILO ATTIVO
  my_lcd.Set_Text_colour(255, 255, 255);
  my_lcd.Set_Text_Size(4); // moltiplicatore di font size
  my_lcd.Set_Text_Mode(1); // mode > 0 : senza sfondo
  my_lcd.Print_String(Flipper.activeProfile.name, CENTER, 16); // stringa, posizione, margin-top
  // HIT
  my_lcd.Set_Text_Size(5);
  my_lcd.Print_String("HIT", CENTER, 215); 
  // MULT
  my_lcd.Set_Text_Size(3);
  my_lcd.Print_String("MULT", CENTER, 340); 
  // LIFE TEXT
  my_lcd.Set_Text_Size(3); 
  my_lcd.Print_String("LIFE", 690, 20);
  // SCORE TEXT
  my_lcd.Set_Text_colour(0, 0, 0);
  my_lcd.Set_Text_Size(4); 
  my_lcd.Print_String("SCORE", 518, 74);
  // SPINNER TEXT
  my_lcd.Set_Text_colour(0, 0, 0);
  my_lcd.Set_Text_Size(4); 
  my_lcd.Print_String("SPINNER", 518, 174);
  // TIME TEXT
  my_lcd.Set_Text_colour(0, 0, 0);
  my_lcd.Set_Text_Size(4); 
  my_lcd.Print_String("TIME", 518, 274);

  Display.gameScreenDrawn = true;
}

void drawCheckeredFlag (int sizePx, int h, int w, bool firstFill, int xStart, int yStart) {
  
  my_lcd.Set_Draw_color(255,255,255);
  
  for(int i = 0; i < w; i++) { //Scorro in orizzontale
    for (int j = 0; j < h; j++) { //Scorro in verticale
      if (firstFill) {
        if (i%2 == 0) {
          if(j == 0 && i == 0) //Il primo quadrato in alto a sinistra
            my_lcd.Fill_Rectangle(xStart, yStart, xStart+sizePx, yStart+sizePx);
          else if(j%2 == 0)
            my_lcd.Fill_Rectangle(xStart, yStart+(sizePx*j), xStart+sizePx, yStart+((sizePx*j)+sizePx));
        } else if(j%2 != 0) {
            my_lcd.Fill_Rectangle(xStart, yStart+((sizePx)*j), xStart+sizePx, yStart+((sizePx)*j)+sizePx);
        }
      }
    }
    xStart += sizePx;
  }
}

int getSecond() {
  int second = ((millis()-Game.startTime)/1000);
  int minutes;
  if((second / 60) >= 1){
    minutes = second / 60;
    second = second - (60*minutes);
  }
  return second;
}
int getMinutes() {
  int second = ((millis()-Game.startTime)/1000);
  int minutes = 0;
  if((second / 60) >= 1){
    minutes = second / 60;
    second = second - (60*minutes);
  }
  return minutes;
}



void debugFlushGameScreen() { // FLUSH DELLO SCHERMO DI GIOCO PER DEBUG
  //COLORE DEL BG
  my_lcd.Set_Draw_color(20, 30, 30);
  
  // ROSSO / PER DEBUG
  //my_lcd.Set_Draw_color(255, 0, 0);

  // Centrale Medio // HIT
  my_lcd.Fill_Rectangle(312, 90, 488, 185);
  
  // Centrale Medio Sotto // MULT TIME
  my_lcd.Fill_Rectangle(312, 272, 488, 321);
  
  // Centrale sotto // MULT
  my_lcd.Fill_Rectangle(312, 383, 488, 436);
  
  // Sinistra sotto // SCORE TO NEXT
  my_lcd.Fill_Rectangle(19, 118, 304, 158);
 
  // Destra sotto // SCORE NUM
  my_lcd.Fill_Rectangle(496, 116, 781, 158);
 
  // Destra sotto // SPINNER ACTIVE
  my_lcd.Fill_Rectangle(496, 216, 781, 258);
 
  // Destra sotto // TIME NUM
  my_lcd.Fill_Rectangle(496, 316, 781, 358);

  // Destra sopra // LIFE
  my_lcd.Fill_Rectangle(518, 4, 535, 57);

}

void writeTextLCD(int R, int G, int B, int _size, String value, int relativePositionX, int xStart, int yStart) {
  my_lcd.Set_Text_colour(R, G, B);
  my_lcd.Set_Text_Size(_size); // moltiplicatore di font size
  my_lcd.Set_Text_Mode(1); // mode > 0 : senza sfondo
  if(xStart == -1){
    switch(relativePositionX){
      case 0:
        my_lcd.Print_String(value, LEFT, yStart); // stringa, posizione, margin-top   
        break;
      case 1:
        my_lcd.Print_String(value, CENTER, yStart); // stringa, posizione, margin-top   
        break;
     case 2:
        my_lcd.Print_String(value, RIGHT, yStart); // stringa, posizione, margin-top   
        break;
    }
  } else {
       my_lcd.Print_String(value, xStart, yStart); // stringa, posizione, margin-top    
  }
}

void setup() {

  my_lcd.Init_LCD();
  my_lcd.Fill_Screen(0x0);  
  my_lcd.Set_Rotation(1);
  my_touch.TP_Set_Rotation(0);


  //CLEAR DELLA EPROM SE SERVE
  
  // FIXME: RIPRISTINARE TUTTI A -1
  /*
  for (int i = 0 ; i < PROFILE_SIZE ; i++) {
    EEPROM.put(i, blankProfile); 
  }*/
  Serial.begin(9600);
  delay(3000);
  setupPin();

  //loadDefaultProfile(); //Carica il DefaultProfile nella EEPROM
  //newProfile();
  //flushEEPROM();
  //debugEEPROM();
  initializeScoreBoard();
  Serial.flush();
  
  //Spengo subito il pin di SPINTA
  sendVolt(PIN_SPINTA, DEACTIVATED);
  sendVolt(PIN_GIRELLA, DEACTIVATED);
  Serial.print("//EEPROM LENGTH: ");
  Serial.println(EEPROM.length());
  Serial.print("//PROFILE SIZE: ");
  Serial.println(sizeof(PROFILE));
  Serial.println("=========================");
  Serial.println("       FLIPPER.INO       ");
  Serial.println("   G R A N D   P R I X   ");
  Serial.println("=========================");

  checkProfile();
}

void drawMainMenu() {

  my_lcd.Set_Draw_color(255,255,255);
  my_lcd.Draw_Rectangle(188, 313, 188+(20*5), 313+(20*5));
  writeTextLCD(255,255, 255, 3, "PLAY", -1, 208, 280);

  my_lcd.Set_Draw_color(40, 220, 80);  
  my_lcd.Fill_Rectangle(288+10, 313, 288+10+(20*5), 313+(20*5));
  writeTextLCD(255,255, 255, 3, "USER", -1, 314, 280);
  
  my_lcd.Set_Draw_color(255, 220, 30);
  my_lcd.Fill_Rectangle(388+20, 313, 388+20+(20*5), 313+(20*5));
  writeTextLCD(255,255, 255, 3, "SCORE", -1, 413, 280);
  
  my_lcd.Set_Draw_color(40, 40, 70);
  my_lcd.Fill_Rectangle(488+30, 313, 488+30+(20*5), 313+(20*5));
  writeTextLCD(255,255, 255, 3, "EDIT", -1, 538, 280);

  drawCheckeredFlag(20, 5, 5, true, 188, 313);
  
  my_lcd.Draw_Rectangle(288+10, 313, 288+10+(20*5), 313+(20*5));
  my_lcd.Draw_Rectangle(388+20, 313, 388+20+(20*5), 313+(20*5));
  my_lcd.Draw_Rectangle(488+30, 313, 488+30+(20*5), 313+(20*5));
  
  my_lcd.Set_Draw_color(100,100,100);
  my_lcd.Draw_Rectangle(588+40, 313,    588+40+45, 313+45);
  my_lcd.Draw_Rectangle(588+40, 313+55, 588+40+45, 313+100);
  
}

boolean is_pressed(int16_t y1,int16_t x1,int16_t y2,int16_t x2,int16_t px,int16_t py) {
  if((px > x1 && px < x2) && (py > y1 && py < y2)) {
      Serial.println("true");
      return true;  
  } else {
    // Serial.println("false");
      return false;  
  }
}
int managerTouchScreen() {
  int16_t px = 0;
  int16_t py = 0;
  my_touch.TP_Scan(0);
  if (my_touch.TP_Get_State()&TP_PRES_DOWN) {
    px = my_touch.x;
    py = my_touch.y;
    Serial.println(px);
    Serial.println(py);
  } 
  if(is_pressed(799-(188+(20*5)), 479-(313+(20*5)), 799-188, 479-313, px, py)){
    return 1;
  }
  return -1;
}

void loop() {

  // Game.inGame = true;

  if (!Game.inGame) {  
    if(!Display.pauseMenuDraw){
      drawMainMenu();
      Display.pauseMenuDraw = true;
    }
    mainMenu(); //Se non sono in game, entro nel menù
  }

  //Sono in game
  if (Game.inGame) {
    if (!Display.gameScreenDrawn) {
      drawGameScreen();
      updateGameScreen(10);
    }
    int tempTotal = Game.score;
    gameLoop(tempTotal);
    manageGameStatus();
    updateGameTimeDraw();
  }
  delay(1);
}
