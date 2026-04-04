#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <RotaryEncoder.h>
#include <math.h>

// --- Workaround gegen Arduino-Autoprototypes ---
enum ProgType : uint8_t;  // forward declaration, damit der Typ existiert



// ==== FreeFonts ====
#define USE_FREEFONTS 1
#if USE_FREEFONTS
  #include <Fonts/FreeSans9pt7b.h>
  #include <Fonts/FreeSansBold9pt7b.h>
  #define FF_VAL    (&FreeSansBold9pt7b)
  #define FF_UNIT   (&FreeSans9pt7b)
  #define FF_STAT   (&FreeSansBold9pt7b)
  #define FF_LABEL  (&FreeSans9pt7b)
  #define YADJ_VAL   7
  #define YADJ_UNIT  7
  #define YADJ_STAT  7
  #define YADJ_LABEL 7
  #define YADJ_TOP   7
  #define YADJ_SUM   7
  #define UNIT_GAP_STROM_A_PX   4
  #define UNIT_GAP_AUTO_S_PX    4
  #define UNIT_GAP_TEMP_C_PX    3
  #define UNIT_GAP_WATT_W_PX    2
  #define UNIT_GAP_TOP_V_PX     3
  #define UNIT_GAP_SUM_V_PX     3
  #define UNIT_GAP_SETTINGS_PX  3
  #define VALUE_CELL_GAP_PX     1
#endif

// =================== Hardware / Pins ===================
#define MOSFET_PIN     PB8
#define TFT_CS         PA10
#define TFT_RST        PA9
#define TFT_DC         PA8
#define ENCODER_A      PB13
#define ENCODER_B      PB14
#define ENCODER_BTN    PB15
#define START_BTN      PB4
#define CAP1_PIN       PA0
#define CAP2_PIN       PA1
#define PULSE_PIN      PB9
#define TEMP_PIN       PA3
#define TEMP2_PIN      PA4
#define AUTO_PULSE_PIN PB12
#define BALANCE_C1_PIN PB1
#define BALANCE_C2_PIN PB5
#define FAN_PIN        PB0

// =================== Farben ===================
#define ST77XX_BLACK   ILI9341_BLACK
#define ST77XX_WHITE   ILI9341_WHITE
#define ST77XX_RED     ILI9341_RED
#define ST77XX_GREEN   ILI9341_GREEN
#define ST77XX_BLUE    ILI9341_BLUE
#define ST77XX_YELLOW  ILI9341_YELLOW
#define ST77XX_ORANGE   0xFD20
#define ST77XX_DARKGREY 0x7BEF
#define ST77XX_PURPLE   0x780F
#define ST77XX_AUTO_TEAL 0x136E
#define ST77XX_P1_GREEN 0x13C6
#define ST77XX_SETTINGS_PURPLE 0x6816
#define COLOR565(r,g,b)  ( ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b) >> 3) )
#define ST77XX_RAL5021   COLOR565(0,117,119)
#define ST77XX_RAL4003   COLOR565(214,76,129)

#define TS_SETTINGS_ALL      1
#define SETTINGS_TEXT_YSHIFT 2


// ---- Position der Summen-Spannung (Cap1+Cap2) ----
// Basis-Koordinaten (160x128), werden via SX/SY/SW/SH skaliert.
#define SUM_BOX_X   36  // x-Start in Basis-Pixeln
#define SUM_BOX_Y   12   // y-Start
#define SUM_BOX_W   40   // Breite
#define SUM_BOX_H   10   // Höhe

// =================== EEPROM Adressen ===================
#define EEPROM_ADDR_MAX 0
#define EEPROM_ADDR_MIN 4
#define EEPROM_ADDR_P1   8
#define EEPROM_ADDR_P2   12
#define EEPROM_ADDR_MODE 16
#define EEPROM_ADDR_TEMPWARN 20
#define EEPROM_ADDR_AUTOPULSE 24
#define EEPROM_ADDR_ZIELSTROM 28
#define EEPROM_ADDR_STARTBTN_DELAY 32
#define EEPROM_ADDR_RESUMEHYST 36
#define EEPROM_ADDR_ZIELLEISTUNG 40
#define EEPROM_ADDR_WELDV 44
#define EEPROM_ADDR_I_OFFSET 48
#define EEPROM_ADDR_I_GAIN   52
#define EEPROM_ADDR_WELDCOUNT 56
#define EEPROM_ADDR_PPAUSE    60

// =================== Kalibrierung ===================
#define V_CALIB_CAP1 1.0015
#define V_CALIB_CAP2 1.0015
#define RSHUNT_OHMS  0.0050f

// =================== Objekte ===================
Adafruit_INA219 ina219;
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
RotaryEncoder encoder(ENCODER_A, ENCODER_B, RotaryEncoder::LatchMode::FOUR3);

// =================== Skalierung 160x128 -> 320x240 ===================
static inline int SX(int x){return (int)roundf(x*2.0f);} 
static inline int SW(int w){return (int)roundf(w*2.0f);} 
static inline int SY(int y){return (int)roundf(y*1.875f);} 
static inline int SH(int h){return (int)roundf(h*1.875f);} 
static inline void setCursorScaled(int x, int y){ tft.setCursor(SX(x), SY(y)); }

#if USE_FREEFONTS
static inline void useFont(const GFXfont* f, uint16_t color){
  tft.setFont(f); tft.setTextSize(1); tft.setTextColor(color);
}
static inline int textWidthPx(const String& s, const GFXfont* f){
  int16_t x1,y1; uint16_t w,h; tft.setFont(f); tft.setTextSize(1);
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h); return (int)w;
}
static inline int centeredXForTextPx(const String& s, const GFXfont* f, int x_in_px, int w_in_px){
  int16_t x1,y1; uint16_t w,h;
  tft.setFont(f); tft.setTextSize(1);
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return x_in_px + ((int)w_in_px - (int)w)/2;
}
#endif

// ==== Layout-Trim für die oberen Kacheln ====
#define TOP_TILE_TRIM_SCREEN_PX  24
#define TOP_TILE_TRIM_BASE_PX    ((int)lroundf((float)TOP_TILE_TRIM_SCREEN_PX / 1.875f))
static inline int topTileValueYOffset(int h_base){ return (h_base/2) + 3; }

// =================== Settings-Layout ===================
const int SETTINGS_Y0=0, SETTINGS_SPACING=13, SETTINGS_LINE_H=13;
const int SETTINGS_VAL_X=96, SETTINGS_VAL_W=64;
const int UNIT_C_SHIFT_PX = 9;
const int UNIT_1_SHIFT_PX = -50;
const int UNIT_2_SHIFT_PX = -50;

// =================== Parameter / Zustände ===================
enum MenuState { MAIN, SETTINGS };
MenuState menuState = MAIN;

int menuIndex=0, settingIndex=0;
bool inEditMode=false;
long lastEncPos=0;
bool screenNeedsUpdate=true;
bool settingsFirstDraw=true;
int  prevSettingIndex=-1;

int   p1Time=10, p2Time=10;
int   pPauseTime=0;
float capMin=2.0, capMax=4.2;
float tempWarn=60.0;
float autoPulseDelay=0.5;
float zielStrom=2.0;
float zielLeistungW=20.0f;
float startBtnDelay=0.0;
float resumeHyst=0.10f;

// UI-Dirty-Flags für Temp-Anzeige
static float lastT1Shown = -1000.0f;
static float lastT2Shown = -1000.0f;
// Anti-Spike-Filter für NTC
static float lastT1 = NAN, lastT2 = NAN;


float tempC1=0.0f, tempC2=0.0f;

bool  ladeFreigabe=true;
float cachedV1=0.0f, cachedV2=0.0f, cachedIStrA=0.0f;
static uint32_t weldCount=0;

// ---- Fan control ----
const float FAN_ON_C  = 55.0f;
const float FAN_OFF_C = 40.0f;
bool fanState=false;

// =================== Balancer ===================
const float BAL_TOL=0.01f;
const unsigned long MIN_BAL_MS=1000;
static unsigned long balSince=0;
static int balState=0;

// =================== PI-Regler / PWM-Parametrisierung ===================
const int PWM_TOP = 4500;
const uint16_t DUTY_MIN = 0, DUTY_MAX = PWM_TOP;
float Kp_counts_per_A   = 30.0f;
float Ki_counts_per_A_s = 0.50f;

const uint16_t CONTROL_PERIOD_MS = 30;
const float    TAU_I_FILTER_S    = 0.15f;

static uint16_t dutyCounts = 0;
static float i_meas_filt = 0.0f, i_int = 0.0f;

// ===== Ramping =====
const float RAMP_THRESH_FRAC = 0.70f;
const float RAMP_FAST_A_S    = 2.0f;
const float RAMP_SLOW_A_S    = 0.5f;
const float RAMP_DOWN_A_S    = 25.0f;
static float i_ref_cmd = 0.0f;
static bool  wasCharging = false;

// ===== Duty-Slew-Rate =====
const float DUTY_FAST_PCT_S = 70.0f;
const float DUTY_SLOW_PCT_S = 25.0f;

// Separates Integrator-Limit
const float IINT_LIMIT = PWM_TOP * 0.6f;

// =================== Weld-Voltage-Target & Gates ===================
float weldVTarget = 5.00f;
static bool weldChargeGate = false;
static bool forcedDischarge = false;

// Hysterese / Toleranzen
const float WELD_GATE_HYST_V  = 0.010f;
const float WELD_DISCH_EPS_V  = 0.010f;

// Zell-Overvoltage
const float OV_EPS_V = 0.005f;

// =================== Strommessung Offset & Gain ===================
float I_OFFSET_mV = 0.8f;
float I_GAIN      = 0.820f;

// =================== Puls-schonendes Laden ===================
#define TRIG_PB4_ACTIVE_LOW     1
#define AUTO_PB12_ACTIVE_HIGH   1
#define USE_TEST_SECONDS 0

float     CHARGE_THROTTLE_PCT    = 0.99f;
const     uint16_t THROTTLE_STEP_COUNTS = 1000;
const     uint16_t RESTORE_STEP_COUNTS  = 1000;
const     uint16_t STEP_DELAY_MS        = 0;

const     uint16_t THROTTLE_DWELL_PRE_MS  = 5;
const     uint16_t THROTTLE_DWELL_POST_MS = 5;

uint16_t  PULSE_TEST_MS = 5;

static bool weldHold = false;

// ======= Auto-Pulse One-Shot =======
static bool          autoInputActive   = false;
static bool          autoLatched       = false;
static bool          autoShotScheduled = false;
static unsigned long autoShotTimeMs    = 0;

// ======= Hand-Trigger One-Shot =======
static bool          pb4InputActive    = false;
static bool          pb4Latched        = false;
static bool          pb4ShotScheduled  = false;
static unsigned long pb4ShotTimeMs     = 0;

// =================== Prototypen ===================

void drawMenu();
void drawSettingsMenu();
void handleEncoder();
void showWeldingStatus(const String& msg, uint16_t color);
float readVoltage(int pin, float R1, float R2, float calib);
float readVoltageAveraged(int pin, float R1, float R2, float calib);
float readChargeCurrentRaw();
void drawWeldCount();
void doWeldSequence();
void drawTileFrame(int x,int y,int w,int h,bool selected,uint16_t color);
void drawTileP1();
void drawTileP2();
void drawTilePause();
void drawTileAuto();
void drawTileStrom();
void drawTileTemp();
void drawTileSettings();
void drawTileWeldV();
void drawSettingsLine(int index, const String& label, float value, bool selected);
void drawSettingsValue(int index, float value, bool selected);
void updateFanControl();
static void redrawTileByIndex(int idx);
static inline void clampWeldTargetToRange();
static int  seqFindPos(uint8_t idx);
static void seqMoveBy(int steps);
void calibrateZeroCurrentOffset();
static inline bool canPulseNow(const char** reason = nullptr);
void updateStatusAuto();  // <— NEU


// ==== Stoppuhr ====
void drawTileStopwatch();
void updateStopwatch();
String fmtStopwatch(uint32_t s);

// ====== Helpers ======
String fmtFloat(float v, uint8_t decimals){
  char b[24]; dtostrf(v,0,decimals,b); return String(b);
}
static inline void clampWeldTargetToRange(){
  float lo = 2.0f * capMin;
  float hi = 2.0f * capMax;
  if (weldVTarget < lo) weldVTarget = lo;
  if (weldVTarget > hi) weldVTarget = hi;
}
static inline uint16_t umin16(uint16_t a, uint16_t b){ return (a<b)?a:b; }

// === Festes Format „00.0“ ===
String fmt00_0(float v){
  if (v < 0) v = 0;
  if (v > 99.95f) v = 99.95f;
  int iv = (int)floorf(v);
  int tenth = (int)floorf((v - iv) * 10.0f + 0.5f);
  if (tenth >= 10) { tenth -= 10; iv += 1; if (iv > 99) { iv = 99; tenth = 9; } }
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d.%01d", iv, tenth);
  return String(buf);
}

static inline int centeredXForValueUnit(const String& val,const char* unit,int x_px,int w_px,int gap_px,const GFXfont* f_val,const GFXfont* f_unit){
  int ww = textWidthPx(val, f_val);
  int wu = unit && *unit ? textWidthPx(String(unit), f_unit) : 0;
  int total = ww + (unit && *unit ? gap_px : 0) + wu;
  return x_px + (w_px - total)/2;
}

static void rampDutyDownTo(uint16_t target){
  while(dutyCounts > target){
    uint16_t step = umin16(THROTTLE_STEP_COUNTS, (uint16_t)(dutyCounts - target));
    dutyCounts -= step;
    Timer4.setCompare(3, dutyCounts);
    delay(0);
  }
}
static void rampDutyUpTo(uint16_t target){
  while(dutyCounts < target){
    uint16_t step = umin16(RESTORE_STEP_COUNTS, (uint16_t)(target - dutyCounts));
    dutyCounts += step;
    Timer4.setCompare(3, dutyCounts);
    delay(0);
  }
}

// -------- Stoppuhr-Zustände --------
static bool     swRunning = false;
static bool     swAutoPaused = false;
static uint32_t swElapsedMs = 0;
static uint32_t swLastTickMs = 0;
static uint32_t swLastDrawnSec = 0;

// --- Stopwatch-Draw-Cache ---
static uint16_t swLastBg  = 0;
static bool     swLastSel = false;

static int16_t  swTxtX=0, swTxtY=0;
static uint16_t swTxtW=0, swTxtH=0;

static String   swPrevStr = "";

// Zellen-Layout für "HH:MM:SS"
static uint16_t swCellW[8];
static int16_t  swCellX[8];
static int16_t  swBaselineY=0;

static uint16_t swMaxDigitW=0;
static uint16_t swColonW=0;
static uint8_t  swLayoutValid=0;
static bool     swForceFullRedraw = true;

// -------- Encoder-Linearfolge --------
static const uint8_t MENU_SEQ[9] = {7,0,8,1,2,3,4,6,5};
static int seqFindPos(uint8_t idx){ for(int i=0;i<9;i++) if(MENU_SEQ[i]==idx) return i; return 0; }
static void seqMoveBy(int steps){
  int curPos = seqFindPos(menuIndex);
  int newPos = curPos + steps;
  if(newPos < 0) newPos = 0;
  if(newPos > 8) newPos = 8;
  int previousIndex = menuIndex;
  menuIndex = MENU_SEQ[newPos];
  if(previousIndex != menuIndex){ redrawTileByIndex(previousIndex); redrawTileByIndex(menuIndex); }
}

// ----------- Akku-Rahmen/Gauge -----------
static inline void drawTopBatteryFrame(int x,int y,int w,int h,uint16_t color){
  const int bx = SX(x),  by = SY(y);
  const int bw = SW(w),  bh = SH(h);
  const int b  = 2;
  tft.fillRect(bx,      by,      bw, b,  color);
  tft.fillRect(bx,      by+bh-b, bw, b,  color);
  tft.fillRect(bx,      by,      b,  bh, color);
  tft.fillRect(bx+bw-b, by,      b,  bh, color);
}
void drawTopBatteryGauge(int x,int y,int w,int h,float volt){
  const int b = 2;
  float pct = (volt - capMin) / max(0.001f, (capMax - capMin));
  pct = constrain(isnan(pct)?0.0f:pct, 0.0f, 1.0f);
  const int ix = SX(x) + b;
  const int iy = SY(y) + b;
  const int iw = SW(w) - 2*b;
  const int ih = SH(h) - 2*b;
  tft.fillRect(ix, iy, iw, ih, ST77XX_BLACK);
  uint16_t col = (pct<0.10f)?ST77XX_RED:((pct<0.30f)?ST77XX_ORANGE:ST77XX_GREEN);
  int fw=(int)roundf(iw*pct);
  if(fw>0){ int fx=ix+(iw-fw); tft.fillRect(fx, iy, fw, ih, col); }
}

// =================== EEPROM ===================
#define EEPROMFloatReadBlk(addr,var,minv,maxv,defv) do{ var=EEPROMFloatRead(addr); if(isnan(var)||var<(minv)||var>(maxv)){var=(defv); EEPROMFloatWrite(addr,var);} }while(0)
float EEPROMFloatRead(int a){ float v; byte* p=(byte*)&v; for(int i=0;i<sizeof(float);i++) p[i]=EEPROM.read(a+i); return v;}
void  EEPROMFloatWrite(int a,float v){ byte* p=(byte*)&v; for(int i=0;i<sizeof(float);i++) EEPROM.write(a+i,p[i]); }
uint32_t EEPROMUInt32Read(int a){ uint32_t v; byte* p=(byte*)&v; for(int i=0;i<sizeof(uint32_t);i++) p[i]=EEPROM.read(a+i); return v; }
void     EEPROMUInt32Write(int a,uint32_t v){ byte* p=(byte*)&v; for(int i=0;i<sizeof(uint32_t);i++) EEPROM.write(a+i,p[i]); }


// =================== NEU: Startbildschirm ===================
static void showStartupScreen(){
  // Vollbild schwarz
  tft.fillScreen(ST77XX_BLACK);

  // Display-Abmessungen (Rotation 3 -> 320x240)
  const int scrW = tft.width();   // 320
  const int scrH = tft.height();  // 240

  // Ladebalken: 80% Breite, 16px Höhe, 2px weiße Außenwand, innen blau
  const int barW = (int)lround(0.80 * scrW); // 192 px
  const int barH = 20;
  const int border = 2;

  // Position: horizontal zentriert, leicht oberhalb der Mitte
  const int barX = (scrW - barW)/2;
  const int barY = (scrH - barH)/2 - 20;

  // Rahmen (2 px) zeichnen
  tft.drawRect(barX, barY, barW, barH, ST77XX_WHITE);
  tft.drawRect(barX+1, barY+1, barW-2, barH-2, ST77XX_WHITE);

  // Innenbereich zuerst schwarz
  const int innerX = barX + border;
  const int innerY = barY + border;
  const int innerW = barW - 2*border;
  const int innerH = barH - 2*border;
  tft.fillRect(innerX, innerY, innerW, innerH, ST77XX_BLACK);

  // Text unter dem Balken
  const String line = "SPOT-WELDER-60W10A.V1.0";
  useFont(FF_STAT, ST77XX_WHITE);
  int tx = centeredXForTextPx(line, FF_STAT, 0, scrW);
  int16_t bx, by; uint16_t bw, bh;
  tft.getTextBounds(line, 0, 0, &bx, &by, &bw, &bh);
  int textY = barY + barH + 20; // 80px unter dem Balken
  tft.setCursor(tx, textY);
  tft.print(line);
  tft.setFont(NULL);

  // Animation ~3s: von links nach rechts füllen
  const unsigned long totalMs = 3000;
  const int steps = innerW;                       // Pixelweise füllen
  const unsigned long stepDelay = totalMs / max(1, steps);

  for (int w = 1; w <= innerW; ++w){
    tft.fillRect(innerX + (w-1), innerY, 1, innerH, ST77XX_BLUE);
    delay(stepDelay);
  }

  delay(150);
}

// =================== Setup ===================
bool lastStartBtnState=HIGH, currentStartBtnState=HIGH;

void setup(){
  #ifdef ARDUINO_ARCH_STM32
    analogReadResolution(12);
  #endif

  EEPROMFloatReadBlk(EEPROM_ADDR_MAX,capMax,2.0f,4.2f,4.2f);
  EEPROMFloatReadBlk(EEPROM_ADDR_MIN,capMin,2.0f,4.2f,2.0f);
  EEPROMFloatReadBlk(EEPROM_ADDR_TEMPWARN,tempWarn,0.0f,100.0f,60.0f);
  EEPROMFloatReadBlk(EEPROM_ADDR_AUTOPULSE,autoPulseDelay,0.0f,2.0f,0.5f);
  EEPROMFloatReadBlk(EEPROM_ADDR_ZIELSTROM,zielStrom,0.1f,10.0f,2.0f);
  EEPROMFloatReadBlk(EEPROM_ADDR_STARTBTN_DELAY,startBtnDelay,0.0f,2.0f,0.0f);
  EEPROMFloatReadBlk(EEPROM_ADDR_RESUMEHYST,resumeHyst,0.01f,0.10f,0.10f);
  EEPROMFloatReadBlk(EEPROM_ADDR_ZIELLEISTUNG,zielLeistungW,1.0f,60.0f,20.0f);

  weldVTarget = EEPROMFloatRead(EEPROM_ADDR_WELDV);
  if(isnan(weldVTarget) || weldVTarget<0.0f || weldVTarget>10.0f) weldVTarget = 2.0f*capMax;
  clampWeldTargetToRange();

  EEPROMFloatReadBlk(EEPROM_ADDR_I_OFFSET, I_OFFSET_mV, -5.0f, +5.0f, 0.0f);
  EEPROMFloatReadBlk(EEPROM_ADDR_I_GAIN,   I_GAIN,       0.90f, 1.10f, 1.000f);

  p1Time=EEPROM.read(EEPROM_ADDR_P1); if(p1Time<1||p1Time>20) p1Time=10;
  p2Time=EEPROM.read(EEPROM_ADDR_P2); if(p2Time<0||p2Time>20) p2Time=0;

  int pp = EEPROM.read(EEPROM_ADDR_PPAUSE);
  if(pp<0 || pp>50) pp=0;
  pPauseTime = pp;

  uint32_t wc = EEPROMUInt32Read(EEPROM_ADDR_WELDCOUNT);
  if (wc == 0xFFFFFFFFu || wc > 100000000u) wc = 0u;
  weldCount = wc;

  pinMode(MOSFET_PIN, PWM); Timer4.setPrescaleFactor(1);
  Timer4.setOverflow(PWM_TOP); Timer4.setCompare(3,0);

  pinMode(ENCODER_BTN, INPUT_PULLUP);
  pinMode(START_BTN, INPUT_PULLUP);
  pinMode(AUTO_PULSE_PIN, INPUT);

  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);

  pinMode(BALANCE_C1_PIN, OUTPUT); pinMode(BALANCE_C2_PIN, OUTPUT);
  digitalWrite(BALANCE_C1_PIN, LOW); digitalWrite(BALANCE_C2_PIN, LOW);
  pinMode(FAN_PIN, OUTPUT); digitalWrite(FAN_PIN, LOW);

  tft.begin(); tft.setRotation(3); tft.setTextWrap(false);
  tft.fillScreen(ILI9341_BLACK);

  // Startbildschirm
  showStartupScreen();

  ina219.begin();
  ina219.setCalibration_32V_2A();

  encoder.setPosition(0);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), []{ encoder.tick(); }, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), []{ encoder.tick(); }, CHANGE);

  drawMenu();
}

// =================== Variablen für Kopfzeile ===================
static int lastNum1_rx = -1, lastNum1_ry = 0, lastNum1_rw = 0, lastNum1_rh = 0;
static int lastNum2_rx = -1, lastNum2_ry = 0, lastNum2_rw = 0, lastNum2_rh = 0;

// =================== Anti-Flacker Layouts STROM & TEMP ===================
static bool   stLayoutValid=false, stForceFullRedraw=true;
static bool   stLastSel=false;
static uint16_t stTileBgColor=ST77XX_PURPLE;
static int16_t stX=0, stY=0; static uint16_t stW=0, stH=0;
static uint16_t stCellWDigit=0, stCellWDot=0, stCellH=0;
static int16_t stCellX[2][4]; static int16_t stUnitX_A=0, stUnitX_W=0;
static int16_t stUnitBaseY1=0, stUnitBaseY2=0;
static String stPrevA="", stPrevW="";
static int16_t stAsc=0, stDesc=0;

static bool   tpLayoutValid=false, tpForceFullRedraw=true;
static bool   tpLastSel=false;
static uint16_t tpTileBgColor=ST77XX_RED;
static int16_t tpX=0, tpY=0; static uint16_t tpW=0, tpH=0;
static uint16_t tpCellWDigit=0, tpCellWDot=0, tpCellH=0;
static int16_t tpCellX[2][4]; static int16_t tpUnitX_C1=0, tpUnitX_C2=0;
static int16_t tpUnitBaseY1=0, tpUnitBaseY2=0;
static String tpPrevT1="", tpPrevT2="";
static int16_t tpAsc=0, tpDesc=0;

// =================== Puls-Freigabe ===================
static inline bool canPulseNow(const char** reason) {
  const float v1 = cachedV1, v2 = cachedV2;
  if (max(tempC1, tempC2) > tempWarn) { if (reason) *reason = "TEMP"; return false; }
  if (v1 < capMin || v2 < capMin)      { if (reason) *reason = "UV";   return false; }
  if (v1 > capMax + OV_EPS_V || v2 > capMax + OV_EPS_V) { if (reason) *reason = "OV"; return false; }
  return true;
}


// =================== NEU: Entlade-Animation ===================
static bool dischargeAnimActive = false;
static float dischargeAnimPct = 1.0f;              // 1.0 → voll, 0.0 → leer
static unsigned long lastDischargeAnimUpdate = 0;
static int animPrevFW = -1;                        // vorherige Füllbreite (Pixel) für differenzielles Übermalen

// ======= Fortschritts-Bar unten (Hand/Auto) =======
// Position/Größe (Basis-Koords 160x128 → via SX/SY/SW/SH skaliert)
const int PROG_X = 54;
const int PROG_Y = 26;     // ganz unten
const int PROG_W = 120;     // volle Breite
const int PROG_H = 15;       // Höhe

// 1) Enum MUSS vor jeder Verwendung kommen!
enum ProgType : uint8_t { PROG_NONE, PROG_HAND, PROG_AUTO };

// 2) State
static ProgType     progType   = PROG_NONE;
static unsigned long progStartMs = 0;
static unsigned long progDurMs   = 0;
static int           progPrevW   = -1;   // letzte Füllbreite für differenzielles Zeichnen

// 3) Helpers + Zeichenlogik
static inline uint16_t progressColorFor(ProgType t){
  return (t == PROG_HAND) ? ST77XX_GREEN
       : (t == PROG_AUTO) ? ST77XX_GREEN
                          : ST77XX_AUTO_TEAL;
}
static void progressClearArea(){
  tft.fillRect(SX(PROG_X), SY(PROG_Y), SW(PROG_W), SH(PROG_H), ST77XX_AUTO_TEAL);
}
static void startProgress(ProgType t, unsigned long dur_ms){
  if (dur_ms == 0) { progType = PROG_NONE; return; }
  progType    = t;
  progStartMs = millis();
  progDurMs   = dur_ms;
  progPrevW   = -1;
  progressClearArea();
}
static void cancelProgress(){
  if (progType != PROG_NONE){
    progressClearArea();
    progType = PROG_NONE;
    progPrevW = -1;
  }
}
static void updateProgress(){
  if (progType == PROG_NONE) return;

  unsigned long now  = millis();
  float frac = (progDurMs > 0) ? (float)(now - progStartMs) / (float)progDurMs : 1.0f;
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;

  int fullW = SW(PROG_W);
  int newW  = (int)lroundf(frac * fullW);

  int x = SX(PROG_X);
  int y = SY(PROG_Y);
  int h = SH(PROG_H);
  uint16_t col = progressColorFor(progType);

  if (progPrevW < 0){
    if (newW > 0) tft.fillRect(x, y, newW, h, col);
  } else if (newW != progPrevW){
    if (newW > progPrevW){
      tft.fillRect(x + progPrevW, y, newW - progPrevW, h, col);
    } else {
      tft.fillRect(x + newW, y, progPrevW - newW, h, ST77XX_BLACK);
    }
  }
  progPrevW = newW;
}


// =================== loop ===================
void loop(){
  handleEncoder();

  // ======= Temperatur (1 s) =======
  static unsigned long lastTempUpdate=0;
  if (millis() - lastTempUpdate >= 1000) {

    // (2) Median-von-5: robust gegen einzelne ADC-Ausreißer
    auto readNTC_med5 = [](int pin)->float {
      float v[5];
      for (int i=0;i<5;i++){
        int raw = analogRead(pin); raw = constrain(raw, 1, 4094);
        float R = (float)raw * 10000.0f / (4095.0f - (float)raw); // 10k Festwiderstand
        v[i] = 1.0f/(log(R/10000.0f)/3950.0f + 1.0f/298.15f) - 273.15f;
        delayMicroseconds(200);
      }
      // insertion-sort für 5 Werte → Median auf v[2]
      for (int i=1;i<5;i++){ float t=v[i]; int j=i-1; while (j>=0 && v[j]>t){ v[j+1]=v[j]; j--; } v[j+1]=t; }
      return v[2];
    };

    float newT1 = readNTC_med5(TEMP_PIN);
    float newT2 = readNTC_med5(TEMP2_PIN);
    lastTempUpdate = millis();

    // (3) Anti-Spike: große Einzelsprünge weich abfangen
    const float MAX_STEP = 1.5f; // °C/sek erlaubt
    if (!isnan(lastT1) && fabsf(newT1 - lastT1) > MAX_STEP) newT1 = 0.7f*lastT1 + 0.3f*newT1;
    if (!isnan(lastT2) && fabsf(newT2 - lastT2) > MAX_STEP) newT2 = 0.7f*lastT2 + 0.3f*newT2;
    lastT1 = newT1; lastT2 = newT2;

    // Sanfte IIR-Glättung
    static bool tInit=false;
    if (!tInit) { tempC1=newT1; tempC2=newT2; tInit=true; }
    else { tempC1 = 0.85f*tempC1 + 0.15f*newT1; tempC2 = 0.85f*tempC2 + 0.15f*newT2; }

    updateFanControl();

    // Temp-Kachel nur bei Änderung
    if (menuState == MAIN) {
      bool t1Changed = fabsf(tempC1 - lastT1Shown) >= 0.1f;
      bool t2Changed = fabsf(tempC2 - lastT2Shown) >= 0.1f;
      if (t1Changed || t2Changed) {
        drawTileTemp();
        lastT1Shown = tempC1;
        lastT2Shown = tempC2;
      }
    }

    // Status aktualisieren
    updateStatusAuto();
  }

  // ======= Messungen 10ms =======
  static unsigned long lastSample=0;
  if(millis()-lastSample>=10){
    float v1=readVoltageAveraged(CAP1_PIN,20000.0,10000.0,V_CALIB_CAP1);
    float vTot=readVoltageAveraged(CAP2_PIN,20000.0,10000.0,V_CALIB_CAP2);
    float v2=vTot-v1; cachedV1=v1; cachedV2=v2;

    float i_raw=readChargeCurrentRaw();
    static float dispFilt=0.0f; dispFilt=0.85f*dispFilt+0.15f*i_raw;
    cachedIStrA=dispFilt; lastSample=millis();
  }

  // ======= Weld-Ziel Gate =======
  {
    float vSumNow = cachedV1 + cachedV2;
    if(vSumNow > weldVTarget + WELD_GATE_HYST_V) weldChargeGate = true;
    else if(vSumNow < weldVTarget - WELD_GATE_HYST_V) weldChargeGate = false;
  }

  // ======= Kopfzeile/Spannungen 1s =======
  static unsigned long lastDisplayUpdate = 0;
  static int lastV1_mV = -100000, lastV2_mV = -100000, lastVSum_mV = -100000;
  static int lastFW1 = -1, lastFW2 = -1;
  static uint16_t lastCol1 = 0, lastCol2 = 0;

  if (menuState == MAIN && millis() - lastDisplayUpdate >= 1000) {
    const int b = 2;
    const int cap_w = 47, cap_h = 11;
    const int cap1_ix = SX(6)  + b, cap1_iy = SY(0) + b;
    const int cap2_ix = SX(64) + b, cap2_iy = SY(0) + b;
    const int cap_iw  = SW(cap_w) - 2*b;
    const int cap_ih  = SH(cap_h) - 2*b;
    const int txt_y  = SY(2 + YADJ_TOP) - 1;

    float v1 = cachedV1, v2 = cachedV2;
    int v1_mV = (int)lroundf(v1 * 1000.0f);
    int v2_mV = (int)lroundf(v2 * 1000.0f);
    int vSum_mV = v1_mV + v2_mV;

    auto calcColor = [](float pct)->uint16_t {
      return (pct < 0.10f) ? ST77XX_RED : ((pct < 0.30f) ? ST77XX_ORANGE : ST77XX_GREEN);
    };

    auto drawDeltaFill = [&](int ix, int iy, int iw, int ih, float volt,
                             int &lastFW, uint16_t &lastCol) {
      float pct = (volt - capMin) / max(0.001f, (capMax - capMin));
      pct = constrain(isnan(pct)?0.0f:pct, 0.0f, 1.0f);
      uint16_t col = calcColor(pct);
      int fw = (int)roundf(iw * pct);
      int xFilled = ix + (iw - fw);

      if (lastFW < 0) {
        tft.fillRect(ix, iy, iw, ih, ST77XX_BLACK);
        if (fw > 0) tft.fillRect(xFilled, iy, fw, ih, col);
      } else {
        if (col != lastCol) { if (fw > 0) tft.fillRect(xFilled, iy, fw, ih, col); }
        if (fw > lastFW) {
          int add = fw - lastFW; tft.fillRect(ix + (iw - fw), iy, add, ih, col);
        } else if (fw < lastFW) {
          int sub = lastFW - fw; tft.fillRect(ix + (iw - lastFW), iy, sub, ih, ST77XX_BLACK);
        }
      }
      lastFW  = fw; lastCol = col;
    };

    auto repaintGaugeUnderRect = [&](int ix, int iy, int iw, int ih, int fw, uint16_t col,
                                     int rx, int ry, int rw, int rh) {
      int xFilled = ix + (iw - fw);
      int ux = ix, uw = iw - fw;
      int cx = max(rx, ux), cw = min(rx+rw, ux+uw) - cx;
      if (cw > 0) tft.fillRect(cx, ry, cw, rh, ST77XX_BLACK);
      int fx = max(rx, xFilled), fw2 = min(rx+rw, ix+iw) - fx;
      if (fw2 > 0) tft.fillRect(fx, ry, fw2, rh, col);
    };

    // Während der Entlade-Animation Zellanzeigen NICHT aktualisieren
    if (!dischargeAnimActive) {
      if (v1_mV != lastV1_mV) {
        drawDeltaFill(cap1_ix, cap1_iy, cap_iw, cap_ih, v1, lastFW1, lastCol1);
        String vs = fmtFloat(v1, 3) + "V";
        int num_x = centeredXForTextPx(vs, FF_VAL, cap1_ix, cap_iw);

        auto restoreRectClipped1 = [&](int rx,int ry,int rw,int rh){
          int clipX = max(rx, cap1_ix), clipW = min(rx+rw, cap1_ix+cap_iw) - clipX;
          int clipY = max(ry, cap1_iy), clipH = min(ry+rh, cap1_iy+cap_ih) - clipY;
          if (clipW > 0 && clipH > 0) {
            repaintGaugeUnderRect(cap1_ix, cap1_iy, cap_iw, cap_ih, lastFW1, lastCol1, clipX, clipY, clipW, clipH);
          }
        };
        if (lastNum1_rx >= 0) restoreRectClipped1(lastNum1_rx, lastNum1_ry, lastNum1_rw, lastNum1_rh);

        int16_t nx1, ny1; uint16_t nw, nh;
        useFont(FF_VAL, ST77XX_WHITE);
        tft.getTextBounds(vs, num_x, txt_y, &nx1, &ny1, &nw, &nh);
        restoreRectClipped1(nx1, ny1, nw, nh);

        tft.setCursor(num_x, txt_y); tft.print(vs); tft.setFont(NULL);
        lastNum1_rx = nx1; lastNum1_ry = ny1; lastNum1_rw = nw; lastNum1_rh = nh;
        lastV1_mV = v1_mV;
      }

      if (v2_mV != lastV2_mV) {
        drawDeltaFill(cap2_ix, cap2_iy, cap_iw, cap_ih, v2, lastFW2, lastCol2);
        String vs = fmtFloat(v2, 3) + "V";
        int num_x = centeredXForTextPx(vs, FF_VAL, cap2_ix, cap_iw);

        auto restoreRectClipped2 = [&](int rx,int ry,int rw,int rh){
          int clipX = max(rx, cap2_ix), clipW = min(rx+rw, cap2_ix+cap_iw) - clipX;
          int clipY = max(ry, cap2_iy), clipH = min(ry+rh, cap2_iy+cap_ih) - clipY;
          if (clipW > 0 && clipH > 0) {
            repaintGaugeUnderRect(cap2_ix, cap2_iy, cap_iw, cap_ih, lastFW2, lastCol2, clipX, clipY, clipW, clipH);
          }
        };
        if (lastNum2_rx >= 0) restoreRectClipped2(lastNum2_rx, lastNum2_ry, lastNum2_rw, lastNum2_rh);

        int16_t nx1, ny1; uint16_t nw, nh;
        useFont(FF_VAL, ST77XX_WHITE);
        tft.getTextBounds(vs, num_x, txt_y, &nx1, &ny1, &nw, &nh);
        restoreRectClipped2(nx1, ny1, nw, nh);

        tft.setCursor(num_x, txt_y); tft.print(vs); tft.setFont(NULL);
        lastNum2_rx = nx1; lastNum2_ry = ny1; lastNum2_rw = nw; lastNum2_rh = nh;
        lastV2_mV = v2_mV;
      }
    }

    if (vSum_mV != lastVSum_mV) {
      drawSumVoltageBox(vSum_mV / 1000.0f);
      lastVSum_mV = vSum_mV;
    }

    // Status aktualisieren
    updateStatusAuto();

    lastDisplayUpdate = millis();
  }

  // ======= Strom-Kachel 500ms =======
  static unsigned long lastCurrentKachelUpdate=0;
  static float lastA = -1.0f;
  static float lastW = -1.0f;
  if (millis() - lastCurrentKachelUpdate >= 500) {
    float A = cachedIStrA;
    float W = (cachedV1 + cachedV2) * A;
    if (menuState == MAIN && (fabs(A - lastA) >= 0.05f || fabs(W - lastW) >= 0.1f)) {
      drawTileStrom();
      lastA = A; lastW = W;
    }
    lastCurrentKachelUpdate = millis();
  }

  // ======= Stoppuhr =======
  updateStopwatch();

  // ======= Ladefreigabe/Hysterese =======
  {
    float vMax=max(cachedV1,cachedV2);
    if (ladeFreigabe && vMax >= capMax + OV_EPS_V) {
      ladeFreigabe = false; dutyCounts = 0; i_int = 0.0f; Timer4.setCompare(3, 0);
    }
    if(!ladeFreigabe && vMax<=capMax-resumeHyst){ ladeFreigabe=true; }
  }

  // ======= Balancing / Entladung =======
  {
    float vSum = cachedV1 + cachedV2;

    if (vSum > weldVTarget + WELD_DISCH_EPS_V) {
      if (!forcedDischarge) { forcedDischarge = true; balSince = millis(); }
    } else if (vSum <= weldVTarget) {
      forcedDischarge = false;
    }

    if (dutyCounts <= 5) {
      if (forcedDischarge) {
        // Entladen über beide Balancer
        digitalWrite(BALANCE_C1_PIN, HIGH);
        digitalWrite(BALANCE_C2_PIN, HIGH);
        if (balState != 2) {
          balState = 2;
          updateStatusAuto();
        }
      } else {
        float diff=cachedV1-cachedV2;
        int desired=(diff>BAL_TOL)?+1:(diff<-BAL_TOL)?-1:0;
        if(desired!=balState && millis()-balSince>=MIN_BAL_MS){
          balState=desired; balSince=millis();
          digitalWrite(BALANCE_C1_PIN, balState==+1?HIGH:LOW);
          digitalWrite(BALANCE_C2_PIN, balState==-1?HIGH:LOW);
        }
      }
    } else {
      if(balState!=0){
        balState=0; balSince=millis();
        digitalWrite(BALANCE_C1_PIN, LOW); digitalWrite(BALANCE_C2_PIN, LOW);
      }
    }
  }

  // ======= Entlade-Animation + Status „ENTLADEN“ =======
  if (menuState == MAIN) {
    const bool animCondition = (forcedDischarge && dutyCounts <= 5);
    const int b = 2;
    const int cap_w = 47, cap_h = 11;
    const int iw = SW(cap_w) - 2*b;
    const int ih = SH(cap_h) - 2*b;
    const int ix1 = SX(6) + b,  iy1 = SY(0) + b;
    const int ix2 = SX(64) + b, iy2 = SY(0) + b;

    if (animCondition) {
      if (!dischargeAnimActive) {
        dischargeAnimActive = true;
        dischargeAnimPct = 1.0f;
        lastDischargeAnimUpdate = millis();
        animPrevFW = iw;

        tft.fillRect(ix1, iy1, iw, ih, ST77XX_GREEN);
        tft.fillRect(ix2, iy2, iw, ih, ST77XX_GREEN);

        showWeldingStatus("ENTLADEN", ST77XX_ORANGE);
      }

      unsigned long now = millis();
      if (now - lastDischargeAnimUpdate >= 100) {
        lastDischargeAnimUpdate = now;

        dischargeAnimPct -= 0.05f;
        if (dischargeAnimPct <= 0.0f) {
          dischargeAnimPct = 1.0f;
          tft.fillRect(ix1, iy1, iw, ih, ST77XX_GREEN);
          tft.fillRect(ix2, iy2, iw, ih, ST77XX_GREEN);
          animPrevFW = iw;
        } else {
          int newFW = (int)roundf(iw * dischargeAnimPct);
          int oldFW = animPrevFW; if(oldFW < 0) oldFW = iw;
          if (newFW < oldFW) {
            int oldXFilled = ix1 + (iw - oldFW);
            int newXFilled = ix1 + (iw - newFW);
            int delta = newXFilled - oldXFilled;
            if (delta > 0) {
              tft.fillRect(oldXFilled, iy1, delta, ih, ST77XX_BLACK);
              tft.fillRect((oldXFilled - ix1) + ix2, iy2, delta, ih, ST77XX_BLACK);
            }
            animPrevFW = newFW;
          }
        }
      }
    } else {
      if (dischargeAnimActive) {
        dischargeAnimActive = false;

        drawTopBatteryGauge(6, 0, 47, 11, cachedV1);
        drawTopBatteryGauge(64, 0, 47, 11, cachedV2);

        const int txt_y  = SY(2 + YADJ_TOP) - 1;
        String v1s = fmtFloat(cachedV1, 3) + "V";
        String v2s = fmtFloat(cachedV2, 3) + "V";
        int cap1_ix = SX(6) + b;
        int cap2_ix = SX(64) + b;
        int cap_iw  = SW(cap_w) - 2*b;

        int num1_x = centeredXForTextPx(v1s, FF_VAL, cap1_ix, cap_iw);
        int num2_x = centeredXForTextPx(v2s, FF_VAL, cap2_ix, cap_iw);
        useFont(FF_VAL, ST77XX_WHITE);
        tft.setCursor(num1_x, txt_y); tft.print(v1s);
        tft.setCursor(num2_x, txt_y); tft.print(v2s);
        tft.setFont(NULL);

        int16_t nx1, ny1, nx2, ny2; uint16_t nw1, nh1, nw2, nh2;
        useFont(FF_VAL, ST77XX_WHITE);
        tft.getTextBounds(v1s, num1_x, txt_y, &nx1, &ny1, &nw1, &nh1);
        tft.getTextBounds(v2s, num2_x, txt_y, &nx2, &ny2, &nw2, &nh2);
        tft.setFont(NULL);
        lastNum1_rx = nx1; lastNum1_ry = ny1; lastNum1_rw = nw1; lastNum1_rh = nh1;
        lastNum2_rx = nx2; lastNum2_ry = ny2; lastNum2_rw = nw2; lastNum2_rh = nh2;

        int v1_mV = (int)lroundf(cachedV1 * 1000.0f);
        int v2_mV = (int)lroundf(cachedV2 * 1000.0f);
        lastV1_mV = v1_mV; lastV2_mV = v2_mV; lastVSum_mV = v1_mV + v2_mV;

        auto calcColor = [](float pct)->uint16_t {
          return (pct < 0.10f) ? ST77XX_RED : ((pct < 0.30f) ? ST77XX_ORANGE : ST77XX_GREEN);
        };
        float pct1 = (cachedV1 - capMin) / max(0.001f, (capMax - capMin));
        pct1 = constrain(isnan(pct1)?0.0f:pct1, 0.0f, 1.0f);
        float pct2 = (cachedV2 - capMin) / max(0.001f, (capMax - capMin));
        pct2 = constrain(isnan(pct2)?0.0f:pct2, 0.0f, 1.0f);
        lastFW1 = (int)roundf((SW(cap_w) - 2*b) * pct1);
        lastCol1 = calcColor(pct1);
        lastFW2 = (int)roundf((SW(cap_w) - 2*b) * pct2);
        lastCol2 = calcColor(pct2);

        animPrevFW = -1;

        updateStatusAuto();
      }
    }
  }

  // ======= PI-Regler =======
  static unsigned long lastReg=0;
  if ( (ladeFreigabe && !weldChargeGate) && max(tempC1,tempC2) < tempWarn) {
    if (weldHold) {
      Timer4.setCompare(3, dutyCounts);
      lastReg = millis();
    } else
    if (millis() - lastReg >= CONTROL_PERIOD_MS) {
      lastReg = millis();

      float i_raw_reg = readChargeCurrentRaw();
      float dt    = CONTROL_PERIOD_MS / 1000.0f;
      float alpha = dt / (TAU_I_FILTER_S + dt);
      i_meas_filt += alpha * (i_raw_reg - i_meas_filt);

      if (!wasCharging) { i_ref_cmd = i_meas_filt; i_int = 0.0f; wasCharging = true; }

      const float I_END_A             = 0.20f;
      const float TAPER_WIDTH_CELL_V  = 0.05f;
      const float TAPER_WIDTH_SUM_V   = 0.10f;

      float vSum_now = cachedV1 + cachedV2;
      float vMaxNow  = max(cachedV1, cachedV2);

      float i_base   = min(zielStrom, zielLeistungW / max(vSum_now, 0.10f));
      float i_ref_target  = i_base;

      if (vMaxNow >= capMax - TAPER_WIDTH_CELL_V) {
        float k     = (vMaxNow - (capMax - TAPER_WIDTH_CELL_V)) / TAPER_WIDTH_CELL_V;
        k = constrain(k, 0.0f, 1.0f);
        float i_env = I_END_A + (i_base - I_END_A) * (1.0f - k);
        i_ref_target = min(i_ref_target, i_env);
      }
      if (weldVTarget > 0.0f && vSum_now >= weldVTarget - TAPER_WIDTH_SUM_V) {
        float k2    = (vSum_now - (weldVTarget - TAPER_WIDTH_SUM_V)) / TAPER_WIDTH_SUM_V;
        k2 = constrain(k2, 0.0f, 1.0f);
        float i_env2 = I_END_A + (i_base - I_END_A) * (1.0f - k2);
        i_ref_target = min(i_ref_target, i_env2);
      }

      float thresh_A   = RAMP_THRESH_FRAC * i_ref_target;
      float rampUpRate = (i_ref_cmd < thresh_A) ? RAMP_FAST_A_S : RAMP_SLOW_A_S;
      float rampDnRate = RAMP_DOWN_A_S;

      float dAllowed = ((i_ref_target > i_ref_cmd) ? rampUpRate : rampDnRate) * dt;
      float dCmd     = constrain(i_ref_target - i_ref_cmd, -dAllowed, dAllowed);
      i_ref_cmd     += dCmd;

      float e      = i_ref_cmd - i_meas_filt;
      float du_p   = Kp_counts_per_A * e;
      i_int       += Ki_counts_per_A_s * e * dt;
      if (i_int >  IINT_LIMIT) i_int =  IINT_LIMIT;
      if (i_int < -IINT_LIMIT) i_int = -IINT_LIMIT;

      float du_total = du_p + i_int;

      float countsPerPct = PWM_TOP / 100.0f;
      uint16_t stepFast  = (uint16_t)lroundf(countsPerPct * (DUTY_FAST_PCT_S * dt));
      uint16_t stepSlow  = (uint16_t)lroundf(countsPerPct * (DUTY_SLOW_PCT_S * dt));
      if (stepFast == 0) stepFast = 1;
      if (stepSlow  == 0) stepSlow  = 1;

      uint16_t maxStepThisTick = (i_ref_cmd < thresh_A) ? stepFast : stepSlow;

      int16_t step = (int16_t)lroundf(du_total);
      if (step >  (int16_t)maxStepThisTick) step =  (int16_t)maxStepThisTick;
      if (step < -(int16_t)maxStepThisTick) step = -(int16_t)maxStepThisTick;

      int32_t newDuty = (int32_t)dutyCounts + step;
      if (newDuty > DUTY_MAX) { newDuty = DUTY_MAX; if (e > 0) i_int *= 0.9f; }
      if (newDuty < DUTY_MIN) { newDuty = DUTY_MIN; if (e < 0) i_int *= 0.9f; }

      dutyCounts = (uint16_t)newDuty;
      Timer4.setCompare(3, dutyCounts);
    }
  } else {
    dutyCounts = 0; i_int = 0.0f; i_ref_cmd = 0.0f;
    Timer4.setCompare(3, 0);
    wasCharging = false;
  }

  // ======= TRIGGER: PB4 & PB12 =======
  {
    static uint8_t lastPB4  = HIGH;
    static uint8_t lastPB12 = LOW;
    static unsigned long lastEdgeMsPB4  = 0;
    static unsigned long lastEdgeMsPB12 = 0;
    const  unsigned long debounceMs     = 30;
    unsigned long now = millis();

    auto isActivePB4 = [](){
      uint8_t v = digitalRead(START_BTN);
      return TRIG_PB4_ACTIVE_LOW ? (v==LOW) : (v==HIGH);
    };
    auto isActivePB12 = [](){
      uint8_t v = digitalRead(AUTO_PULSE_PIN);
      return AUTO_PB12_ACTIVE_HIGH ? (v==HIGH) : (v==LOW);
    };

    uint8_t cur4  = digitalRead(START_BTN);
    uint8_t cur12 = digitalRead(AUTO_PULSE_PIN);

    if (cur4 != lastPB4) { lastEdgeMsPB4 = now; lastPB4 = cur4; }
    if ((now - lastEdgeMsPB4) > debounceMs) {
      bool act4 = isActivePB4();
      pb4InputActive = act4;

      if (act4 && !pb4Latched) {
        pb4Latched = true;
        if (startBtnDelay > 0.0f) {
          pb4ShotScheduled = true;
          pb4ShotTimeMs    = now + (unsigned long)lroundf(startBtnDelay * 1000.0f);
          startProgress(PROG_HAND, (unsigned long)lroundf(startBtnDelay * 1000.0f));
        } else {
          const char* why=nullptr;
          if (canPulseNow(&why)) {
            pb4ShotScheduled = false;
            doWeldSequence();
          } else {
            pb4ShotScheduled = false;
            cancelProgress();
            showWeldingStatus(why ? String(why) : "BLOCK", ST77XX_RED);
          }
        }
      }
      if (!act4 && pb4Latched) {
        pb4Latched       = false;
        pb4ShotScheduled = false;
        cancelProgress();
      }
    }

    if (pb4ShotScheduled && pb4InputActive) {
      if ((long)(now - pb4ShotTimeMs) >= 0) {
        pb4ShotScheduled = false;
        const char* why=nullptr;
        if (canPulseNow(&why)) {
          doWeldSequence();
        } else {
          showWeldingStatus(why ? String(why) : "BLOCK", ST77XX_RED);
        }
        cancelProgress();
      }
    }

    if (cur12 != lastPB12) { lastEdgeMsPB12 = now; lastPB12 = cur12; }
    if ((now - lastEdgeMsPB12) > debounceMs) {
      bool act12 = isActivePB12();
      autoInputActive = act12;

      if (act12 && !autoLatched) {
        autoLatched = true;
        if (autoPulseDelay > 0.0f) {
          autoShotScheduled = true;
          autoShotTimeMs    = now + (unsigned long)lroundf(autoPulseDelay * 1000.0f);
          startProgress(PROG_AUTO, (unsigned long)lroundf(autoPulseDelay * 1000.0f));
        } else {
          autoShotScheduled = false;
        }
      }
      if (!act12 && autoLatched) {
        autoLatched       = false;
        autoShotScheduled = false;
        cancelProgress();
      }
    }

    if (autoShotScheduled && autoInputActive) {
      if ((long)(now - autoShotTimeMs) >= 0) {
        autoShotScheduled = false;
        const char* why=nullptr;
        if (canPulseNow(&why)) {
          doWeldSequence();
        } else {
          showWeldingStatus(why ? String(why) : "BLOCK", ST77XX_RED);
        }
        cancelProgress();
      }
    }
  }

  // ======= Fortschritts-Balken zeichnen =======
  updateProgress();

  if(screenNeedsUpdate){
    if(menuState==MAIN) drawMenu();
    else { settingsFirstDraw=true; prevSettingIndex=-1; drawSettingsMenu(); }
    screenNeedsUpdate=false;
  }

  delay(10);
}

// =================== UI / Bedienung ===================
void handleEncoder(){
  encoder.tick();
  long newPos=encoder.getPosition();

  if(!inEditMode){
    if(newPos!=lastEncPos){
      if(menuState==MAIN){
        int steps = (int)(newPos - lastEncPos);
        seqMoveBy(steps);
      }else{
        int prev=settingIndex;
        settingIndex=constrain(settingIndex+(newPos-lastEncPos),0,9);
        if(prev!=settingIndex) drawSettingsMenu();
      }
      lastEncPos=newPos;
    }

    if(digitalRead(ENCODER_BTN)==LOW){
      if(menuState==MAIN){
        if(menuIndex==7){
          static unsigned long lastPress = 0;
          unsigned long now = millis();
          bool isDouble = (now - lastPress) < 350;
          lastPress = now;

          if(isDouble){
            swRunning = false; swElapsedMs = 0; swLastTickMs = now;
            swLastDrawnSec = 0; swPrevStr = ""; swForceFullRedraw = true; drawTileStopwatch();
          }else{
            swRunning = !swRunning; swLastTickMs = now;
            swPrevStr = ""; swForceFullRedraw = true; drawTileStopwatch();
          }
          while(digitalRead(ENCODER_BTN)==LOW){} 
          return;

        }else if(menuIndex==5){
          if(swRunning){ swRunning = false; swAutoPaused = true; }
          menuState=SETTINGS; settingsFirstDraw=true; prevSettingIndex=-1;
          screenNeedsUpdate=true;
          delay(200); return;
        }else{
          inEditMode=true; delay(200); return;
        }
      }else{
        if(settingIndex==9){ // EXIT
          EEPROMFloatWrite(EEPROM_ADDR_MAX,capMax);
          EEPROMFloatWrite(EEPROM_ADDR_MIN,capMin);
          EEPROM.write(EEPROM_ADDR_P1,p1Time);
          EEPROM.write(EEPROM_ADDR_P2,p2Time);
          EEPROM.write(EEPROM_ADDR_PPAUSE, (uint8_t)constrain(pPauseTime,0,50));
          EEPROMFloatWrite(EEPROM_ADDR_TEMPWARN,tempWarn);
          EEPROMFloatWrite(EEPROM_ADDR_AUTOPULSE,autoPulseDelay);
          EEPROMFloatWrite(EEPROM_ADDR_ZIELSTROM,zielStrom);
          EEPROMFloatWrite(EEPROM_ADDR_ZIELLEISTUNG,zielLeistungW);
          EEPROMFloatWrite(EEPROM_ADDR_STARTBTN_DELAY,startBtnDelay);
          EEPROMFloatWrite(EEPROM_ADDR_RESUMEHYST,resumeHyst);
          EEPROMFloatWrite(EEPROM_ADDR_WELDV, weldVTarget);
          EEPROMFloatWrite(EEPROM_ADDR_I_OFFSET, I_OFFSET_mV);
          EEPROMFloatWrite(EEPROM_ADDR_I_GAIN,   I_GAIN);
          EEPROMUInt32Write(EEPROM_ADDR_WELDCOUNT, weldCount);
          menuState=MAIN; screenNeedsUpdate=true;

          if(swAutoPaused){
            swAutoPaused = false; swRunning = true; swLastTickMs = millis();
          }
          swForceFullRedraw = true;

          // Status auf aktuellen Zustand setzen
          updateStatusAuto();
          delay(200); return;

        }else if(settingIndex==8){ // I-Offset Auto
          useFont(FF_LABEL, ST77XX_WHITE); setCursorScaled(10,115+YADJ_LABEL);
          tft.print("Kalibriere 0A Offset...");
          tft.setFont(NULL);

          calibrateZeroCurrentOffset();

          tft.fillRect(SX(0), SY(110), SW(160), SH(16), ST77XX_BLACK);

          updateStatusAuto();

          delay(200); return;

        }else if(settingIndex==7){ // Reset Count
          weldCount=0;
          EEPROMUInt32Write(EEPROM_ADDR_WELDCOUNT, weldCount);
          drawWeldCount();

          updateStatusAuto();
          delay(200); return;

        }else{
          inEditMode=true; delay(200); return;
        }
      }
    }

  }else{
    if(newPos!=lastEncPos){
      float delta=(newPos-lastEncPos)*0.1f;
      float step01 = 0.01f*(newPos-lastEncPos);
      float step10 = 0.10f*(newPos-lastEncPos);

      if(menuState==MAIN){
        if(menuIndex==0){ p1Time=constrain(p1Time+(newPos-lastEncPos),1,20); drawTileP1(); }
        else if(menuIndex==8){ pPauseTime=constrain(pPauseTime+(newPos-lastEncPos),0,50); drawTilePause(); }
        else if(menuIndex==1){ p2Time=constrain(p2Time+(newPos-lastEncPos),0,20); drawTileP2(); }
        else if(menuIndex==2){ autoPulseDelay=constrain(autoPulseDelay+delta,0.0f,2.0f); drawTileAuto(); }
        else if(menuIndex==6){
          clampWeldTargetToRange();
          float lo=2.0f*capMin, hi=2.0f*capMax;
          weldVTarget = constrain(weldVTarget + step01, lo, hi);
          drawTileWeldV();
          updateStatusAuto();
        }
      }else{
        if(settingIndex==0){ capMax        = constrain(capMax        + step01, 2.0f, 4.2f); clampWeldTargetToRange(); }
        else if(settingIndex==1){ capMin   = constrain(capMin        + step01,  2.0f, 4.2f); clampWeldTargetToRange(); }
        else if(settingIndex==2){ tempWarn = constrain(tempWarn      + step10*10.0f, 0.0f, 100.0f); }
        else if(settingIndex==3){ zielStrom= constrain(zielStrom     + step10, 0.1f, 10.0f); }
        else if(settingIndex==4){ float stepW=(newPos-lastEncPos); zielLeistungW=constrain(zielLeistungW+stepW, 1.0f, 60.0f); }
        else if(settingIndex==5){ startBtnDelay = constrain(startBtnDelay + step01, 0.0f, 2.0f); }
        else if(settingIndex==6){ float step=0.01f*(newPos-lastEncPos); resumeHyst=constrain(resumeHyst+step, 0.01f, 0.10f); }

        float values[]={capMax,capMin,tempWarn,zielStrom,zielLeistungW,startBtnDelay,resumeHyst};
        drawSettingsValue(settingIndex, values[settingIndex], true);

        updateStatusAuto();
      }
      lastEncPos=newPos;
    }
    if(digitalRead(ENCODER_BTN)==LOW){ inEditMode=false; delay(200); }
  }
}

// =================== Anzeige/Status ===================
// NEU: Status nur aktualisieren, wenn sich Text/Farbe ändern
// und nur im MAIN-Menü zeichnen (nicht in SETTINGS)
static String     g_statusMsg       = "";
static uint16_t   g_statusColor     = ST77XX_WHITE;
static MenuState  g_statusDrawnForState = SETTINGS;

void showWeldingStatus(const String& msg, uint16_t color){
  // In SETTINGS nichts zeichnen, nur merken
  if (menuState != MAIN) {
    g_statusMsg   = msg;
    g_statusColor = color;
    g_statusDrawnForState = menuState;
    return;
  }

  bool needRedraw =
    (msg != g_statusMsg) ||
    (color != g_statusColor) ||
    (g_statusDrawnForState != MAIN);

  if (!needRedraw) return;

  tft.fillRect(SX(115), SY(0), SW(50), SH(10), ST77XX_BLACK);
  useFont(FF_STAT, color);
  setCursorScaled(115,2+YADJ_STAT);
  tft.print(msg);
  tft.setFont(NULL);

  g_statusMsg        = msg;
  g_statusColor      = color;
  g_statusDrawnForState = MAIN;
}

// NEU: Zentrale Statuslogik
void updateStatusAuto(){
  if (weldHold) { showWeldingStatus("WELDING", ST77XX_YELLOW); return; }
  if (forcedDischarge && dutyCounts <= 5) { showWeldingStatus("ENTLADE", ST77XX_ORANGE); return; }

  bool charging = (ladeFreigabe && !weldChargeGate) && (dutyCounts > 5) && (max(tempC1,tempC2) < tempWarn);
  if (charging) { showWeldingStatus("LADEN", ST77XX_GREEN); return; }

  if (max(tempC1, tempC2) > tempWarn) { showWeldingStatus("TEMP", ST77XX_RED); return; }
  if (cachedV1 > capMax + OV_EPS_V || cachedV2 > capMax + OV_EPS_V) { showWeldingStatus("OV", ST77XX_RED); return; }
  if (cachedV1 < capMin || cachedV2 < capMin) { showWeldingStatus("UV", ST77XX_RED); return; }

  showWeldingStatus("BEREIT", ST77XX_WHITE);
}


void drawWeldCount(){
  const int x = 0;
  const int y = 15;
  const int w = 33;
  const int h = 10;

  String s = "C:" + String(weldCount);
  tft.fillRect(SX(x), SY(y), SW(w), SH(h), ST77XX_BLACK);
  useFont(FF_LABEL, ST77XX_WHITE);
  setCursorScaled(x+2, y + YADJ_LABEL);
  tft.print(s);
  tft.setFont(NULL);
}

void doWeldSequence(){
  const char* why=nullptr;
  if (!canPulseNow(&why)) {
    showWeldingStatus(why ? String(why) : "BLOCK", ST77XX_RED);
    cancelProgress();
    return;
  }

  showWeldingStatus("WELDING", ST77XX_YELLOW);

  weldHold = true;
  uint16_t dutySaved  = dutyCounts;
  uint16_t dutyTarget = (uint16_t)lroundf((float)dutySaved*(1.0f - CHARGE_THROTTLE_PCT));
  if (dutyTarget > dutySaved) dutyTarget = dutySaved;
  rampDutyDownTo(dutyTarget);

  if (THROTTLE_DWELL_PRE_MS > 0) delay(5);

  digitalWrite(PULSE_PIN, HIGH); delay(p1Time); digitalWrite(PULSE_PIN, LOW);
  if(p2Time>0 && pPauseTime>0){ delay(pPauseTime); }
  if(p2Time>0){ digitalWrite(PULSE_PIN, HIGH); delay(p2Time); digitalWrite(PULSE_PIN, LOW); }

  if (THROTTLE_DWELL_POST_MS > 0) delay(5);

  if ((ladeFreigabe && !weldChargeGate) && max(tempC1, tempC2) < tempWarn){
    rampDutyUpTo(dutySaved);
  } else {
    dutyCounts = 0; Timer4.setCompare(3, 0);
  }

  weldHold = false;

  delay(100);
  updateStatusAuto();

  cancelProgress();

  weldCount++;
  EEPROMUInt32Write(EEPROM_ADDR_WELDCOUNT, weldCount);
  drawWeldCount();
}

// =================== Messungen ===================
float readVoltage(int pin,float R1,float R2,float calib){
  int val=analogRead(pin); float v=(val*3.3f/4095.0f); return v*(R1+R2)/R2*calib;
}
float readVoltageAveraged(int pin,float R1,float R2,float calib){
  const int N=24; float sum=0.0f;
  for(int i=0;i<N;i++){ sum+=readVoltage(pin,R1,R2,calib); delayMicroseconds(75);} return sum/N;
}

// ====== Strom „Methode 2“ ======
float readChargeCurrentRaw(){
  float vshunt_mV  = ina219.getShuntVoltage_mV();
  float v_corr_mV  = (vshunt_mV - I_OFFSET_mV) * I_GAIN;
  float i_A = fabs( (v_corr_mV / 1000.0f) / RSHUNT_OHMS );
  return i_A;
}

// =================== UI: Menü & Settings ===================

#define TOP_TILE_W 38
#define TOP_TILE_GAP 2
#define TOP_TILE_X0 2
#define TOP_TILE_X1 (TOP_TILE_X0 + TOP_TILE_W + TOP_TILE_GAP)
#define TOP_TILE_X2 (TOP_TILE_X1 + TOP_TILE_W + TOP_TILE_GAP)
#define TOP_TILE_X3 (TOP_TILE_X2 + TOP_TILE_W + TOP_TILE_GAP)


static void drawSumVoltageBox(float vSum){
  tft.fillRect(SX(SUM_BOX_X), SY(SUM_BOX_Y), SW(SUM_BOX_W), SH(SUM_BOX_H), ST77XX_BLACK);

  String vs = fmtFloat(vSum, 3);
  useFont(FF_VAL, ST77XX_WHITE);
  int xText = centeredXForTextPx(vs, FF_VAL, SX(SUM_BOX_X), SW(SUM_BOX_W));
  int yText = SY(SUM_BOX_Y + 3 + YADJ_SUM);
  tft.setCursor(xText, yText);
  tft.print(vs);

  int w = textWidthPx(vs, FF_VAL);
  useFont(FF_UNIT, ST77XX_WHITE);
  tft.setCursor(xText + w + UNIT_GAP_SUM_V_PX, yText);
  tft.print("V");
  tft.setFont(NULL);
}

void drawMenu(){
  stForceFullRedraw = true; stLayoutValid=false;
  tpForceFullRedraw = true; tpLayoutValid=false;

  tft.fillScreen(ST77XX_BLACK);
  uint16_t frameColor=ST77XX_P1_GREEN;

  // Rahmen
  drawTopBatteryFrame(6, 0, 47, 11, frameColor);
  tft.fillRect(SX(4),  SY(2), SW(2), SH(7), frameColor);
  drawTopBatteryFrame(64, 0, 47, 11, frameColor);
  tft.fillRect(SX(62), SY(2), SW(2), SH(7), frameColor);

  // Gauge / oder Anim-Startbild
  const int b=2; const int iw = SW(47)-2*b; const int ih = SH(11)-2*b;
  const int ix1 = SX(6)+b,  iy1 = SY(0)+b;
  const int ix2 = SX(64)+b, iy2 = SY(0)+b;

  if (forcedDischarge && dutyCounts <= 5) {
    tft.fillRect(ix1, iy1, iw, ih, ST77XX_GREEN);
    tft.fillRect(ix2, iy2, iw, ih, ST77XX_GREEN);
    animPrevFW = iw;
  } else {
    drawTopBatteryGauge(6,0,47,11,cachedV1);
    drawTopBatteryGauge(64,0,47,11,cachedV2);

    const int txt_y   = SY(2 + YADJ_TOP) - 1;
    String v1s=fmtFloat(cachedV1,3) + "V";
    String v2s=fmtFloat(cachedV2,3) + "V";
    int cap1_ix = SX(6)  + b;
    int cap2_ix = SX(64) + b;
    int cap_iw  = SW(47) - 2*b;
    int num1_x = centeredXForTextPx(v1s, FF_VAL, cap1_ix, cap_iw);
    int num2_x = centeredXForTextPx(v2s, FF_VAL, cap2_ix, cap_iw);
    useFont(FF_VAL,ST77XX_WHITE);
    tft.setCursor(num1_x, txt_y); tft.print(v1s);
    tft.setCursor(num2_x, txt_y); tft.print(v2s);
    tft.setFont(NULL);
  }

  // Summe
  drawSumVoltageBox(cachedV1 + cachedV2);

  // Status (zentral gesteuert)
  updateStatusAuto();

  // Top Tiles
  drawTileP1();
  drawTilePause();
  drawTileP2();
  drawTileAuto();

  // Stoppuhr
  swForceFullRedraw = true;
  drawTileStopwatch();

  // Bottom Tiles
  drawTileStrom();
  drawTileTemp();
  drawTileWeldV();
  drawTileSettings();

  drawWeldCount();

  // Fortschrittsbereich unten leeren
  progressClearArea();
}

// ---- Settings ----
void drawSettingsMenu(){
  String labels[]={
    "Max Volt","Min Volt","Temp Warn","Zielstrom","Zielleistung",
    "Start Delay","Wiederanlauf dV","Reset Count","I-Offset Auto","EXIT"
  };
  float values[]={capMax,capMin,tempWarn,zielStrom,zielLeistungW,startBtnDelay,resumeHyst,0,0,0};

  if(settingsFirstDraw){
    tft.fillScreen(ST77XX_BLACK);
    for(int i=0;i<10;i++) drawSettingsLine(i,labels[i],values[i],(i==settingIndex));
    settingsFirstDraw=false; prevSettingIndex=settingIndex; return;
  }
  if(settingIndex!=prevSettingIndex){
    drawSettingsLine(prevSettingIndex,labels[prevSettingIndex],values[prevSettingIndex],false);
    drawSettingsLine(settingIndex,labels[settingIndex],values[settingIndex],true);
    prevSettingIndex=settingIndex;
  }
}

void drawSettingsLine(int index,const String& label,float value,bool selected){
  uint16_t bg=selected?ST77XX_BLUE:ST77XX_BLACK;
  tft.fillRect(SX(0), SY(SETTINGS_Y0+index*SETTINGS_SPACING), SW(160), SH(SETTINGS_LINE_H), bg);
  useFont(FF_LABEL,ST77XX_WHITE); setCursorScaled(4,SETTINGS_Y0+index*SETTINGS_SPACING+YADJ_LABEL+SETTINGS_TEXT_YSHIFT); tft.print(label); tft.setFont(NULL);

  if(index==7 || index==8 || index==9){
    tft.fillRect(SX(SETTINGS_VAL_X), SY(SETTINGS_Y0+index*SETTINGS_SPACING),
                 SW(SETTINGS_VAL_W), SH(SETTINGS_LINE_H), bg);
    return;
  }
  drawSettingsValue(index,value,selected);
}

void drawSettingsValue(int index,float value,bool selected){
  if(index==7 || index==8 || index==9) return;
  String unit=""; uint8_t dec=1; float shown=value;
  switch(index){
    case 0: unit="V"; dec=2; break;
    case 1: unit="V"; dec=2; break;
    case 2: unit="C"; break;
    case 3: unit="A"; break;
    case 4: unit="W"; dec=0; break;
    case 5: unit="s"; dec=2; break;
    case 6: unit="mV"; dec=0; shown=value*1000.0f; break;
    default: dec=0;
  }
  uint16_t bg=selected?ST77XX_BLUE:ST77XX_BLACK; int yBase=SETTINGS_Y0+index*SETTINGS_SPACING;
  tft.fillRect(SX(SETTINGS_VAL_X), SY(yBase), SW(SETTINGS_VAL_W), SH(SETTINGS_LINE_H), bg);
  String vs=fmtFloat(shown,dec); useFont(FF_VAL,ST77XX_WHITE); setCursorScaled(SETTINGS_VAL_X,yBase+YADJ_VAL+SETTINGS_TEXT_YSHIFT); tft.print(vs);
  int w=textWidthPx(vs,FF_VAL); if(unit.length()){ useFont(FF_UNIT,ST77XX_WHITE);
    tft.setCursor(SX(SETTINGS_VAL_X)+w+UNIT_GAP_SETTINGS_PX, SY(yBase+YADJ_UNIT+SETTINGS_TEXT_YSHIFT)); tft.print(unit); }
  tft.setFont(NULL);
}

// =================== Tiles ===================
void drawTileFrame(int x,int y,int w,int h,bool selected,uint16_t color){
  const int bx = SX(x),  by = SY(y);
  const int bw = SW(w),  bh = SH(h);
  const int b  = 2;

  if(selected){
    tft.fillRect(bx,      by,      bw, b,  ST77XX_WHITE);
    tft.fillRect(bx,      by+bh-b, bw, b,  ST77XX_WHITE);
    tft.fillRect(bx,      by,      b,  bh, ST77XX_WHITE);
    tft.fillRect(bx+bw-b, by,      b,  bh, ST77XX_WHITE);
    tft.fillRect(bx+b, by+b, bw-2*b, bh-2*b, color);
  }else{
    tft.fillRect(bx, by, bw, bh, color);
  }
}

void drawTileP1(){
  const int x = TOP_TILE_X0;
  const int y = 30 + TOP_TILE_TRIM_BASE_PX;
  const int w = TOP_TILE_W;
  const int h = 45 - TOP_TILE_TRIM_BASE_PX;

  drawTileFrame(x,y,w,h, menuIndex==0, ST77XX_P1_GREEN);

  useFont(FF_LABEL,ST77XX_WHITE);
  setCursorScaled(x+5, y+5+YADJ_LABEL); tft.print("Puls1"); tft.setFont(NULL);

  String v=String(p1Time);
  useFont(FF_VAL,ST77XX_WHITE);
  setCursorScaled(x+5, y + topTileValueYOffset(h) + YADJ_VAL); tft.print(v);
  int wpx=textWidthPx(v,FF_VAL);
  useFont(FF_UNIT,ST77XX_WHITE);
  tft.setCursor(SX(x+5)+wpx+UNIT_GAP_SETTINGS_PX, SY(y + topTileValueYOffset(h) + YADJ_UNIT)); tft.print("ms");
  tft.setFont(NULL);
}

void drawTilePause(){
  const int x = TOP_TILE_X1;
  const int y = 30 + TOP_TILE_TRIM_BASE_PX;
  const int w = TOP_TILE_W;
  const int h = 45 - TOP_TILE_TRIM_BASE_PX;

  drawTileFrame(x,y,w,h, menuIndex==8, ST77XX_ORANGE);

  useFont(FF_LABEL,ST77XX_WHITE);
  setCursorScaled(x+5, y+5+YADJ_LABEL); tft.print("Pause"); tft.setFont(NULL);

  String v=String(pPauseTime);
  useFont(FF_VAL,ST77XX_WHITE);
  setCursorScaled(x+5, y + topTileValueYOffset(h) + YADJ_VAL); tft.print(v);
  int wpx=textWidthPx(v,FF_VAL);
  useFont(FF_UNIT,ST77XX_WHITE);
  tft.setCursor(SX(x+5)+wpx+UNIT_GAP_SETTINGS_PX, SY(y + topTileValueYOffset(h) + YADJ_UNIT)); tft.print("ms");
  tft.setFont(NULL);
}

void drawTileP2(){
  const int x = TOP_TILE_X2;
  const int y = 30 + TOP_TILE_TRIM_BASE_PX;
  const int w = TOP_TILE_W;
  const int h = 45 - TOP_TILE_TRIM_BASE_PX;

  drawTileFrame(x,y,w,h, menuIndex==1, ST77XX_RAL4003);

  useFont(FF_LABEL,ST77XX_WHITE);
  setCursorScaled(x+5, y+5+YADJ_LABEL); tft.print("Puls 2"); tft.setFont(NULL);

  String v=String(p2Time);
  useFont(FF_VAL,ST77XX_WHITE);
  setCursorScaled(x+5, y + topTileValueYOffset(h) + YADJ_VAL); tft.print(v);
  int wpx=textWidthPx(v,FF_VAL);
  useFont(FF_UNIT,ST77XX_WHITE);
  tft.setCursor(SX(x+5)+wpx+UNIT_GAP_SETTINGS_PX, SY(y + topTileValueYOffset(h) + YADJ_UNIT)); tft.print("ms");
  tft.setFont(NULL);
}

void drawTileAuto(){
  const int x = TOP_TILE_X3;
  const int y = 30 + TOP_TILE_TRIM_BASE_PX;
  const int w = TOP_TILE_W;
  const int h = 45 - TOP_TILE_TRIM_BASE_PX;

  drawTileFrame(x,y,w,h, menuIndex==2, ST77XX_AUTO_TEAL);

  useFont(FF_LABEL,ST77XX_WHITE);
  setCursorScaled(x+5, y+5+YADJ_LABEL); tft.print("Auto"); tft.setFont(NULL);

  if(autoPulseDelay==0.0f){
    useFont(FF_VAL,ST77XX_WHITE);
    setCursorScaled(x+5, y + topTileValueYOffset(h) + YADJ_VAL); tft.print("AUS"); tft.setFont(NULL);
  }else{
    String v=String(autoPulseDelay,1);
    useFont(FF_VAL,ST77XX_WHITE);
    setCursorScaled(x+5, y + topTileValueYOffset(h) + YADJ_VAL); tft.print(v);
    int wpx=textWidthPx(v,FF_VAL);
    useFont(FF_UNIT,ST77XX_WHITE);
    tft.setCursor(SX(x+5)+wpx+UNIT_GAP_AUTO_S_PX, SY(y + topTileValueYOffset(h) + YADJ_UNIT)); tft.print("s");
    tft.setFont(NULL);
  }
}

// ---- UNTERE REIHE ----
void drawTileStrom(){ 
  const int x=2,y=78,w=39,h=47;
  const bool selected = (menuIndex==3);
  stX = SX(x); stY = SY(y); stW = SW(w); stH = SH(h);
  stTileBgColor = ST77XX_PURPLE;

  const int LINE_GAP_BASEPX = 10;
  const int line1_y_base = y + 20;
  const int line2_y_base = line1_y_base + LINE_GAP_BASEPX;

  if (stForceFullRedraw || !stLayoutValid || stLastSel != selected) {
    drawTileFrame(x,y,w,h, selected, ST77XX_PURPLE);
    useFont(FF_LABEL,ST77XX_WHITE); setCursorScaled(x+5,y+5+YADJ_LABEL); tft.print("Strom"); tft.setFont(NULL);
    tft.fillRect(stX+2, SY(line1_y_base-2), stW-4, SH(h - (line1_y_base - y) - 2), stTileBgColor);

    buildFourCellLayout(stX, stY, stW, stH,
                        line1_y_base, line2_y_base,
                        stCellWDigit, stCellWDot, stCellH,
                        stCellX, stUnitX_A, stUnitX_W,
                        stUnitBaseY1, stUnitBaseY2,
                        UNIT_GAP_STROM_A_PX, "A","W",
                        stAsc, stDesc, VALUE_CELL_GAP_PX);

    useFont(FF_UNIT, ST77XX_WHITE);
    tft.setCursor(stUnitX_A, stUnitBaseY1); tft.print("A");
    tft.setCursor(stUnitX_W, stUnitBaseY2); tft.print("W");
    tft.setFont(NULL);

    stPrevA = "    "; stPrevW = "    ";
    stLayoutValid = true; stForceFullRedraw = false; stLastSel = selected;
  }

  const String aStr = fmt00_0(cachedIStrA);
  const String wStr = fmt00_0((cachedV1+cachedV2)*cachedIStrA);
  int baseline1 = SY(line1_y_base + YADJ_VAL);
  int baseline2 = SY(line2_y_base + YADJ_VAL);
  drawValueDiff(aStr, stPrevA, stCellX[0], baseline1, stCellWDigit, stCellWDot, stCellH, stTileBgColor, stAsc, stDesc);
  drawValueDiff(wStr, stPrevW, stCellX[1], baseline2, stCellWDigit, stCellWDot, stCellH, stTileBgColor, stAsc, stDesc);
}

static void drawDegreeBeforeC(int16_t unitX, int16_t baseY, uint16_t fg, uint16_t bg){
  useFont(FF_UNIT, fg);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds("C", unitX, baseY, &x1, &y1, &w, &h);
  const int d=5, gap=-7;
  int cx = x1 - gap - d/2; int cy = y1 + d/2;
  tft.fillCircle(cx, cy, d/2, fg);
  tft.fillCircle(cx, cy, (d/2)-1, bg);
}

void drawTileTemp(){
  const int x=43, y=78, w=43, h=47;
  const bool selected = (menuIndex==4);
  tpX = SX(x); tpY = SY(y); tpW = SW(w); tpH = SH(h);
  tpTileBgColor = ST77XX_RED;

  const int LINE_GAP_BASEPX = 10;
  const int line1_y_base = y + 20;
  const int line2_y_base = line1_y_base + LINE_GAP_BASEPX;

  if (tpForceFullRedraw || !tpLayoutValid || tpLastSel != selected) {
    drawTileFrame(x,y,w,h, selected, ST77XX_RED);
    useFont(FF_UNIT, ST77XX_WHITE); setCursorScaled(x+5, y+6 + YADJ_LABEL); tft.print("Temp"); tft.setFont(NULL);
    tft.fillRect(tpX+2, SY(line1_y_base-2), tpW-4, SH(h - (line1_y_base - y) - 2), tpTileBgColor);

    buildFourCellLayout(tpX, tpY, tpW, tpH,
                        line1_y_base, line2_y_base,
                        tpCellWDigit, tpCellWDot, tpCellH,
                        tpCellX, tpUnitX_C1, tpUnitX_C2,
                        tpUnitBaseY1, tpUnitBaseY2,
                        UNIT_GAP_TEMP_C_PX, "C","C", tpAsc, tpDesc, VALUE_CELL_GAP_PX);

    const int VALUE_X_SHIFT_PX = 5;
    for (int row = 0; row < 2; ++row) for (int col = 0; col < 4; ++col) tpCellX[row][col] += VALUE_X_SHIFT_PX;

    useFont(FF_UNIT, ST77XX_WHITE);
    tft.setCursor(tpUnitX_C1 + UNIT_C_SHIFT_PX, tpUnitBaseY1); tft.print("C");
    tft.setCursor(tpUnitX_C2 + UNIT_C_SHIFT_PX, tpUnitBaseY2); tft.print("C");
    tft.setCursor(tpUnitX_C1 + UNIT_1_SHIFT_PX, tpUnitBaseY1); tft.print("1.");
    tft.setCursor(tpUnitX_C2 + UNIT_2_SHIFT_PX, tpUnitBaseY2); tft.print("2.");
    drawDegreeBeforeC(tpUnitX_C1, tpUnitBaseY1, ST77XX_WHITE, tpTileBgColor);
    drawDegreeBeforeC(tpUnitX_C2, tpUnitBaseY2, ST77XX_WHITE, tpTileBgColor);
    tft.setFont(NULL);

    tpPrevT1 = "    "; tpPrevT2 = "    ";
    tpLayoutValid = true; tpForceFullRedraw = false; tpLastSel = selected;
  }

  const String t1Str = fmt00_0(tempC1);
  const String t2Str = fmt00_0(tempC2);
  int baseline1 = SY(line1_y_base + YADJ_VAL);
  int baseline2 = SY(line2_y_base + YADJ_VAL);
  drawValueDiff(t1Str, tpPrevT1, tpCellX[0], baseline1, tpCellWDigit, tpCellWDot, tpCellH, tpTileBgColor, tpAsc, tpDesc);
  drawValueDiff(t2Str, tpPrevT2, tpCellX[1], baseline2, tpCellWDigit, tpCellWDot, tpCellH, tpTileBgColor, tpAsc, tpDesc);
}

void drawTileSettings(){
  int x=126,y=78,w=34,h=47;
  drawTileFrame(x,y,w,h, menuIndex==5, ST77XX_SETTINGS_PURPLE);
  useFont(FF_LABEL,ST77XX_WHITE); setCursorScaled(x+5,y+20+YADJ_LABEL); tft.print("Einst."); tft.setFont(NULL);
}

void drawTileWeldV(){
  int x=88,y=78,w=36,h=47;
  drawTileFrame(x,y,w,h, menuIndex==6, ST77XX_DARKGREY);
  useFont(FF_LABEL,ST77XX_WHITE); setCursorScaled(x+5,y+6+YADJ_LABEL); tft.print("Weld V"); tft.setFont(NULL);
  clampWeldTargetToRange();
  String v=fmtFloat(weldVTarget,2);
  useFont(FF_VAL,ST77XX_WHITE); setCursorScaled(x+5,y+25+YADJ_VAL); tft.print(v);
  int wpx=textWidthPx(v,FF_VAL); useFont(FF_UNIT,ST77XX_WHITE);
  tft.setCursor(SX(x+5)+wpx+UNIT_GAP_SETTINGS_PX, SY(y+25+YADJ_UNIT)); tft.print("V"); tft.setFont(NULL);
}

// =================== Fan control ===================
void updateFanControl(){
  float hottest = max(tempC1, tempC2);
  if(!fanState && hottest >= FAN_ON_C){
    fanState = true; digitalWrite(FAN_PIN, HIGH);
  }else if(fanState && hottest <= FAN_OFF_C){
    fanState = false; digitalWrite(FAN_PIN, LOW);
  }
}

// =================== Helper: gezielt Kachel neuzeichnen ===================
static void redrawTileByIndex(int idx){
  switch(idx){
    case 7: drawTileStopwatch(); break;
    case 0: drawTileP1();       break;
    case 8: drawTilePause();    break;
    case 1: drawTileP2();       break;
    case 2: drawTileAuto();     break;
    case 3: stForceFullRedraw=true; stLayoutValid=false; drawTileStrom(); break;
    case 4: tpForceFullRedraw=true; tpLayoutValid=false; drawTileTemp();  break;
    case 5: drawTileSettings(); break;
    case 6: drawTileWeldV();    break;
  }
}

// =================== 0-A-Autokalibrierung ===================
void calibrateZeroCurrentOffset(){
  Timer4.setCompare(3, 0);
  delay(10);
  const int N = 200;
  float sum_mV = 0.0f;
  for(int i=0;i<N;i++){
    sum_mV += ina219.getShuntVoltage_mV();
    delay(2);
  }
  I_OFFSET_mV = sum_mV / N;
  EEPROMFloatWrite(EEPROM_ADDR_I_OFFSET, I_OFFSET_mV);
}

// =================== STOPPUHR ===================
static void measureText(const String& s, uint16_t &w, uint16_t &h){
  int16_t bx, by; uint16_t bw, bh;
  useFont(FF_VAL, ST77XX_WHITE);
  tft.getTextBounds(s, 0, 0, &bx, &by, &bw, &bh);
  if(bw==0) bw=1; if(bh==0) bh=12;
  w=bw; h=bh;
}
static void valueFontMetrics(uint16_t &digitW, uint16_t &dotW,uint16_t &cellH,int16_t &asc,int16_t &desc){
  useFont(FF_VAL, ST77XX_WHITE);
  bool init=false; int16_t minTop=0, maxBottom=0; digitW=0; dotW=0;
  auto addChar=[&](char c){
    int16_t x1,y1; uint16_t w,h; tft.getTextBounds(String(c), 0, 0, &x1, &y1, &w, &h);
    if(!init){ minTop=y1; maxBottom=y1+h; init=true; } else { if(y1<minTop) minTop=y1; if(y1+h>maxBottom) maxBottom=y1+h; }
    return w;
  };
  for(char c='0'; c<='9'; ++c){ uint16_t w = addChar(c); if(w>digitW) digitW=w; }
  dotW = addChar('.');
  asc  = -minTop; desc = maxBottom; cellH = (uint16_t)(asc + desc);
}
static void buildFourCellLayout(int tile_px_x,int tile_px_y,int tile_px_w,int tile_px_h,int line1_y_base,int line2_y_base,uint16_t &cellWdigit,uint16_t &cellWdot,uint16_t &cellH,int16_t cellX_out[2][4],int16_t &unitX1,int16_t &unitX2,int16_t &unitBaseY1,int16_t &unitBaseY2,int unit_gap_px,const char* unit1,const char* unit2,int16_t &ascOut,int16_t &descOut,int value_cell_gap_px){
  valueFontMetrics(cellWdigit, cellWdot, cellH, ascOut, descOut);
  const int gap = max(0, value_cell_gap_px);
  uint16_t valueWidth = 2*cellWdigit + cellWdot + cellWdigit + 3*gap;
  useFont(FF_UNIT, ST77XX_WHITE);
  int16_t bx,by; uint16_t bw,bh; tft.getTextBounds(unit1, 0, 0, &bx, &by, &bw, &bh);
  uint16_t unitW = bw ? bw : 1;
  unitBaseY1 = SY(line1_y_base + YADJ_UNIT);
  unitBaseY2 = SY(line2_y_base + YADJ_UNIT);
  int valueBlockW = valueWidth + unit_gap_px + unitW;
  int leftX = tile_px_x + (tile_px_w - valueBlockW)/2;
  for(int row=0; row<2; ++row){
    int x = leftX;
    cellX_out[row][0] = x; x += cellWdigit + gap;
    cellX_out[row][1] = x; x += cellWdigit + gap;
    cellX_out[row][2] = x; x += cellWdot   + gap;
    cellX_out[row][3] = x;
  }
  unitX1 = leftX + valueWidth + unit_gap_px;
  unitX2 = unitX1;
}
static void drawCharInCell(char c,int16_t cellX,int16_t baselineY,uint16_t cellW,uint16_t /*cellH*/,uint16_t bg,int16_t asc,int16_t desc,bool useUnitFont=false){
  tft.setFont(useUnitFont ? FF_UNIT : FF_VAL);
  int16_t bx,by; uint16_t bw,bh; tft.getTextBounds(String(c), 0, 0, &bx, &by, &bw, &bh);
  if(bw==0) bw=1;
  const int clrPadX=1, clrPadY=1; int topY=baselineY-asc; int clrH=asc+desc;
  tft.fillRect(cellX - clrPadX, topY - clrPadY, cellW + 2*clrPadX, clrH + 2*clrPadY, bg);
  int cx = cellX + ((int)cellW - (int)bw)/2;
  tft.setCursor(cx, baselineY);
  tft.print(c);
}
static void drawValueDiff(const String& cur,String& prev,int16_t cellX[4],int16_t baselineY,uint16_t cellWdigit,uint16_t cellWdot,uint16_t cellH,uint16_t bg,int16_t asc,int16_t desc){
  String c=cur; if(c.length()<4){ while(c.length()<4) c=" "+c; }
  String p=prev; if(p.length()<4){ while(p.length()<4) p=" "+p; }
  uint16_t W[4]={cellWdigit,cellWdigit,cellWdot,cellWdigit};
  useFont(FF_VAL, ST77XX_WHITE);
  for(int i=0;i<4;i++) if(c[i]!=p[i]) drawCharInCell(c[i], cellX[i], baselineY, W[i], cellH, bg, asc, desc, false);
  prev = c;
}

static void buildStopwatchLayout(int barX, int barY, int barW, int barH){
  uint16_t w,h;
  swMaxDigitW = 0;
  for(char c='0'; c<='9'; ++c){ measureText(String(c), w, h); if(w>swMaxDigitW) swMaxDigitW = w; }
  measureText(":", swColonW, h);
  uint16_t cellGap = 1, totalW = 0;
  for(int i=0;i<8;i++){ bool isColon=(i==2||i==5); swCellW[i]=isColon?swColonW:swMaxDigitW; totalW+=swCellW[i]; if(i<7) totalW+=cellGap; }
  int leftX = barX + (barW - (int)totalW)/2;
  uint16_t refH; measureText("00:00:00", w, refH);
  swBaselineY = barY + (barH + (int)refH)/2 - 1;
  int x = leftX; for(int i=0;i<8;i++){ swCellX[i]=x; x+=swCellW[i]; if(i<7) x+=cellGap; }
  swTxtX = leftX; swTxtW = totalW; swTxtY = barY + (barH - (int)refH)/2; swTxtH = refH; swLayoutValid = 1;
}
static void drawStopwatchFixed(const String& s, uint16_t bg){
  if(swPrevStr.length()!=8) swPrevStr="        ";

  useFont(FF_VAL, ST77XX_WHITE);

  const int CLR_PAD_X = 1;
  const int CLR_PAD_Y = 1;

  for(int i=0;i<8;i++){
    char nc = s[i];
    char pc = swPrevStr[i];

    bool needsRedraw = swForceFullRedraw || (nc != pc);
    if(!needsRedraw) continue;

    int x = swCellX[i];
    int w = swCellW[i];

    tft.fillRect(x - CLR_PAD_X, swTxtY - CLR_PAD_Y,
                 w + 2*CLR_PAD_X, swTxtH + 2*CLR_PAD_Y, bg);

    int16_t bx,by; uint16_t bw,bh;
    tft.getTextBounds(String(nc), 0, 0, &bx, &by, &bw, &bh);
    if(bw==0) bw=1; if(bh==0) bh=12;
    int cx = x + (w - (int)bw)/2;
    tft.setCursor(cx, swBaselineY);
    tft.print(nc);
  }

  swPrevStr = s;
  swForceFullRedraw = false;
  tft.setFont(NULL);
}

void drawTileStopwatch(){
  if(menuState != MAIN) return;

  const int x=2, w=50, h=15, GAP=2;
  const int y = (30 + TOP_TILE_TRIM_BASE_PX) - h - GAP;

  uint16_t bg = swRunning ? ST77XX_AUTO_TEAL : ST77XX_DARKGREY;
  bool selected = (menuIndex==7);

  if(swForceFullRedraw || swLastBg != bg || swLastSel != selected){
    tft.fillRect(SX(x), SY(y), SW(w), SH(h), bg);
    if(selected){
      tft.drawRect(SX(x), SY(y), SW(w), SH(h), ST77XX_WHITE);
      tft.drawRect(SX(x)+1, SY(y)+1, SW(w)-2, SH(h)-2, ST77XX_WHITE);
    }
    swLayoutValid = 0;
    swPrevStr = "";
    swLastBg = bg; swLastSel = selected;
  }

  if(!swLayoutValid){
    buildStopwatchLayout(SX(x), SY(y), SW(w), SH(h));
  }

  String s = fmtStopwatch(swElapsedMs/1000);
  if(s.length()!=8){ while(s.length()<8) s = "0" + s; }

  drawStopwatchFixed(s, bg);
}


String fmtStopwatch(uint32_t s){
  uint32_t hh = s/3600; s%=3600;
  uint32_t mm = s/60;
  uint32_t ss = s%60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
           (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
  return String(buf);
}

void updateStopwatch(){
  static unsigned long lastUpd = 0;
  unsigned long now = millis();

  if(swRunning){
    if(swLastTickMs==0) swLastTickMs = now;
    uint32_t dt = (uint32_t)(now - swLastTickMs);
    if(dt > 0){
      swElapsedMs += dt;
      swLastTickMs = now;
    }
  }else{
    swLastTickMs = now;
  }

  if(now - lastUpd >= 1000){
    lastUpd = now;
    uint32_t sec = swElapsedMs/1000;
    if(menuState == MAIN && (sec != swLastDrawnSec || menuIndex==7)){
      drawTileStopwatch();
      swLastDrawnSec = sec;
    }
  }
}
