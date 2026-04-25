# ☀️ 스마트 태양광 에너지 자동 활용 시스템

> 2026 학생 SW융합 해커톤대회 출품작  
> 전라남도교육청창의융합교육원

## 📌 작품 개요

태양광 발전 전기를 한전이 매입하지 않아 낭비되는 낮 시간 잉여 전력을, **집에 사람이 없어도 자동으로 가전제품을 작동시켜** 에너지 손실을 방지하는 스마트 에너지 관리 시스템입니다.

---

## 🗂️ 프로젝트 구조

```
📁 solar-energy-manager/
├── 📁 arduino/
│   └── solar_energy_manager/
│       └── solar_energy_manager.ino   ← 아두이노 코드
├── 📁 webapp/
│   └── index.html                     ← 웹 대시보드 (Firebase 연동)
├── 📄 README.md
└── 📄 sensor_list.md                  ← 필요 부품 목록
```

---

## 🔧 필요 부품 (센서 목록)

| 부품명 | 수량 | 역할 |
|--------|------|------|
| ESP32 개발보드 | 1 | 메인 컨트롤러 + Wi-Fi |
| INA219 전류·전압 센서 | 1 | 태양광 발전량 실시간 측정 |
| 4채널 5V 릴레이 모듈 | 1 | 가전제품 4종 ON/OFF 제어 |
| DHT22 온습도 센서 | 1 | 실내 온도에 따른 우선 가전 결정 |
| DS3231 RTC 모듈 | 1 | 정확한 시간 기록 |
| 16×2 LCD (I2C) | 1 | 현장 발전량·상태 표시 |
| 브레드보드, 점퍼선 | 일식 | 회로 구성 |

---

## 🔌 배선 (핀 연결)

### INA219 (I2C)
| INA219 핀 | ESP32 핀 |
|-----------|----------|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### DHT22
| DHT22 핀 | ESP32 핀 |
|----------|----------|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO 4 |

### DS3231 RTC (I2C)
| DS3231 핀 | ESP32 핀 |
|-----------|----------|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 (공유) |
| SCL | GPIO 22 (공유) |

### 4채널 릴레이 모듈
| 릴레이 채널 | ESP32 핀 | 연결 가전 |
|------------|----------|-----------|
| IN1 | GPIO 26 | 공기청정기/에어컨 |
| IN2 | GPIO 27 | 세탁기 |
| IN3 | GPIO 14 | 식기세척기 |
| IN4 | GPIO 12 | 전기온수기 |
| VCC | 5V | |
| GND | GND | |

### LCD 16x2 (I2C)
| LCD 핀 | ESP32 핀 |
|--------|----------|
| VCC | 5V |
| GND | GND |
| SDA | GPIO 21 (공유) |
| SCL | GPIO 22 (공유) |

---

## 🚀 시작하기

### 1단계: Firebase 프로젝트 생성
1. [Firebase Console](https://console.firebase.google.com) 접속
2. 새 프로젝트 생성
3. **Realtime Database** 활성화 (테스트 모드)
4. 프로젝트 설정 > 일반에서 **API 키, 프로젝트 ID** 확인
5. Realtime Database URL 확인 (`your-project.firebaseio.com`)

### 2단계: 아두이노 코드 설정
`arduino/solar_energy_manager/solar_energy_manager.ino` 파일을 열어 아래 항목 수정:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";      // ← 본인 Wi-Fi 이름
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // ← 본인 Wi-Fi 비밀번호
#define FIREBASE_HOST "your-project-id.firebaseio.com"  // ← Firebase URL
#define FIREBASE_AUTH "YOUR_FIREBASE_DATABASE_SECRET"   // ← Firebase 시크릿 키
```

### 3단계: 라이브러리 설치
Arduino IDE > 툴 > 라이브러리 관리에서 설치:
- `Adafruit INA219`
- `DHT sensor library`
- `RTClib`
- `LiquidCrystal_I2C`
- `Firebase ESP Client` (by Mobizt)

### 4단계: 업로드
ESP32를 PC에 연결 후 **업로드** 버튼 클릭

### 5단계: 웹앱 실행
`webapp/index.html`을 브라우저로 열고 Firebase 정보 입력 후 연결

---

## 📊 작동 원리 (단계별 가전 제어)

```
발전량 300W 이상  → 공기청정기 (또는 에어컨) ON
발전량 600W 이상  → 공기청정기 + 세탁기 ON
발전량 1,000W 이상 → 공기청정기 + 세탁기 + 식기세척기 ON  
발전량 1,500W 이상 → 공기청정기 + 세탁기 + 식기세척기 + 전기온수기 ON
발전량 감소       → 역순으로 가전제품 OFF
```

---

## 🌿 기대 효과

- 낮 시간 잉여 발전 전력 활용률: 0% → **최대 70%**
- 전기요금 절감: 평균 가정 기준 월 **2~4만원** 절약 예상
- CO₂ 절감: 1kWh당 **0.4781kg CO₂** 감소

---

## 👨‍🏫 팀 정보

- **출품학교**: (학교명 입력)
- **지도교사**: (교사명 입력)  
- **팀원**: (학생 이름 입력)
- **학교급**: 고등학교

---

## 📝 라이선스

본 작품의 저작권은 출품자에게 있으며, 교육적 목적으로 자유롭게 활용 가능합니다.
