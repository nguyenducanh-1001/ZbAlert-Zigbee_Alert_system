const int pirPin = 6;
const int ledPin = 7;

// Biến volatile dùng trong hàm ngắt
volatile bool motionState = LOW;
volatile bool stateChanged = false;

// Hàm ngắt xử lý tức thì khi có thay đổi trên chân pirPin
void IRAM_ATTR pirISR() {
  motionState = digitalRead(pirPin);
  stateChanged = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(pirPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Kích hoạt ngắt: Bất kỳ sự thay đổi (CHANGE) nào trên chân PIR sẽ gọi hàm pirISR
  attachInterrupt(digitalPinToInterrupt(pirPin), pirISR, CHANGE);

  Serial.println("ESP32-C6 PIR Interrupt Test");
}

void loop() {
  // Chỉ khi trạng thái thay đổi mới cập nhật LED và in Serial
  if (stateChanged) {
    digitalWrite(ledPin, motionState);

    if (motionState == HIGH) {
      Serial.println("Đang có chuyển động");
    } else {
      Serial.println("Không có gì");
    }

    stateChanged = false; // Reset cờ báo
  }
}