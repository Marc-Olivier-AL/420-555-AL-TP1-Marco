enum Pin : uint8_t {
  D0 = 16, // starts pulled high, no interrupt or pwm
  D1 =  5,
  D2 =  4,
  D3 =  0, // always pulled high, fails to boot if low
  D4 =  2, // always pulled high, fails to boot if low
  D5 = 14,
  D6 = 12,
  D7 = 13,
  D8 = 15, // always pulled low, fails to boot if high
  RX =  3,
  TX =  1
};

// --- Broches ---
const Pin DATA   = D7;  // SER des registres
const Pin CLK    = D5;  // SRCLK
const Pin LATCH  = D0;  // RCLK
const Pin BOUTON = D1;
const Pin BUZZER = D2;

// --- Position de chaque DEL dans les 16 bits ---
// bits 0 à 7  : premier registre (score, prise, cibles)
// bits 8 à 15 : deuxième registre (barre de puissance)
const uint16_t SCORE[3]     = { 1 << 0, 1 << 1, 1 << 2 };
const uint16_t PRISE        = 1 << 3;
const uint16_t CIBLE[3]     = { 1 << 4, 1 << 5, 1 << 6 };
const uint16_t PUISSANCE[5] = { 1 << 8, 1 << 9, 1 << 10, 1 << 11, 1 << 12 };

// --- Groupes de DEL ---
const uint16_t TOUS_SCORE      = (1 << 0) | (1 << 1) | (1 << 2);
const uint16_t TOUTES_CIBLES   = (1 << 4) | (1 << 5) | (1 << 6);
const uint16_t TOUTE_PUISSANCE = (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12);

uint16_t etat = 0;   // la "mémoire" : quelles DEL sont allumées

const unsigned long TEMPS_MAX = 3000;   // temps (ms) pour remplir la barre de puissance

int points = 0;   // nombre de points marqués (0 à 3)
int prises = 0;   // nombre de prises (0 à 2)

void afficher() {
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLK, MSBFIRST, etat >> 8);    // octet du haut -> barre
  shiftOut(DATA, CLK, MSBFIRST, etat & 0xFF);  // octet du bas  -> score, prise, cibles
  digitalWrite(LATCH, HIGH);
}

void allumer(uint16_t masque)  { etat |= masque;  afficher(); }
void eteindre(uint16_t masque) { etat &= ~masque; afficher(); }
void toutEteindre()            { etat = 0;        afficher(); }

void clignoter(uint16_t masque, int fois, int dureeMs) {
  for (int i = 0; i < fois; i++) {
    allumer(masque);  delay(dureeMs);
    eteindre(masque); delay(dureeMs);
  }
}

// --- SONS ---
void note(int frequence, int dureeMs) {
  tone(BUZZER, frequence, dureeMs);
  delay(dureeMs);
  noTone(BUZZER);
}

void arreterSon() { noTone(BUZZER); }

// Son continu : plus le niveau (1 à 5) est haut, plus la note est aiguë
void sonChargement(int niveau) {
  tone(BUZZER, 300 + niveau * 150);
}

void sonVol() {
  for (int f = 1200; f > 300; f -= 50) {
    tone(BUZZER, f);
    delay(10);
  }
  noTone(BUZZER);
}

void sonSucces() {
  note(880, 100);
  note(1320, 200);
}

void sonEchec() {
  note(200, 300);
  note(150, 400);
}

void victoire() {
  toutEteindre();
  int melodie[6] = { 523, 659, 784, 1047, 784, 1047 };
  int duree[6]   = { 150, 150, 150, 300, 150, 500 };
  for (int i = 0; i < 6; i++) {
    if (i % 2 == 0) {                       // note paire : DEL 1 et 3
      eteindre(SCORE[1]);
      allumer(SCORE[0] | SCORE[2]);
    } else {                                // note impaire : DEL 2
      eteindre(SCORE[0] | SCORE[2]);
      allumer(SCORE[1]);
    }
    note(melodie[i], duree[i]);
  }
}

void defaite() {
  toutEteindre();
  int melodie[4] = { 392, 349, 330, 262 };
  int duree[4]   = { 300, 300, 300, 600 };
  for (int i = 0; i < 4; i++) {
    if (i % 2 == 0) allumer(PRISE);         // la DEL rouge clignote
    else            eteindre(PRISE);
    note(melodie[i], duree[i]);
  }
}

// Attend un appui, charge tant que le bouton est tenu.
// Renvoie la cible atteinte (0, 1 ou 2), ou -1 si hors circuit.
int chargerTir() {
  // 1. Ignorer un appui commencé avant : on attend d'abord que le bouton soit relâché
  while (digitalRead(BOUTON) == LOW) delay(10);

  // 2. Attendre un nouvel appui
  while (digitalRead(BOUTON) == HIGH) delay(10);
  delay(20);                          // anti-rebond à l'appui

  // 3. Charger tant que le bouton est tenu
  unsigned long debut = millis();
  unsigned long duree = 0;
  int niveau = 0;
  sonChargement(0);

  while (digitalRead(BOUTON) == LOW) {
    duree = millis() - debut;

    if (duree >= TEMPS_MAX) {         // maximum atteint : le tir part tout seul
      allumer(PUISSANCE[4]);
      arreterSon();
      return -1;                      // tir hors circuit
    }

    int nouveauNiveau = duree * 5 / TEMPS_MAX;
    if (nouveauNiveau > niveau) {     // une DEL de plus à allumer
      niveau = nouveauNiveau;
      allumer(PUISSANCE[niveau - 1]);
      sonChargement(niveau);
    }
    delay(10);
  }

  // 4. Bouton relâché : le tir part
  arreterSon();
  delay(20);                          // anti-rebond au relâchement
  return duree * 3 / TEMPS_MAX;       // cible atteinte : 0, 1 ou 2
}

void jouerRonde() {
  // 1. Choisir une cible au hasard et l'afficher 1 seconde
  int cible = random(3);              // 0, 1 ou 2
  Serial.print("Cible visee : ");
  Serial.println(cible + 1);
  allumer(CIBLE[cible]);
  delay(1000);                        // pendant ce temps, le bouton n'est pas lu
  eteindre(CIBLE[cible]);

  // 2. Le joueur charge et tire
  int atteinte = chargerTir();        // 0, 1, 2 ou -1 (hors circuit)
  sonVol();

  // 3. Résultat
  if (atteinte == cible) {            // succès
    allumer(CIBLE[atteinte]);
    sonSucces();
    clignoter(SCORE[points], 3, 150);
    allumer(SCORE[points]);           // la DEL de score reste allumée
    points++;
    Serial.println("Succes");
  } else {                            // échec
    if (atteinte != -1) {
      allumer(CIBLE[atteinte]);       // la cible touchée (aucune si hors circuit)
    }
    sonEchec();
    clignoter(PRISE, 3, 150);
    allumer(PRISE);                   // la DEL de prise reste allumée
    prises++;
    Serial.println("Echec");
  }

  // 4. Fin de ronde : on éteint les cibles et la barre, le score et la prise restent
  delay(1000);
  eteindre(TOUTES_CIBLES | TOUTE_PUISSANCE);
  delay(500);
}

void debutDePartie() {
  uint16_t groupes[4] = { PRISE, TOUS_SCORE, TOUTES_CIBLES, TOUTE_PUISSANCE };
  int notes[4] = { 523, 659, 784, 1047 };   // do, mi, sol, do aigu
  for (int tour = 0; tour < 3; tour++) {
    for (int g = 0; g < 4; g++) {
      allumer(groupes[g]);
      note(notes[g], 200);
      eteindre(groupes[g]);
    }
  }
  toutEteindre();
  delay(1500);   // courte pause avant la première ronde
}

void setup() {
  Serial.begin(115200);
  pinMode(DATA, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(BOUTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  toutEteindre();
  randomSeed(analogRead(A0));   // A0 est reliée à rien : elle capte du bruit, donc un point de départ différent à chaque démarrage
}

void loop() {
  // --- Début de partie ---
  points = 0;
  prises = 0;
  toutEteindre();
  debutDePartie();

  // --- Rondes : on continue tant que la partie n'est ni gagnée ni perdue ---
  while (points < 3 && prises < 2) {
    jouerRonde();
  }

  // --- Fin de partie ---
  if (points == 3) {
    victoire();
  } else {
    defaite();
  }
  toutEteindre();
  delay(3000);      // quelques secondes, puis on retourne au début de partie
}