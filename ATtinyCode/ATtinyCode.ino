//// STATES ////
#define sRedLightFlash 0
#define sYellowLightFlash 1
#define sGreenLightFlash 2
#define sRed 3
#define sYellow 4
#define sGreen 5
#define sYellowFlash 6
#define sRedFlash 7
#define sSlowParty 8
#define sFastParty 9
#define sRandom 10
#define sOff 11

int State = sRed;

//// PIN NAMES ////
const int RedLight = 0;
const int YellowLight = 1;
const int GreenLight = 2;
const int ModeButton = 3;
const int PowerSW = 4;

//// VARS ////

//// Steady on Vars ////
const long RedLightLenghth = 26000;
const long YellowLightLength = 4000;
const long GreenLightLength = 40000;
long RedTime = RedLightLenghth;
long YellowTime = YellowLightLength;
long GreenTime = GreenLightLength;
unsigned long CurrentMillis;
unsigned long PreviousMillis;

//// Red and Yellow Flash Vars ////
long PrevLightMillis = 0;
long LightInt1 = 1000;
long LightInt2 = 100;
int NumLight = 2; // Less 1
int LightNum = 0;
bool LightChanged = true;
const long FlashInterval = 500;
int YellowLightState = LOW;
int RedLightState = LOW;

//// Random Flash Vars ////
int NumberOfLights = 3;
long NextFlash[3];
int LightPINs[] = {RedLight, YellowLight, GreenLight};
int LightState[3];

//// Button Debounce Vars ////
int ButtonState;
int LastButtonState = HIGH;
long LastDebounceTime = 0;
long DebounceDelay = 40;
bool ButtonToggle = false;

void setup() {
  //// PIN MODE ////
  pinMode(RedLight, OUTPUT);
  pinMode(YellowLight, OUTPUT);
  pinMode(GreenLight, OUTPUT);
  pinMode(ModeButton, INPUT);
  pinMode(PowerSW, INPUT);

  // Turn the lights off on boot
  digitalWrite(RedLight, LOW);
  digitalWrite(YellowLight, LOW);
  digitalWrite(GreenLight, LOW);

  // Enable internal resistor
  digitalWrite(ModeButton, HIGH);
  digitalWrite(PowerSW, HIGH);

  CurrentMillis = millis();
}

void loop() {
  switch (State) {
    case sRed:
      fRed();
      break;

    case sYellow:
      fYellow();
      break;

    case sGreen:
      fGreen();
      break;

    case sYellowFlash:
      fYellowFlash();
      break;

    case sRedFlash:
      fRedFlash();
      break;

    case sSlowParty:
      fSlowParty();
      break;

    case sFastParty:
      fFastParty();
      break;

    case sRandom:
      fRandom();
      break;

    case sOff:
      fOff();
      break;
  }
}

//// FUNCTIONS ////

void fRed() {
  digitalWrite(YellowLight, LOW);
  digitalWrite(GreenLight, LOW);
  digitalWrite (RedLight, HIGH);

  if (millis() - CurrentMillis > 1000) {
    RedTime = RedTime - 1000;
    CurrentMillis = millis();
  }

  if (RedTime == 0) {
    RedTime = RedLightLenghth;
    State = sGreen;
  }

  fButtonPress();

  if (ButtonToggle == true) {
    RedTime = RedLightLenghth;
    ButtonToggle = false;
    State = sYellowFlash;
  }

  if (digitalRead (PowerSW) == LOW) {
    State = sOff;
  }
}

void fYellow() {
  digitalWrite(GreenLight, LOW);
  digitalWrite (RedLight, LOW);
  digitalWrite(YellowLight, HIGH);

  if (millis() - CurrentMillis > 1000) {
    YellowTime = YellowTime - 1000;
    CurrentMillis = millis();
  }

  if (YellowTime == 0) {
    YellowTime = YellowLightLength;
    State = sRed;
  }

  fButtonPress();

  if (ButtonToggle == true) {
    YellowTime = YellowLightLength;
    ButtonToggle = false;
    State = sYellowFlash;
  }

  if (digitalRead (PowerSW) == LOW) {
    State = sOff;
  }
}

void fGreen() {
  digitalWrite(YellowLight, LOW);
  digitalWrite (RedLight, LOW);
  digitalWrite(GreenLight, HIGH);

  if (millis() - CurrentMillis > 1000) {
    GreenTime = GreenTime - 1000;
    CurrentMillis = millis();
  }

  if (GreenTime == 0) {
    GreenTime = GreenLightLength;
    State = sYellow;
  }

  fButtonPress();

  if (ButtonToggle == true) {
    GreenTime = GreenLightLength;
    ButtonToggle = false;
    State = sYellowFlash;
  }

  if (digitalRead (PowerSW) == LOW) {
    State = sOff;
  }
}

void fYellowFlash() {
  digitalWrite (RedLight, LOW);
  digitalWrite(GreenLight, LOW);

  CurrentMillis = millis();

  if (CurrentMillis - PreviousMillis >= FlashInterval) {
    PreviousMillis = CurrentMillis;

    if (YellowLightState == LOW) {
      YellowLightState = HIGH;
    } else {
      YellowLightState = LOW;
    }
    digitalWrite(YellowLight, YellowLightState);
  }

  fButtonPress();

  if (ButtonToggle == true) {
    ButtonToggle = false;
    State = sRedFlash;
  }

  if (digitalRead (PowerSW) == LOW) {
    State = sOff;
  }
}

void fRedFlash() {
  digitalWrite (YellowLight, LOW);
  digitalWrite(GreenLight, LOW);

  CurrentMillis = millis();

  if (CurrentMillis - PreviousMillis >= FlashInterval) {
    PreviousMillis = CurrentMillis;

    if (RedLightState == LOW) {
      RedLightState = HIGH;
    } else {
      RedLightState = LOW;
    }
    digitalWrite(RedLight, RedLightState);
  }

  fButtonPress();

  if (ButtonToggle == true) {
    ButtonToggle = false;
    State = sSlowParty;
  }

  if (digitalRead (PowerSW) == LOW) {
    State = sOff;
  }
}

void fSlowParty() {
  unsigned long CurrentLightMillis = millis();

  if (CurrentLightMillis - PrevLightMillis > LightInt1) {
    PrevLightMillis = CurrentLightMillis;
    LightNum++;
    if (LightNum > NumLight) LightNum = 0;  // If lights have come on, start over.
    LightChanged = true;
  }

  if (LightChanged == true) {
    LightChanged = false; // Reset for next round.
    switch (LightNum)
    {
      case sRedLightFlash:
        fRedLightFlash();
        break;
      case sYellowLightFlash:
        fYellowLightFlash();
        break;
      case sGreenLightFlash:
        fGreenLightFlash();
        break;
    }
  }

  fButtonPress();

  if (ButtonToggle == true) {
    ButtonToggle = false;
    State = sFastParty;
  }

  if (digitalRead (PowerSW) == LOW) {
    State = sOff;
  }
}

void fFastParty() {
  unsigned long CurrentLightMillis = millis();

  if (CurrentLightMillis - PrevLightMillis > LightInt2) {
    PrevLightMillis = CurrentLightMillis;
    LightNum++;
    if (LightNum > NumLight) LightNum = 0;  // If lights have come on, start over
    LightChanged = true;
  }

  if (LightChanged == true) {
    LightChanged = false; // Reset for next round.
    switch (LightNum)
    {
      case sRedLightFlash:
        fRedLightFlash();
        break;
      case sYellowLightFlash:
        fYellowLightFlash();
        break;
      case sGreenLightFlash:
        fGreenLightFlash();
        break;
    }
  }

  fButtonPress();

  if (ButtonToggle == true) {
    ButtonToggle = false;
    State = sRandom;
  }

  if (digitalRead (PowerSW) == LOW) {
    State = sOff;
  }
}

void fRedLightFlash() {
  digitalWrite (GreenLight, LOW);
  digitalWrite (RedLight, HIGH);
}

void fYellowLightFlash() {
  digitalWrite (RedLight, LOW);
  digitalWrite (YellowLight, HIGH);
}

void fGreenLightFlash() {
  digitalWrite (YellowLight, LOW);
  digitalWrite (GreenLight, HIGH);
}

void fRandom() {
  for (int i = 0; i < NumberOfLights; i++) {
    if (millis() > NextFlash[i]) {
      if (LightState[i] == LOW) LightState[i] = HIGH; else LightState[i] = LOW;
      digitalWrite(LightPINs[i], LightState[i]);
      NextFlash[i] = millis() + random(50, 1500) ; // next toggle random time
    }
  }

  fButtonPress();

  if (ButtonToggle == true) {
    ButtonToggle = false;
    State = sRed;
  }

  if (digitalRead (PowerSW) == LOW) {
    State = sOff;
  }
}

void fOff() {
  digitalWrite(RedLight, LOW);
  digitalWrite(YellowLight, LOW);
  digitalWrite(GreenLight, LOW);

  if (digitalRead (PowerSW) == HIGH) {
    State = sRed;
    RedTime = RedLightLenghth;
    YellowTime = YellowLightLength;
    GreenTime = GreenLightLength;
  }
}

void fButtonPress() {
  int Reading = digitalRead(ModeButton) == LOW;

  if (Reading != LastButtonState) {
    LastDebounceTime = millis();
  }

  if ((millis() - LastDebounceTime) > DebounceDelay) {
    if (Reading != ButtonState) {
      ButtonState = Reading;
      if (ButtonState == LOW) { // Logic is inverted since we're using the internal resistors. When button is pressed the PIN goes LOW, when not pressed it's HIGH.
        ButtonToggle = true;
      }
    }
  }
  LastButtonState = Reading;
}
