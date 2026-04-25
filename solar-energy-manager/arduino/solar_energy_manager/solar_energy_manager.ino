/*
 * =====================================================
 *  스마트 태양광 에너지 자동 활용 시스템 (v3)
 *
 *  사용 보드: ESP32
 *  센서 구성:
 *    - CdS 조도센서   : 현장 실측 빛 세기 (아날로그)
 *    - DHT22          : 실내 온습도
 *    - DS3231 RTC     : 정확한 시간 기록
 *    - 1채널 릴레이 × 3개 : 가전제품 제어
 *    - LCD 16x2(I2C)  : 현황 표시
 *    - OpenWeatherMap API : 지역 날씨 데이터 (Wi-Fi)
 *
 *  ┌─────────────────────────────────────────────┐
 *  │         예상 발전량 계산 원리               │
 *  │                                             │
 *  │  발전량 = 최대출력                          │
 *  │         × 현장조도계수  ← CdS 센서 실측    │
 *  │         × 날씨API계수   ← 구름/날씨 상태   │
 *  │         × 시간대계수    ← 태양 고도 보정   │
 *  │                                             │
 *  │  [왜 두 가지를 같이 쓰나요?]               │
 *  │  공식 날씨 데이터는 넓은 지역 평균값이라   │
 *  │  우리 집 옥상의 실제 빛 세기와 다를 수     │
 *  │  있습니다. CdS 센서로 현장값을 측정하고,   │
 *  │  날씨 API로 구름·날씨 추세를 보완하면      │
 *  │  훨씬 정확한 발전량 예측이 가능합니다.     │
 *  └─────────────────────────────────────────────┘
 *
 *  [필수 라이브러리] Arduino IDE > 라이브러리 관리:
 *  1. ArduinoJson         (날씨 API JSON 파싱)
 *  2. DHT sensor library  (온습도)
 *  3. RTClib              (RTC 시계)
 *  4. LiquidCrystal_I2C  (LCD)
 *  5. Firebase ESP Client (by Mobizt)
 *  ※ HTTPClient는 ESP32 기본 내장 → 설치 불필요
 *
 *  [OpenWeatherMap 무료 API 키 발급]
 *  openweathermap.org 회원가입 → API keys 메뉴
 * =====================================================
 */

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <FirebaseESP32.h>
#include <math.h>

// ─── ★ 여기만 수정하세요 ──────────────────────────────────────
const char* WIFI_SSID     = "iptime2.4";
const char* WIFI_PASSWORD = "woaini1123!";
const char* OWM_API_KEY   = "90aa2c67eb363b7ccd92de56ec3182c2";  // openweathermap.org
const char* OWM_CITY      = "Mokpo";              // 현재 도시 (영문)
const char* OWM_COUNTRY   = "KR";
#define FIREBASE_HOST "https://solar-energy-2026-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "rJIEd7eDcRieqGnOS6L9kgeBJN0YfAMUFT1s41O9"
// ─────────────────────────────────────────────────────────────

// ─── 핀 설정 ──────────────────────────────────────────────────
#define CDS_PIN      34   // CdS 조도센서 (아날로그 입력 전용 핀)
                          // ESP32: GPIO34, 35, 36, 39 중 하나 사용
                          // 회로: 3.3V → CdS → GPIO34 → 10kΩ → GND

#define RELAY_1_PIN  26   // 공기청정기 / 에어컨  (300W 이상)
#define RELAY_2_PIN  27   // 세탁기              (600W 이상)
#define RELAY_3_PIN  14   // 식기세척기          (1000W 이상)
#define DHT_PIN       4
#define DHT_TYPE     DHT22

// ─── 시스템 상수 ──────────────────────────────────────────────
#define MAX_POWER_W    2000.0f   // 최대 예상 발전량 (W, 맑은날 정오 기준)
#define CDS_MAX_RAW    4095      // ESP32 ADC 최댓값 (12bit)
#define THRESHOLD_1     300      // 릴레이 작동 기준 (W)
#define THRESHOLD_2     600
#define THRESHOLD_3    1000
#define TEMP_AC         28.0     // 에어컨 우선 작동 온도 (°C)
#define WEATHER_INTERVAL 600000  // 날씨 API 갱신: 10분
#define FIREBASE_INTERVAL  5000  // Firebase 전송: 5초

// ─── 객체 ─────────────────────────────────────────────────────
DHT dht(DHT_PIN, DHT_TYPE);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
FirebaseData fbData;
FirebaseConfig fbConfig;
FirebaseAuth fbAuth;

// ─── 전역 변수 ────────────────────────────────────────────────
// CdS 센서
int   cds_raw          = 0;     // CdS 원시값 (0~4095)
float cds_factor       = 0;     // 현장 조도 계수 (0.0~1.0)

// 날씨 API
int   weather_id       = 800;   // 날씨 코드 (800=맑음)
int   cloud_pct        = 0;     // 구름량 (%)
float outside_temp     = 0;     // 외부 기온 (°C)
String weather_desc    = "맑음";
float weather_factor   = 1.0;   // 날씨 계수 (0.0~1.0)

// 발전량 계산
float time_factor      = 0;     // 시간대 계수 (0.0~1.0)
float power_W          = 0;     // 최종 예상 발전량 (W)

// DHT22
float temperature = 0, humidity = 0;

// 릴레이 (1채널 모듈 3개)
bool relay1State = false, relay2State = false, relay3State = false;

// 타이머
unsigned long lastWeatherTime  = 0;
unsigned long lastFirebaseTime = 0;
unsigned long lastEnergyTime   = 0;
float totalEnergy_Wh = 0;

// ─── 함수 선언 ────────────────────────────────────────────────
void connectWiFi();
bool fetchWeather();
float calcTimeFactor();
void computePower();
void controlRelays();
void readDHT();
void readCds();
void accumulateEnergy();
void updateLCD();
void sendToFirebase();
String getTimeString();

// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== 스마트 태양광 에너지 시스템 v3 ===");
  Serial.println("    CdS 조도센서 + 날씨 API 융합 방식\n");

  int pins[] = {RELAY_1_PIN, RELAY_2_PIN, RELAY_3_PIN};
  for (int p : pins) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); }

  Wire.begin();
  dht.begin();
  Serial.println("✅ DHT22 OK");

  if (rtc.begin()) {
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // 최초 시간 설정 시 주석 해제
    Serial.println("✅ DS3231 RTC OK");
  }

  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Solar v3        ");
  lcd.setCursor(0,1); lcd.print("CdS + API Mode  ");
  delay(1200);

  connectWiFi();

  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);
  Serial.println("✅ Firebase OK");

  fetchWeather(); // 첫 날씨 수신

  lastEnergyTime = lastWeatherTime = lastFirebaseTime = millis();

  Serial.println("\n[현장조도] [날씨계수] [시간계수] → [발전량W] | 작동가전");
  Serial.println("──────────────────────────────────────────────────────");
}

// ═══════════════════════════════════════════════════════════════
void loop() {
  // 10분마다 날씨 API 갱신
  if (millis() - lastWeatherTime >= WEATHER_INTERVAL) {
    fetchWeather();
    lastWeatherTime = millis();
  }

  readCds();        // CdS 현장 조도 읽기
  readDHT();        // 실내 온습도 읽기
  computePower();   // 발전량 계산 (CdS + API + 시간 융합)
  controlRelays();  // 릴레이 제어
  accumulateEnergy();
  updateLCD();

  if (millis() - lastFirebaseTime >= FIREBASE_INTERVAL) {
    sendToFirebase();
    lastFirebaseTime = millis();
  }
  delay(500);
}

// ─── CdS 조도센서 읽기 ────────────────────────────────────────
void readCds() {
  // ESP32 ADC: 0(어두움) ~ 4095(밝음)
  // CdS 특성: 빛이 강할수록 저항↓ → 전압↑ → ADC값↑
  cds_raw = analogRead(CDS_PIN);

  // 0.0 ~ 1.0 으로 정규화
  // constrain: 노이즈로 범위 벗어나는 값 방지
  cds_factor = constrain((float)cds_raw / CDS_MAX_RAW, 0.0, 1.0);
}

// ─── DHT22 읽기 ───────────────────────────────────────────────
void readDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;
}

// ─── 시간대 계수 계산 (태양 고도 근사) ──────────────────────
float calcTimeFactor() {
  DateTime now = rtc.now();
  int hour = now.hour();
  int minute = now.minute();
  float h = hour + minute / 60.0;

  // 오전 6시~오후 6시 사이만 발전 가능
  // sin 함수로 정오(12시)에 최대값 1.0, 일출/일몰에 0이 되도록 근사
  if (h < 6.0 || h > 18.0) return 0.0;

  // sin( (h-6) / 12 × π ) → 6시=0, 12시=1, 18시=0
  float angle = ((h - 6.0) / 12.0) * PI;
  return constrain(sin(angle), 0.0, 1.0);
}

// ─── 핵심: 발전량 통합 계산 ──────────────────────────────────
void computePower() {
  time_factor = calcTimeFactor();

  // ┌────────────────────────────────────────────────┐
  // │  세 가지 계수를 곱해서 최종 발전량 계산        │
  // │                                                │
  // │  cds_factor    : 지금 이 자리의 실제 빛 세기  │
  // │  weather_factor: 날씨 API의 구름/상태 반영    │
  // │  time_factor   : 아침/낮/저녁 태양 고도 보정  │
  // │                                                │
  // │  세 값을 단순 평균하지 않고 곱하는 이유:      │
  // │  하나라도 0에 가까우면 발전이 안 되기 때문    │
  // │  (예: 밤이면 time_factor=0 → 발전량=0)       │
  // └────────────────────────────────────────────────┘
  power_W = MAX_POWER_W * cds_factor * weather_factor * time_factor;
  power_W = constrain(power_W, 0, MAX_POWER_W);

  int cnt = relay1State + relay2State + relay3State;
  Serial.printf("  CdS:%.2f  날씨:%.2f  시간:%.2f  →  %.0fW  | 가전%d개\n",
                cds_factor, weather_factor, time_factor, power_W, cnt);
}

// ─── 릴레이 단계별 제어 ──────────────────────────────────────
void controlRelays() {
  bool hot = (temperature >= TEMP_AC);

  auto setRelay = [](int pin, bool &state, bool newState, const char* name) {
    if (newState == state) return;
    state = newState;
    digitalWrite(pin, state ? LOW : HIGH);
    Serial.printf("    → %s %s\n", name, state ? "켜짐 ✅" : "꺼짐 ⬜");
  };

  setRelay(RELAY_1_PIN, relay1State, power_W >= THRESHOLD_1,
           hot ? "에어컨" : "공기청정기");
  setRelay(RELAY_2_PIN, relay2State, power_W >= THRESHOLD_2, "세탁기");
  setRelay(RELAY_3_PIN, relay3State, power_W >= THRESHOLD_3, "식기세척기");
}

// ─── 누적 발전량 ─────────────────────────────────────────────
void accumulateEnergy() {
  unsigned long now = millis();
  totalEnergy_Wh += power_W * (now - lastEnergyTime) / 3600000.0;
  lastEnergyTime = now;
}

// ─── LCD 표시 ────────────────────────────────────────────────
void updateLCD() {
  lcd.clear();
  // 1행: 예상발전량 + 구름량
  lcd.setCursor(0,0);
  lcd.print(String((int)power_W) + "W  " + String(cloud_pct) + "%" + weather_desc.substring(0,2));
  // 2행: 가전 작동 수 + 실내온도
  lcd.setCursor(0,1);
  int cnt = relay1State + relay2State + relay3State;
  lcd.print("가전:" + String(cnt) + "개 " + String(temperature,0) + "C");
}

// ─── OpenWeatherMap API 호출 ─────────────────────────────────
bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = "http://api.openweathermap.org/data/2.5/weather?q=";
  url += String(OWM_CITY) + "," + String(OWM_COUNTRY);
  url += "&appid=" + String(OWM_API_KEY);
  url += "&units=metric&lang=kr";

  HTTPClient http;
  http.begin(url);
  int code = http.GET();

  if (code != 200) {
    Serial.println("⚠️ 날씨 API 오류 " + String(code) + " → 이전 값 유지");
    http.end();
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();

  if (err) {
    Serial.println("⚠️ JSON 파싱 실패: " + String(err.c_str()));
    return false;
  }

  weather_id   = doc["weather"][0]["id"].as<int>();
  cloud_pct    = doc["clouds"]["all"].as<int>();
  outside_temp = doc["main"]["temp"].as<float>();
  weather_desc = doc["weather"][0]["description"].as<String>();

  // ── 날씨 코드별 계수 설정 ──────────────────────────────────
  // OpenWeatherMap 날씨 코드 체계:
  //  800       : 맑음
  //  801       : 구름 조금 (11~25%)
  //  802       : 구름 약간 (25~50%)
  //  803~804   : 흐림
  //  7xx       : 안개/연무
  //  6xx       : 눈
  //  5xx       : 비
  //  2xx~3xx   : 뇌우/이슬비
  if      (weather_id == 800)                     weather_factor = 1.00;
  else if (weather_id == 801)                     weather_factor = 0.80;
  else if (weather_id == 802)                     weather_factor = 0.60;
  else if (weather_id >= 803 && weather_id <= 804)weather_factor = 0.35;
  else if (weather_id >= 700 && weather_id < 800) weather_factor = 0.40;
  else if (weather_id >= 600 && weather_id < 700) weather_factor = 0.20;
  else if (weather_id >= 500 && weather_id < 600) weather_factor = 0.10;
  else                                            weather_factor = 0.05;

  // 구름량으로 추가 미세 보정 (구름 50% → 계수 25% 추가 감소)
  weather_factor *= (1.0 - (cloud_pct / 100.0) * 0.5);
  weather_factor  = constrain(weather_factor, 0.05, 1.0);

  Serial.printf("☁️ 날씨 갱신: %s (ID:%d) 구름:%d%% 외부기온:%.1f°C 계수:%.2f\n",
                weather_desc.c_str(), weather_id, cloud_pct, outside_temp, weather_factor);
  return true;
}

// ─── Wi-Fi 연결 ──────────────────────────────────────────────
void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("WiFi 연결 중...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Wi-Fi 연결");
  for (int i = 0; WiFi.status() != WL_CONNECTED && i < 20; i++) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" ✅ " + WiFi.localIP().toString());
    lcd.setCursor(0,1); lcd.print("WiFi OK!        ");
  } else {
    Serial.println(" ⚠️ 실패 → 오프라인 모드");
    lcd.setCursor(0,1); lcd.print("WiFi 실패-오프라인");
  }
  delay(800);
}

// ─── Firebase 전송 ───────────────────────────────────────────
void sendToFirebase() {
  if (WiFi.status() != WL_CONNECTED) return;
  String t    = getTimeString();
  String base = "/solarData/realtime";
  String hist = "/solarData/history/" + t;

  Firebase.setFloat(fbData,  base + "/power_W",        power_W);
  Firebase.setFloat(fbData,  base + "/cds_factor",     cds_factor);
  Firebase.setFloat(fbData,  base + "/cds_raw",        cds_raw);
  Firebase.setFloat(fbData,  base + "/weather_factor", weather_factor);
  Firebase.setFloat(fbData,  base + "/time_factor",    time_factor);
  Firebase.setInt(fbData,    base + "/cloud_pct",      cloud_pct);
  Firebase.setInt(fbData,    base + "/weather_id",     weather_id);
  Firebase.setString(fbData, base + "/weather_desc",   weather_desc);
  Firebase.setFloat(fbData,  base + "/outside_temp",   outside_temp);
  Firebase.setFloat(fbData,  base + "/temperature",    temperature);
  Firebase.setFloat(fbData,  base + "/humidity",       humidity);
  Firebase.setFloat(fbData,  base + "/totalEnergy_Wh", totalEnergy_Wh);
  Firebase.setBool(fbData,   base + "/relay1",         relay1State);
  Firebase.setBool(fbData,   base + "/relay2",         relay2State);
  Firebase.setBool(fbData,   base + "/relay3",         relay3State);
  Firebase.setString(fbData, base + "/updatedAt",      t);

  Firebase.setFloat(fbData, hist + "/power_W",        power_W);
  Firebase.setFloat(fbData, hist + "/cds_factor",     cds_factor);
  Firebase.setFloat(fbData, hist + "/weather_factor", weather_factor);
  Firebase.setInt(fbData,   hist + "/cloud_pct",      cloud_pct);
  Firebase.setBool(fbData,  hist + "/relay1",         relay1State);
  Firebase.setBool(fbData,  hist + "/relay2",         relay2State);
  Firebase.setBool(fbData,  hist + "/relay3",         relay3State);

  Serial.println("    📡 Firebase 전송: " + t);
}

// ─── 시간 문자열 ─────────────────────────────────────────────
String getTimeString() {
  DateTime now = rtc.now();
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d",
           now.year(), now.month(), now.day(), now.hour(), now.minute());
  return String(buf);
}
