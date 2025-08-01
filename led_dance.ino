#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// LED Matrix pins
const int CS_PIN = 5;

WebServer server(80);

// MAX7219 registers
const int MAX7219_REG_NOOP = 0x00;
const int MAX7219_REG_DIGIT0 = 0x01;
const int MAX7219_REG_DIGIT1 = 0x02;
const int MAX7219_REG_DIGIT2 = 0x03;
const int MAX7219_REG_DIGIT3 = 0x04;
const int MAX7219_REG_DIGIT4 = 0x05;
const int MAX7219_REG_DIGIT5 = 0x06;
const int MAX7219_REG_DIGIT6 = 0x07;
const int MAX7219_REG_DIGIT7 = 0x08;
const int MAX7219_REG_DECODEMODE = 0x09;
const int MAX7219_REG_INTENSITY = 0x0A;
const int MAX7219_REG_SCANLIMIT = 0x0B;
const int MAX7219_REG_SHUTDOWN = 0x0C;
const int MAX7219_REG_DISPLAYTEST = 0x0F;

// 8x8 LED matrix state
byte matrix[8] = {0};

void setup() {
  Serial.begin(115200);
  
  // Initialize SPI
  SPI.begin();
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  
  // Initialize MAX7219
  initMatrix();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Web server routes
  server.on("/", handleRoot);
  server.on("/set", handleSetLED);
  server.on("/clear", handleClear);
  server.on("/pattern", handlePattern);
  server.enableCORS(true);
  
  server.begin();
  Serial.println("Web server started");
  
  // Test pattern
  testMatrix();
}

void loop() {
  server.handleClient();
}

void sendToMatrix(int reg, int data) {
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(reg);
  SPI.transfer(data);
  digitalWrite(CS_PIN, HIGH);
}

void initMatrix() {
  // Initialize MAX7219
  sendToMatrix(MAX7219_REG_SHUTDOWN, 0x01);      // Exit shutdown mode
  sendToMatrix(MAX7219_REG_DISPLAYTEST, 0x00);   // Disable display test
  sendToMatrix(MAX7219_REG_DECODEMODE, 0x00);    // No decode mode
  sendToMatrix(MAX7219_REG_SCANLIMIT, 0x07);     // Scan all 8 digits
  sendToMatrix(MAX7219_REG_INTENSITY, 0x08);     // Set brightness (0-15)
  
  // Clear display
  for (int i = 0; i < 8; i++) {
    sendToMatrix(MAX7219_REG_DIGIT0 + i, 0x00);
    matrix[i] = 0;
  }
}

void testMatrix() {
  // Simple test pattern
  Serial.println("Testing LED Matrix...");
  
  // Light up diagonal
  for (int i = 0; i < 8; i++) {
    matrix[i] = 1 << i;
    sendToMatrix(MAX7219_REG_DIGIT0 + i, matrix[i]);
    delay(200);
  }
  
  delay(1000);
  
  // Clear
  for (int i = 0; i < 8; i++) {
    matrix[i] = 0;
    sendToMatrix(MAX7219_REG_DIGIT0 + i, 0);
  }
  
  Serial.println("Matrix test complete!");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>ESP32 LED Matrix</title></head><body>";
  html += "<h1>ESP32 LED Matrix Controller</h1>";
  html += "<p>Matrix is ready!</p>";
  html += "<p>Use the web interface to control LEDs</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSetLED() {
  if (server.hasArg("row") && server.hasArg("col") && server.hasArg("state")) {
    int row = server.arg("row").toInt();
    int col = server.arg("col").toInt();
    bool state = server.arg("state").toInt();
    
    if (row >= 0 && row < 8 && col >= 0 && col < 8) {
      // Fix orientation - flip both row and column
      int actualRow = 7 - row;  // Vertical flip
      int actualCol = 7 - col;  // Horizontal flip
      
      if (state) {
        matrix[actualRow] |= (1 << actualCol);
      } else {
        matrix[actualRow] &= ~(1 << actualCol);
      }
      sendToMatrix(MAX7219_REG_DIGIT0 + actualRow, matrix[actualRow]);
      server.send(200, "text/plain", "OK");
      
      Serial.print("LED ");
      Serial.print(row);
      Serial.print(",");
      Serial.print(col);
      Serial.print(" -> ");
      Serial.print(actualRow);
      Serial.print(",");
      Serial.print(actualCol);
      Serial.print(" = ");
      Serial.println(state ? "ON" : "OFF");
    } else {
      server.send(400, "text/plain", "Invalid coordinates");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleClear() {
  for (int i = 0; i < 8; i++) {
    matrix[i] = 0;
    sendToMatrix(MAX7219_REG_DIGIT0 + i, 0);
  }
  server.send(200, "text/plain", "Cleared");
  Serial.println("Matrix cleared");
}

void handlePattern() {
  if (server.hasArg("data")) {
    String data = server.arg("data");
    
    for (int row = 0; row < 8; row++) {
      matrix[row] = 0;
      for (int col = 0; col < 8; col++) {
        int index = row * 8 + col;
        if (index < data.length() && data[index] == '1') {
          matrix[row] |= (1 << col);
        }
      }
      sendToMatrix(MAX7219_REG_DIGIT0 + row, matrix[row]);
    }
    server.send(200, "text/plain", "Pattern set");
    Serial.println("Pattern updated");
  } else {
    server.send(400, "text/plain", "No pattern data");
  }
}
