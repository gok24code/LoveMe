#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- AYARLAR ---
const int TOUCH_PIN = 2;   
const int BUZZER_PIN = 3;  
const int SLEEP_TIMEOUT = 15000; // 15 sn hareketsizlikte uyur
const int TAP_THRESHOLD = 500;   // Basılı tutma eşiği
const int DOUBLE_TAP_TIMEOUT = 400; // İkinci tıklama için bekleme süresi (ms)

// --- DEĞİŞKENLER ---
unsigned long touchStartTime = 0;
bool isTouching = false;
bool ignoreNextRelease = false; 

// Çift Tıklama (Double Tap) Değişkenleri
int tapCounter = 0;
unsigned long lastReleaseTime = 0;

// Animasyon ve Konum
float currentPupilX = 0, targetPupilX = 0;  
float currentPupilY = 0, targetPupilY = 0;
float currentEyeRadius = 18, targetEyeRadius = 18;  

// Zamanlayıcılar
unsigned long lastInteractionTime = 0; 
unsigned long lastIdleActionTime = 0;
unsigned long angryStartTime = 0; 
int nextIdleDelay = 1000;
unsigned long lastSoundTime = 0;

// Durumlar
enum State { NORMAL, BLINK, HAPPY, EXCITED, HEART, ANGRY, SLEEP };
State currentState = NORMAL;

void setup() {
  Serial.begin(9600);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  randomSeed(analogRead(0));

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Hatasi"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  lastInteractionTime = millis();
  sfxChirp(); 
}

void loop() {
  int touchState = digitalRead(TOUCH_PIN);
  unsigned long currentTime = millis();

  // --- 1. DOKUNMA ALGILANDI ---
  if (touchState == HIGH) {
    lastInteractionTime = currentTime; 

    // A) Uyuyorsa -> Sadece Uyan
    if (currentState == SLEEP) {
      currentState = NORMAL;
      targetEyeRadius = 18;
      sfxChirp(); 
      ignoreNextRelease = true; 
      isTouching = true; 
      tapCounter = 0; // Sayacı sıfırla
      return; 
    }

    // B) Zaten Kızgınsa -> Bekle
    if (currentState == ANGRY) {
       angryStartTime = currentTime; 
       return; 
    }

    if (!ignoreNextRelease) {
      // C) ÇİFT TIKLAMA YAKALAMA
      // Eğer daha önce bir kere tıkladıysa ve süre dolmadıysa -> Bu ikinci tıklamadır!
      if (tapCounter == 1 && (currentTime - lastReleaseTime < DOUBLE_TAP_TIMEOUT)) {
        currentState = HAPPY; // Çift tık mutlu eder
        targetEyeRadius = 18;
        targetPupilX = 0; targetPupilY = 0;
        sfxHappyShort();
        
        tapCounter = 0; // Sayacı sıfırla
        isTouching = true; // Basılı tutmaya devam ederse diye
        touchStartTime = currentTime; // Süreyi resetle
        return; // İşlem tamam
      }

      // Normal Dokunuş Başlangıcı
      if (!isTouching) {
        touchStartTime = currentTime;
        isTouching = true;
      }

      // Basılı Tutma Kontrolü (Long Press)
      unsigned long duration = currentTime - touchStartTime;
      if (duration > TAP_THRESHOLD) {
        tapCounter = 0; // Basılı tuttuğu için tap sayacını iptal et
        
        // Uzun basma seviyeleri
        targetPupilX = 0; targetPupilY = 0;
        if (duration < 2500) {
          currentState = HAPPY;
          if (currentTime - lastSoundTime > 600) { sfxPurr(); lastSoundTime = currentTime; }
        } else if (duration < 5000) {
          currentState = EXCITED;
          targetEyeRadius = 22;
           if (currentTime - lastSoundTime > 300) { sfxR2D2(); lastSoundTime = currentTime; }
        } else {
           if (currentState != HEART) sfxLoveMelody();
           currentState = HEART;
        }
      }
    }
  } 
  
  // --- 2. DOKUNMA BIRAKILDI ---
  else { 
    if (isTouching) {
      unsigned long duration = currentTime - touchStartTime;
      isTouching = false;

      if (ignoreNextRelease) {
        ignoreNextRelease = false;
      }
      // Kısa Dokunuş (Tap)
      else if (duration < TAP_THRESHOLD && currentState != SLEEP) {
        // Hemen kızmıyoruz! Tıklamayı sayıyoruz.
        tapCounter++;
        lastReleaseTime = currentTime;
      }
    }

    // --- 3. DURUM KONTROLLERİ (BOŞTA) ---

    // A) TEK TIKLAMA KONTROLÜ (ZAMAN AŞIMI)
    // Eğer 1 kere tıkladıysa ve ikinci tıklama için süre dolduysa -> ŞİMDİ KIZABİLİRSİN
    if (tapCounter > 0 && (currentTime - lastReleaseTime > DOUBLE_TAP_TIMEOUT)) {
      if (tapCounter == 1) { // Sadece 1 kere tıkladıysa
        currentState = ANGRY;
        angryStartTime = currentTime;
        targetEyeRadius = 16;
        targetPupilX = random(-3, 4);
        targetPupilY = random(-3, 4);
        tone(BUZZER_PIN, 150, 150);
      }
      tapCounter = 0; // Sayacı sıfırla
    }

    // B) Kızgınlık Süresi
    if (currentState == ANGRY) {
      if (currentTime - angryStartTime > 2000) {
        currentState = NORMAL;
      } else {
         targetPupilX = random(-2, 3); targetPupilY = random(-2, 3);
      }
    }
    // C) Uyku Süresi
    else if (currentTime - lastInteractionTime > SLEEP_TIMEOUT) {
      currentState = SLEEP;
      targetEyeRadius = 0;
      if (currentTime - lastSoundTime > 4000) { sfxSnore(); lastSoundTime = currentTime; }
    }
    // D) Normal Animasyonlar
    else if (currentState == NORMAL || currentState == HAPPY || currentState == HEART || currentState == EXCITED) {
      if (currentState != NORMAL && !isTouching) currentState = NORMAL; 
      if (currentState == NORMAL && currentTime - lastIdleActionTime > nextIdleDelay) playIdleAnimation(currentTime);
      if (currentState == BLINK && currentTime - lastIdleActionTime > 200) currentState = NORMAL;
    }
  }

  // --- ANİMASYON MOTORU ---
  currentPupilX += (targetPupilX - currentPupilX) * 0.2;
  currentPupilY += (targetPupilY - currentPupilY) * 0.2;
  currentEyeRadius += (targetEyeRadius - currentEyeRadius) * 0.2;

  // --- ÇİZİM ---
  display.clearDisplay();
  switch (currentState) {
    case NORMAL:
    case EXCITED:
      drawPupilEyes((int)currentPupilX, (int)currentPupilY, (int)currentEyeRadius);
      if(currentState==EXCITED) display.fillCircle(64,55,6,WHITE);
      break;
    case ANGRY: drawAngryEyes(); break;
    case BLINK: drawBlink(); break;
    case HAPPY: drawHappyFace(); break;
    case HEART: drawHeartEyes(); break;
    case SLEEP: drawSleepFace(); break;
  }
  display.display();
}

// --- GÖRSEL FONKSİYONLAR ---

// KIZGIN GÖZLER (Kaşsız, Üstten Kesik)
void drawAngryEyes() {
  drawPupilEyes((int)currentPupilX, (int)currentPupilY, (int)currentEyeRadius); 
  // Gözlerin üst kısmını siyah üçgenle keserek "düz" kızgın ifade veriyoruz
  display.fillTriangle(15, 10, 60, 26, 60, 10, BLACK); 
  display.fillTriangle(68, 26, 113, 10, 68, 10, BLACK);
}

void drawPupilEyes(int pupX, int pupY, int radius) {
  display.fillCircle(40, 32, radius, WHITE);
  display.fillCircle(88, 32, radius, WHITE);
}

void playIdleAnimation(unsigned long currentTime) {
  int randAction = random(0, 100);
  bool playSound = (random(0, 10) < 3);
  if (randAction < 40) { targetPupilX=0; targetPupilY=0; currentState=NORMAL; } 
  else if (randAction < 60) { targetPupilX=-8; targetPupilY=0; currentState=NORMAL; if(playSound) sfxChirp(); } 
  else if (randAction < 80) { targetPupilX=8; targetPupilY=0; currentState=NORMAL; if(playSound) sfxChirp(); } 
  else { currentState=BLINK; tone(BUZZER_PIN, 1200, 10); }
  lastIdleActionTime = currentTime;
  nextIdleDelay = random(1000, 3500); 
}

void drawSleepFace() {
  display.fillRect(25, 35, 30, 2, WHITE);
  display.fillRect(73, 35, 30, 2, WHITE);
  unsigned long t = millis()/500;
  display.setCursor(110, 20 - (t%3)*5);
  display.setTextSize(1); display.print("z");
}

void drawBlink() {
  display.fillRect(22, 31, 36, 2, WHITE);
  display.fillRect(70, 31, 36, 2, WHITE);
}

void drawHappyFace() {
  display.fillCircle(40, 40, 18, WHITE); display.fillCircle(40, 32, 18, BLACK); 
  display.fillCircle(88, 40, 18, WHITE); display.fillCircle(88, 32, 18, BLACK); 
  display.fillCircle(64, 55, 3, WHITE);
}

void drawHeartEyes() {
  drawHeart(40, 35, 14); drawHeart(88, 35, 14);
  display.fillTriangle(58, 55, 70, 55, 64, 60, WHITE);
}
void drawHeart(int x, int y, int size) {
  display.fillCircle(x-size/2, y-size/2, size/2, WHITE);
  display.fillCircle(x+size/2, y-size/2, size/2, WHITE);
  display.fillTriangle(x-size, y-size/4, x+size, y-size/4, x, y+size, WHITE);
}

// --- SESLER ---
void sfxChirp() { tone(BUZZER_PIN, 2000, 50); }
void sfxHappyShort() { tone(BUZZER_PIN, 1000, 100); delay(100); tone(BUZZER_PIN, 1500, 150); }
void sfxPurr() { for(int i=0; i<3; i++) { tone(BUZZER_PIN, 100+(i*50), 20); delay(25); } }
void sfxR2D2() { int n=random(2,5); for(int i=0;i<n;i++) { tone(BUZZER_PIN, random(1000,3500), random(30,100)); delay(50); } }
void sfxLoveMelody() { tone(BUZZER_PIN,1319,150); delay(150); tone(BUZZER_PIN,1760,150); delay(150); tone(BUZZER_PIN,2093,300); delay(300); }
void sfxSnore() { for(int i=150; i>50; i--) { tone(BUZZER_PIN, i, 10); delay(10); } }