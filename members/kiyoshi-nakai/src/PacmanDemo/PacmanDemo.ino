// PacmanDemo.ino - パックマンデモ組込み実習
// 作成日: 2026-05-22
// 詳細設計書に基づく実装
#include <LiquidCrystal.h>
#include <IRremote.h>

// --- ピン定義 ---
#define PIN_JOY_VRX   A0
#define PIN_JOY_VRY   A1
#define PIN_JOY_SW    2
#define PIN_IR_RECV   3
#define LCD_COLS      16
#define LCD_ROWS      2

// LCM1602A パラレル接続（4bitモード）
#define PIN_LCD_RS    7
#define PIN_LCD_EN    8
#define PIN_LCD_D4    9
#define PIN_LCD_D5    10
#define PIN_LCD_D6    11
#define PIN_LCD_D7    12

// --- グローバル変数 ---
uint8_t currentState = 0; // 0:初期化中 1:待機中 2:移動中 3:色変更中
bool isInitDone = false;
unsigned long lastInputMillis = 0;
unsigned long lastDebounceMillis = 0;
unsigned long lastIrMillis = 0;
int joyXRaw = 512;
int joyYRaw = 512;
bool joyButtonPressed = false;
uint8_t pacmanX = 0;
uint8_t pacmanY = 0;
uint8_t direction = 0;
unsigned long irCode = 0;
unsigned long lastIrStableCode = 0;
unsigned long lastIrAcceptedMillis = 0;
uint8_t colorMode = 0; // 0:通常 1:色A 2:色B
uint8_t currentCharacterIndex = 0;
uint8_t lastState = 255; // Track last state for change detection
int lastJoyX = -1, lastJoyY = -1; // Track last joystick values
uint8_t lastPacmanX = 255, lastPacmanY = 255; // Track last Pacman position
int joyXFiltered = 512;
int joyYFiltered = 512;
bool joyFilterInitialized = false;
bool joyWasActive = false;

// --- 定数 ---
const int JOY_CENTER = 512;
const int JOY_THRESHOLD = 100;
const int JOY_HYSTERESIS = 20;
const int JOY_STATE_ENTER_THRESHOLD = 130;
const int JOY_STATE_EXIT_THRESHOLD = 80;
const int JOY_DEBUG_DELTA = 25;
const int AXIS_DOMINANCE_MARGIN = 30;
const int DEBOUNCE_DELAY = 50;
const int INPUT_INTERVAL = 100;
const int IR_INTERVAL = 50;
const unsigned long IR_REPEAT_CODE = 0xFFFFFFFF;
const int IR_DUPLICATE_GUARD_MS = 120;

// --- LCD/IRオブジェクト ---
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
IRrecv irrecv(PIN_IR_RECV);
decode_results results;

// --- カスタムキャラ（パックマン各方向, ゴースト, ...） ---
// 右向き
byte pacmanOpenR[8]  = {0x0E,0x1F,0x15,0x1F,0x0E,0x00,0x00,0x00}; // 口開き
byte pacmanCloseR[8] = {0x0E,0x1F,0x1F,0x1F,0x0E,0x00,0x00,0x00}; // 口閉じ
// 左向き（右向きのビット反転）
byte pacmanOpenL[8]  = {0x0E,0x1F,0x15,0x1F,0x0E,0x00,0x00,0x00}; // 口開き
byte pacmanCloseL[8] = {0x0E,0x1F,0x1F,0x1F,0x0E,0x00,0x00,0x00}; // 口閉じ
// 上向き
byte pacmanOpenU[8]  = {0x0E,0x1B,0x17,0x0E,0x0E,0x0E,0x0E,0x00}; // 口開き
byte pacmanCloseU[8] = {0x0E,0x1F,0x1F,0x0E,0x0E,0x0E,0x0E,0x00}; // 口閉じ
// 下向き
byte pacmanOpenD[8]  = {0x00,0x0E,0x0E,0x0E,0x0E,0x17,0x1B,0x0E}; // 口開き
byte pacmanCloseD[8] = {0x00,0x0E,0x0E,0x0E,0x0E,0x1F,0x1F,0x0E}; // 口閉じ
byte ghostChar[8]    = {0x0E,0x1F,0x15,0x1F,0x1B,0x1B,0x00,0x00};
byte altChar[8]      = {0x04,0x0E,0x15,0x04,0x04,0x15,0x0E,0x04};

unsigned long lastPacmanAnimMillis = 0;
bool pacmanMouthOpen = true;

// --- IRリモコンのキャラクター切替コード例（要実機で確認） ---
#define IR_CODE_CHAR_A 0xFF30CF
#define IR_CODE_CHAR_B 0xFF18E7

void setup() {
  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  lcd.begin(LCD_COLS, LCD_ROWS);
  // 0:パックマン口開き 1:口閉じ 2:ゴースト 3:代替キャラ
  lcd.createChar(0, pacmanOpenR);   // デフォルト右向き
  lcd.createChar(1, pacmanCloseR);
  lcd.createChar(2, ghostChar);
  lcd.createChar(3, altChar);
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Init...");
  lcd.setCursor(0,1); lcd.print("LCD check");
  Serial.begin(9600); // Initialize serial communication for debugging
  Serial.println("System Initialized");
  irrecv.enableIRIn();
  pacmanX = 0; pacmanY = 0; direction = 0; colorMode = 0; currentCharacterIndex = 0;
  lastInputMillis = millis();
  lastDebounceMillis = millis();
  lastIrMillis = millis();
  lastPacmanAnimMillis = millis();
  pacmanMouthOpen = true;
  delay(1200);
  lcd.clear();
  isInitDone = true;
}

void loop() {
  readJoystick();
  readButton();
  readIR();

  // パックマンアニメーション（300msごとに口開閉）
  unsigned long now = millis();
  if (now - lastPacmanAnimMillis > 300) {
    pacmanMouthOpen = !pacmanMouthOpen;
    lastPacmanAnimMillis = now;
  }

  updateState();
  updateDisplay(currentState);

  // Output only when values change
  if (currentState != lastState) {
    Serial.print("State: ");
    Serial.println(currentState);
    lastState = currentState;
  }

  if (pacmanX != lastPacmanX || pacmanY != lastPacmanY) {
    Serial.print("Pacman Position: X=");
    Serial.print(pacmanX);
    Serial.print(", Y=");
    Serial.println(pacmanY);
    lastPacmanX = pacmanX;
    lastPacmanY = pacmanY;
  }

  bool isJoyActive = abs(joyXFiltered - JOY_CENTER) > JOY_STATE_EXIT_THRESHOLD ||
                     abs(joyYFiltered - JOY_CENTER) > JOY_STATE_EXIT_THRESHOLD;
  bool joyChangedEnough = abs(joyXFiltered - lastJoyX) >= JOY_DEBUG_DELTA ||
                          abs(joyYFiltered - lastJoyY) >= JOY_DEBUG_DELTA;

  if (isJoyActive && joyChangedEnough) {
    Serial.print("Joystick(F): X=");
    Serial.print(joyXFiltered);
    Serial.print(", Y=");
    Serial.println(joyYFiltered);
    lastJoyX = joyXFiltered;
    lastJoyY = joyYFiltered;
    joyWasActive = true;
  } else if (!isJoyActive && joyWasActive) {
    Serial.println("Joystick(F): CENTER");
    lastJoyX = joyXFiltered;
    lastJoyY = joyYFiltered;
    joyWasActive = false;
  }
}

void readJoystick() {
  int x = analogRead(PIN_JOY_VRX);
  int y = analogRead(PIN_JOY_VRY);
  joyXRaw = x;
  joyYRaw = y;

  // 4-sample equivalent low-pass filter to suppress analog jitter.
  if (!joyFilterInitialized) {
    joyXFiltered = x;
    joyYFiltered = y;
    joyFilterInitialized = true;
  } else {
    joyXFiltered = (joyXFiltered * 3 + x) / 4;
    joyYFiltered = (joyYFiltered * 3 + y) / 4;
  }
}

void readButton() {
  static bool lastButtonState = HIGH;
  bool reading = digitalRead(PIN_JOY_SW);
  if (reading != lastButtonState) {
    lastDebounceMillis = millis();
  }
  if ((millis() - lastDebounceMillis) > DEBOUNCE_DELAY) {
    if (reading == LOW && !joyButtonPressed) {
      joyButtonPressed = true;
      toggleCharacter(true);
    } else if (reading == HIGH) {
      joyButtonPressed = false;
    }
  }
  lastButtonState = reading;
}

void readIR() {
  if (millis() - lastIrMillis < IR_INTERVAL) return;
  lastIrMillis = millis();
  if (irrecv.decode(&results)) {
    unsigned long rawCode = results.value;
    irrecv.resume();

    // Normalize repeat frame to the previous stable code.
    if (rawCode == IR_REPEAT_CODE) {
      if (lastIrStableCode == 0) {
        irCode = 0;
        return;
      }
      rawCode = lastIrStableCode;
    }

    // Suppress very short duplicate bursts from a single press/hold.
    unsigned long now = millis();
    if (rawCode == lastIrStableCode && (now - lastIrAcceptedMillis) < IR_DUPLICATE_GUARD_MS) {
      irCode = 0;
      return;
    }

    irCode = rawCode;
    lastIrStableCode = rawCode;
    lastIrAcceptedMillis = now;

    Serial.print("IR Code Received: 0x");
    Serial.println(irCode, HEX);
    if (irCode == IR_CODE_CHAR_A || irCode == IR_CODE_CHAR_B) {
      toggleCharacter(true); // IRでキャラクター切替
    }
  } else {
    irCode = 0;
  }
}

void updateState() {
  if (currentState == 0) {
    if (isInitDone) currentState = 1;
  } else if (currentState == 1) {
    if (abs(joyXFiltered - JOY_CENTER) > JOY_STATE_ENTER_THRESHOLD ||
        abs(joyYFiltered - JOY_CENTER) > JOY_STATE_ENTER_THRESHOLD) {
      currentState = 2;
    }
  } else if (currentState == 2) {
    if (abs(joyXFiltered - JOY_CENTER) <= JOY_STATE_EXIT_THRESHOLD &&
        abs(joyYFiltered - JOY_CENTER) <= JOY_STATE_EXIT_THRESHOLD) {
      currentState = 1;
    } else {
      int dx = joyXFiltered - JOY_CENTER;
      int dy = joyYFiltered - JOY_CENTER;

      // Move only on dominant axis to avoid diagonal jitter-triggered movement.
      if (abs(dx) >= abs(dy) + AXIS_DOMINANCE_MARGIN) {
        updateHorizontalMove(joyXFiltered);
      } else if (abs(dy) >= abs(dx) + AXIS_DOMINANCE_MARGIN) {
        updateVerticalMove(joyYFiltered);
      }
    }
  }
}

void updateHorizontalMove(int joyX) {
  static unsigned long lastMove = 0;
  if (millis() - lastMove < INPUT_INTERVAL) return;
  if (joyX > JOY_CENTER + JOY_THRESHOLD + JOY_HYSTERESIS) {
    if (pacmanX < LCD_COLS - 1) pacmanX++;
    direction = 0;
    lastMove = millis();
  } else if (joyX < JOY_CENTER - JOY_THRESHOLD - JOY_HYSTERESIS) {
    if (pacmanX > 0) pacmanX--;
    direction = 1;
    lastMove = millis();
  }
}

void updateVerticalMove(int joyY) {
  static unsigned long lastMove = 0;
  if (millis() - lastMove < INPUT_INTERVAL) return;
  if (joyY > JOY_CENTER + JOY_THRESHOLD + JOY_HYSTERESIS) {
    if (pacmanY < LCD_ROWS - 1) pacmanY++;
    direction = 3;
    lastMove = millis();
  } else if (joyY < JOY_CENTER - JOY_THRESHOLD - JOY_HYSTERESIS) {
    if (pacmanY > 0) pacmanY--;
    direction = 2;
    lastMove = millis();
  }
}



void drawPacman(uint8_t x, uint8_t y) {
  lcd.setCursor(x, y);
  if (currentCharacterIndex == 0) {
    // 方向に応じてキャラパターンを差し替え
    // 0:右 1:左 2:上 3:下
    if (direction == 0) { // 右
      if (pacmanMouthOpen) lcd.write(byte(0));
      else lcd.write(byte(1));
    } else if (direction == 1) { // 左
      if (pacmanMouthOpen) {
        lcd.createChar(0, pacmanOpenL);
        lcd.write(byte(0));
      } else {
        lcd.createChar(1, pacmanCloseL);
        lcd.write(byte(1));
      }
    } else if (direction == 2) { // 上
      if (pacmanMouthOpen) {
        lcd.createChar(0, pacmanOpenU);
        lcd.write(byte(0));
      } else {
        lcd.createChar(1, pacmanCloseU);
        lcd.write(byte(1));
      }
    } else if (direction == 3) { // 下
      if (pacmanMouthOpen) {
        lcd.createChar(0, pacmanOpenD);
        lcd.write(byte(0));
      } else {
        lcd.createChar(1, pacmanCloseD);
        lcd.write(byte(1));
      }
    } else {
      // 万一不明な方向は右向き
      if (pacmanMouthOpen) lcd.write(byte(0));
      else lcd.write(byte(1));
    }
  } else if (currentCharacterIndex == 1) {
    lcd.write(byte(2)); // ゴースト
  } else if (currentCharacterIndex == 2) {
    lcd.write(byte(3)); // 代替キャラクター
  }
}

void updateDisplay(int state) {
  static int prevState = -1;
  static int prevX = -1;
  static int prevY = -1;

  if (state != prevState) {
    lcd.clear();
    if (state == 0) {
      lcd.setCursor(0, 0);
      lcd.print("Init...");
      lcd.setCursor(0, 1);
      lcd.print("LCD check");
    }
    prevX = -1;
    prevY = -1;
    prevState = state;
  }

  if (state == 1 || state == 2 || state == 3) {
    if (prevX >= 0 && prevY >= 0 && (prevX != pacmanX || prevY != pacmanY)) {
      lcd.setCursor(prevX, prevY);
      lcd.print(' ');
    }
    drawPacman(pacmanX, pacmanY);
    prevX = pacmanX;
    prevY = pacmanY;
  }
}

void toggleCharacter(bool pressed) {
  if (pressed) {
    currentCharacterIndex = (currentCharacterIndex + 1) % 3;
    Serial.print("Character changed: ");
    if (currentCharacterIndex == 0) Serial.println("Pacman");
    else if (currentCharacterIndex == 1) Serial.println("Ghost");
    else if (currentCharacterIndex == 2) Serial.println("AltChar");
    // 0:パックマン, 1:ゴースト, 2:ブランク
    // キャラ切替はcolorModeとは独立で管理してもよい
    // 必要ならlcd.createCharで再定義
  }
}
