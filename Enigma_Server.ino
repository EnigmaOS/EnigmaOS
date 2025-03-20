#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Enigma_Animation.h"

const int BUTTON_PIN = 0;  // GPIO0
bool serverRunning = true;
bool finishAnimation = false;
bool buttonPressed = false;
unsigned long lastButtonPress = 0;
const unsigned long BUTTON_COOLDOWN = 800; // Cooldown de 800ms

const int DISPLAY_MODE_PIN = 35;  // GPIO35
bool isDarkMode = true;  // Começa em dark mode
bool displayModeButtonPressed = false;
unsigned long lastDisplayModePress = 0;
const unsigned long DISPLAY_MODE_COOLDOWN = 500; // Cooldown de 800ms


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define ANIMATION_DELAY 10

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variáveis para controle da animação Enigma de forma não-bloqueante
unsigned long previousMillis = 0;
int currentFrame = 0;
#define SERVER_TIMEOUT 15000

String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".svg")) return "image/svg+xml";
    return "text/plain";
}

bool handleFileRead(String path) {
    Serial.println("handleFileRead: " + path);
    
    if (path.endsWith("/")) {
        path += "index.html";
    }
    
    String contentType = getContentType(path);
    
    if (SPIFFS.exists(path)) {
        File file = SPIFFS.open(path, "r");
        if (!file) {
            Serial.println("Failed to open file: " + path);
            return false;
        }
        
        size_t fileSize = file.size();
        Serial.printf("File size: %d bytes\n", fileSize);
        
        if (fileSize > 0) {
            server.sendHeader("Connection", "close");
            server.streamFile(file, contentType);
            file.close();
            return true;
        }
        file.close();
    }
    
    Serial.println("File Not Found: " + path);
    return false;
}


// Modifique a função handleButton() para:
void handleButton() {
  unsigned long currentTime = millis();
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonPressed && (currentTime - lastButtonPress >= BUTTON_COOLDOWN)) {
      buttonPressed = true;
      lastButtonPress = currentTime;
      
      if (serverRunning) {
        // Parar servidor
        serverRunning = false;
        finishAnimation = true;
        
        // Atualizar display com status Offline
        display.clearDisplay();
        display.setCursor(0,10);
        display.println("Estado: Offline");
        display.display();
        
        server.close();
      } else {
        // Mostrar estado "A iniciar"
        display.clearDisplay();
        display.setCursor(0,10);
        display.println("Estado: A iniciar");
        display.drawBitmap(
            SCREEN_WIDTH - 31,
            SCREEN_HEIGHT - 31,
            Enigma_AnimationallArray[0], // Frame 1 estático
            31, 31, 1);
        display.display();
        
        // Reconectar WiFi
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
          delay(1000);
          Serial.println("Connecting to WiFi..");
        }
        
        // Mostrar status online e IP
        display.clearDisplay();
        display.setCursor(0,10);
        display.println("Estado: Online");
        display.println("");
        display.print("IP: ");
        display.println(WiFi.localIP());
        display.println("");
        display.print("enigma.local");
        display.display();
        
        server.begin();
        serverRunning = true;
        finishAnimation = false;
      }
    }
  } else {
    buttonPressed = false;
  }
}

void updateAnimation() {
    unsigned long currentMillis = millis();
    
    if (currentMillis - previousMillis >= ANIMATION_DELAY) {
        previousMillis = currentMillis;
        
        // Limpa área da animação
        display.fillRect(SCREEN_WIDTH - 31, SCREEN_HEIGHT - 31, 31, 31, SSD1306_BLACK);
        
        // Desenha frame atual
        display.drawBitmap(
            SCREEN_WIDTH - 31,
            SCREEN_HEIGHT - 31,
            Enigma_AnimationallArray[currentFrame],
            31, 31, 1);
            
        display.display();
        
        // Avança para próximo frame
        if (finishAnimation) {
            // Se finishAnimation é true, continua até chegar ao frame 1
            currentFrame = (currentFrame + 1) % Enigma_AnimationallArray_LEN;
            if (currentFrame == 1) {
                finishAnimation = false;
            }
        } else if (serverRunning) {
            // Se servidor está a correr, continua animação normalmente
            currentFrame = (currentFrame + 1) % Enigma_AnimationallArray_LEN;
        }
    }
}


void handleDisplayMode() {
    unsigned long currentTime = millis();
    
    if (digitalRead(DISPLAY_MODE_PIN) == LOW) {
        if (!displayModeButtonPressed && (currentTime - lastDisplayModePress >= DISPLAY_MODE_COOLDOWN)) {
            displayModeButtonPressed = true;
            lastDisplayModePress = currentTime;
            
            // Inverte o modo atual
            isDarkMode = !isDarkMode;
            display.invertDisplay(!isDarkMode);
        }
    } else {
        displayModeButtonPressed = false;
    }
}


void setup() {
  Serial.begin(115200);

  // Initialize OLED display
  Wire.begin(21, 22); // SDA = 21, SCL = 22
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Initialize SPIFFS
if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    
    // Mostrar informação sobre o espaço no SPIFFS
    Serial.printf("Total space: %d bytes\n", SPIFFS.totalBytes());
    Serial.printf("Used space: %d bytes\n", SPIFFS.usedBytes());
    
    // Listar todos os arquivos no SPIFFS
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file) {
        String fileName = file.name();
        size_t fileSize = file.size();
        Serial.printf("FS File: %s, size: %d\n", fileName.c_str(), fileSize);
        file = root.openNextFile();
    }

  display.clearDisplay();
  display.setCursor(4,15);
  display.println("Servidor Enigma");
  display.setCursor(38,42);
  display.println("Powered by");
  display.setCursor(51,51);
  display.println("Enigma");
  display.drawBitmap(SCREEN_WIDTH - 31, SCREEN_HEIGHT - 31, Enigma_AnimationallArray[0], 31, 31, 1);
  display.display();
  delay(1500);
  

  // Show initializing message
    display.clearDisplay();
  display.setCursor(0,10);
  display.println("Estado: A iniciar");
  display.drawBitmap(
      SCREEN_WIDTH - 31,
      SCREEN_HEIGHT - 31,
      Enigma_AnimationallArray[0], // Frame 1 estático
      31, 31, 1);
  display.display();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(DISPLAY_MODE_PIN, INPUT_PULLUP);
  display.invertDisplay(false); 
  isDarkMode = true;

  // Show connected status and IP
  display.clearDisplay();
  display.setCursor(0,10);
  display.println("Status: Online");
  display.println("");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.println("");
  display.print("enigma.local");
  display.display();

  // Print ESP32 Local IP Address
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Setup mDNS
  if (!MDNS.begin("enigma")) {
    Serial.println("Error starting mDNS");
    return;
  }

  // Handle all file requests
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "File Not Found");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
    handleButton();
    handleDisplayMode();
    
    if (serverRunning) {
        server.handleClient();
    }
    if (serverRunning || finishAnimation) {
        updateAnimation();
    }
}