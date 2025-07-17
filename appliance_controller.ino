#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <time.h>

#ifndef STASSID
#define STASSID "ObieConnect"
#define STAPSK "122ElmStreet"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

const int relayPins[] = {4, 5, 18, 19, 21, 22, 23, 25};
const int numRelays = sizeof(relayPins) / sizeof(relayPins[0]);

// Global off time for relays *other than* the lava lamp
const int offStartHour = 0; // 12 AM
const int offEndHour = 6;   // 6 AM

// Lava lamp schedule: ON from 12 AM to 9 AM
const int lavaLampOnStart = 0;  // 12 AM
const int lavaLampOnEnd = 9;    // 9 AM

WebServer server(80);
volatile bool inactive = false;

// State tracking to prevent constant override of manual controls
bool relayStates[8] = {false}; // Track current relay states
bool manualOverride[8] = {false}; // Track if user manually controlled relay
unsigned long lastScheduleCheck = 0;
const unsigned long scheduleCheckInterval = 60000; // Check schedule every minute

// Time zone configuration (EST/EDT - adjust as needed)
const long gmtOffset_sec = -5 * 3600;  // EST is GMT-5
const int daylightOffset_sec = 3600;   // DST adds 1 hour

const char* htmlContent = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
<title>Choose Appliance</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
body { font-family: Arial, sans-serif; background: linear-gradient(135deg, #1f4037, #99f2c8); color: #fff; text-align: center; margin: 0; padding: 10px; }
h1 { margin: 20px 0; }
.button { display: block; margin: 10px auto; padding: 15px 30px; font-size: 16px; width: 32ch; background-color: #f39c12; color: white; border: none; border-radius: 5px; cursor: pointer; box-sizing: border-box; text-align: left; overflow: hidden; white-space: nowrap; text-overflow: ellipsis;}
.active { background-color: #2ecc71 !important; }
#offButton { background-color: #3498db; text-align: center; }
#inactivity-overlay { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background-color: rgba(20, 40, 31, 0.9); z-index: 1000; justify-content: center; align-items: center; flex-direction: column; }
#inactivity-overlay .message { font-size: 2.5em; color: #fff; text-align: center; padding: 20px; }
.schedule-info { font-size: 0.9em; margin: 5px 0; color: #ccc; }
</style>
<script>
let inactivityTimer;
function handleInactivity() {
  const overlay = document.getElementById('inactivity-overlay');
  overlay.style.display = 'flex';
  setTimeout(turnPowerOff, 2500);
}
function resetInactivityTimer() {
  clearTimeout(inactivityTimer);
  inactivityTimer = setTimeout(handleInactivity, 300000); // 5 minutes
}
function updateRelayStates() {
  fetch('/relayStates').then(response => response.json()).then(states => {
    for (let i = 0; i < states.length; i++) {
      const button = document.getElementById('relay' + (i + 1));
      if (states[i]) {
        button.classList.add('active');
      } else {
        button.classList.remove('active');
      }
    }
  });
}

function controlRelay(relayNum) {
  fetch('/relay?num=' + relayNum).then(() => { updateRelayStates(); });
}
 function turnPowerOff() {
   const offButton = document.getElementById('offButton');
   if (offButton) offButton.classList.add('active');
   fetch('/turnOffActive').then(() => {
     window.location.href = "https://test-drab-five-93.vercel.app/";
   });
 }
 window.addEventListener('load', () => {
   updateRelayStates();
   setInterval(updateRelayStates, 1000);
   resetInactivityTimer();
  ['mousemove', 'mousedown', 'click', 'keypress', 'scroll', 'touchstart'].forEach(event => {
    document.addEventListener(event, resetInactivityTimer, true);
  });
});
</script>
</head>
<body>
<h1>Choose an Appliance!</h1>
<div class="schedule-info">Lava Lamp Auto-On: 12:00 AM - 9:00 AM | Global Off: 12:00 AM - 6:00 AM</div>
<img src="/ElectricityButton.gif" alt="Electricity Button" style="max-width: 20%; height: auto; margin-top: 20px;" />
<button id="relay1" class="button" onclick="controlRelay(1)">1. Lava Lamp</button>
<button id="relay2" class="button" onclick="controlRelay(2)">2. Light-Incandescent</button>
<button id="relay3" class="button" onclick="controlRelay(3)">3. Light-Compact Flourescent</button>
<button id="relay4" class="button" onclick="controlRelay(4)">4. Light-LED</button>
<button id="relay5" class="button" onclick="controlRelay(5)">5. Hair Dryer</button>
<button id="relay6" class="button" onclick="controlRelay(6)">6. Mini-Fridge (Heat Pump)</button>
<button id="relay7" class="button" onclick="controlRelay(7)">7. Meters & Fairy Lights</button>
<button id="relay8" class="button" onclick="controlRelay(8)">8. Coffee Maker</button>
<button id="offButton" class="button" onclick="turnPowerOff()">Return to Carbon Neutral Stories</button>
<div id="inactivity-overlay">
  <div class="message">Returning to Carbon Neutral Stories...</div>
</div>
</body>
</html>
)rawliteral";

void turnOffAllRelays() {
  for (int i = 0; i < numRelays; i++) {
    digitalWrite(relayPins[i], HIGH);
    relayStates[i] = false;
    manualOverride[i] = false; // Reset manual override when turning all off
  }
  Serial.println("All relays are OFF");
}

// Get current local time
struct tm getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  return *timeinfo;
}

// Check if current time is within lava lamp schedule
bool isLavaLampScheduleTime() {
  struct tm currentTime = getCurrentTime();
  int hour = currentTime.tm_hour;
  return (hour >= lavaLampOnStart && hour < lavaLampOnEnd);
}

// Check if current time is within global off schedule
bool isGlobalOffTime() {
  struct tm currentTime = getCurrentTime();
  int hour = currentTime.tm_hour;
  return (hour >= offStartHour && hour < offEndHour);
}

// Apply time-based schedules with improved logic
void applyTimeSchedules() {
  // Only check schedules periodically to avoid constant override
  if (millis() - lastScheduleCheck < scheduleCheckInterval) {
    return;
  }
  lastScheduleCheck = millis();

  struct tm currentTime = getCurrentTime();
  int hour = currentTime.tm_hour;
  int minute = currentTime.tm_min;
  
  Serial.printf("Schedule check - Time: %02d:%02d\n", hour, minute);

  // Rule 1 (Highest Priority): Lava Lamp Schedule (12:00 AM to 8:59 AM)
  if (isLavaLampScheduleTime()) {
    if (!relayStates[0] && !manualOverride[0]) {
      digitalWrite(relayPins[0], LOW); // Turn Lava Lamp ON
      relayStates[0] = true;
      Serial.println("Auto: Lava Lamp turned ON (scheduled time)");
    }
  }

  // Rule 2: Global Off Time (12:00 AM to 5:59 AM) - affects all except lava lamp
  if (isGlobalOffTime()) {
    for (int i = 1; i < numRelays; i++) { // Start from 1 to skip lava lamp
      if (relayStates[i] && !manualOverride[i]) {
        digitalWrite(relayPins[i], HIGH); // Turn OFF
        relayStates[i] = false;
        Serial.printf("Auto: Relay %d turned OFF (global off time)\n", i + 1);
      }
    }
  }

  // Reset manual override flags after schedule periods end
  if (!isLavaLampScheduleTime()) {
    manualOverride[0] = false; // Reset lava lamp override
  }
  if (!isGlobalOffTime()) {
    for (int i = 1; i < numRelays; i++) {
      manualOverride[i] = false; // Reset other relays override
    }
  }
}

void handleRoot() {
  server.send(200, "text/html", htmlContent);
}



void handleRelayControl() {
  if (inactive) {
    server.send(403, "text/plain", "System is inactive");
    return;
  }
  
  if (server.hasArg("num")) {
    int relayNum = server.arg("num").toInt() - 1;
    if (relayNum >= 0 && relayNum < numRelays) {
      // Toggle relay state
      relayStates[relayNum] = !relayStates[relayNum];
      digitalWrite(relayPins[relayNum], relayStates[relayNum] ? LOW : HIGH);
      
      // Set manual override flag
      manualOverride[relayNum] = true;
      
      String action = relayStates[relayNum] ? "ON" : "OFF";
      Serial.printf("Manual: Relay %d turned %s\n", relayNum + 1, action.c_str());
      server.send(200, "text/plain", "Relay " + String(relayNum + 1) + " turned " + action);
    } else {
      server.send(400, "text/plain", "Invalid relay number");
    }
  } else {
    server.send(400, "text/plain", "Relay number not specified");
  }
}

void handleRelayStates() {
  String json = "[";
  for (int i = 0; i < numRelays; i++) {
    // Update relay states from actual pin states
    relayStates[i] = (digitalRead(relayPins[i]) == LOW);
    json += relayStates[i] ? "true" : "false";
    if (i < numRelays - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleStatus() {
  struct tm currentTime = getCurrentTime();
  char timeStr[64];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &currentTime);
  
  String status = "{\n";
  status += "  \"wifi_connected\": " + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",\n";
  status += "  \"ip_address\": \"" + WiFi.localIP().toString() + "\",\n";
  status += "  \"mac_address\": \"" + WiFi.macAddress() + "\",\n";
  status += "  \"signal_strength\": " + String(WiFi.RSSI()) + ",\n";
  status += "  \"uptime\": " + String(millis()) + ",\n";
  status += "  \"free_heap\": " + String(ESP.getFreeHeap()) + ",\n";
  status += "  \"current_time\": \"" + String(timeStr) + "\",\n";
  status += "  \"lava_lamp_schedule_active\": " + String(isLavaLampScheduleTime() ? "true" : "false") + ",\n";
  status += "  \"global_off_active\": " + String(isGlobalOffTime() ? "true" : "false") + "\n";
  status += "}";
  server.send(200, "application/json", status);
}

void handleInactive() {
  inactive = true;
  turnOffAllRelays();
  Serial.println("System set to inactive - all relays turned off");
  server.send(200, "text/plain", "System is now inactive");
}

void handleTurnOffActive() {
  turnOffAllRelays();
  server.send(200, "text/plain", "All active relays turned OFF");
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Relay Controller Starting...");

  // Initialize relay pins and states
  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Start with all relays OFF
    relayStates[i] = false;
    manualOverride[i] = false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiTimeout < 30000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWi-Fi connection failed!");
    return;
  }
  
  Serial.println("\nWi-Fi connected successfully!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Configure time with proper timezone
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  
  Serial.print("Waiting for time synchronization");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTime synchronized!");
  
  // Print current time for verification
  struct tm currentTime = getCurrentTime();
  char timeStr[64];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S %Z", &currentTime);
  Serial.printf("Current local time: %s\n", timeStr);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/relay", handleRelayControl);
  server.on("/relayStates", handleRelayStates);
  server.on("/status", handleStatus);
  server.on("/inactive", handleInactive);
  server.on("/turnOffActive", handleTurnOffActive);
  server.onNotFound(handleNotFound);

  // Serve static files
  server.on("/ElectricityButton.gif", HTTP_GET, []() {
    File file = SPIFFS.open("/ElectricityButton.gif", "r");
    if (file) {
      server.streamFile(file, "image/gif");
      file.close();
    } else {
      server.send(404, "text/plain", "File not found");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
  
  // Initial schedule check
  lastScheduleCheck = 0; // Force immediate check
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Attempting to reconnect...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  server.handleClient();

  // If system is set to inactive, keep everything off
  if (inactive) {
    turnOffAllRelays();
    delay(1000);
    return;
  }

  // Apply automated time-based rules (with rate limiting)
  applyTimeSchedules();
  
  delay(100); // Small delay to prevent excessive CPU usage
} 