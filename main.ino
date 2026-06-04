#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Keypad.h>

#define LCD_SDA 21
#define LCD_SCL 22
#define LCD_COLS 16

#define LED_RED 19
#define LED_GREEN 18

#define BUZZER 5

LiquidCrystal_I2C lcd(0x27, 16, 2);

struct SystemState {
  bool status;
  bool blockInput;
  unsigned long duration;
  String code;
};

struct Buffer {
  String row0;
  String row1;
};

struct BuzzerState {
  bool active;
  unsigned long until;
};

struct RowState {
  String lastText;
  int offset;
  unsigned long lastStep;
};

struct LedState {
  bool greenLed;
  bool redLed;
};

Buffer displayBuffer = {"", ""};
RowState rowState[2] = {{"", 0, 0}, {"", 0, 0}};

LedState ledState = {false, true};

BuzzerState buzzerState = {false, 0,};

SystemState systemState = {false, false, 0,""};

const unsigned long scrollDelay = 750;

const byte ROWS = 4;
const byte COLS = 4;

const int MAX_LEN = 8;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

bool i2CAddrTest(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

void initLcd() {
  Wire.begin(LCD_SDA, LCD_SCL);

  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }

  lcd.init();
  lcd.backlight();

  displayBuffer.row0 = "Access Security System";
  displayBuffer.row1 = "Code:";
}

void initLed(){
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
}

void initBuzzer() {
  pinMode(BUZZER, OUTPUT);
}

void writeLine(int row, String text) {
  lcd.setCursor(0, row);
  lcd.print(text);
  for (int i = text.length(); i < LCD_COLS; i++) {
    lcd.print(" ");
  }
}

void startBuzzer(unsigned long durationMs) {
  buzzerState.active = true;
  buzzerState.until = millis() + durationMs;
  digitalWrite(BUZZER, HIGH);
}

void updateBuzzer() {
  if (buzzerState.active && millis() >= buzzerState.until) {
    digitalWrite(BUZZER, LOW);
    buzzerState.active = false;
  }
}

void updateLeds() {
  digitalWrite(LED_GREEN, ledState.greenLed ? HIGH : LOW);
  digitalWrite(LED_RED, ledState.redLed ? HIGH : LOW);
}

void updateRow(int row, String text) {
  RowState &state = rowState[row];

  // Reset state if text changes
  if (state.lastText != text) {
    state.lastText = text;
    state.offset = 0;
    state.lastStep = 0;
  }

  // If the text nortmally fits then just display it 
  if (text.length() <= LCD_COLS) {
    writeLine(row, text);
    return;
  }

  // Time gate for scroll 
  unsigned long now = millis();
  if (now - state.lastStep < scrollDelay) {
    return;
  }
  state.lastStep = now;

  String padded = text + "    ";
  String window = "";

  for (int i = 0; i < LCD_COLS; i++) {
    window += padded[(state.offset + i) % padded.length()];
  }

  writeLine(row, window);
  state.offset = (state.offset + 1) % padded.length();
}

void updateDisplay() {
  updateRow(0, displayBuffer.row0);
  updateRow(1, displayBuffer.row1);
}

void submitCode() {
  systemState.blockInput = true;
  if (systemState.code == "1234") {
    ledState = {true, false};
    displayBuffer.row1 = "ACCESS GRANTED";
    startBuzzer(2000);
    systemState.duration = millis() + 2000;
  } else {
    displayBuffer.row1 = "ACCESS DENIED";
    systemState.duration = millis() + 2000;
    startBuzzer(2000);
  }
  systemState.code = "";
}

void updateCodeLine() {
  displayBuffer.row1 = "Code: " + systemState.code;
}

void updateSystem() {
  if (systemState.blockInput && millis() >= systemState.duration) {
    if (systemState.code == "") {
      updateCodeLine();
    }
    systemState.blockInput = false;
    ledState.greenLed = false;
    ledState.redLed = true;
    updateCodeLine();
  }
}

void enterCharacter(char key) {
  if (systemState.blockInput) {
    return;
  }

  startBuzzer(50);
  if (key >= '0' && key <= '9') {
    if (systemState.code.length() < MAX_LEN) {
      systemState.code += key;
      updateCodeLine();
    }
  } else if (key == '*') {
    submitCode();
  } else if (key == '#') {
    systemState.code = "";
    updateCodeLine();
  }
}

void setup() {
  Serial.begin(115200);
  initLcd();
  initLed();
  initBuzzer();
}

void loop() {
  updateDisplay();
  updateSystem();
  updateLeds();
  updateBuzzer();
  char key = keypad.getKey();

  if (key) {
    enterCharacter(key);
  }
}
