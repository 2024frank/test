#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <time.h> // For NTP time functions

// Set Wifi network and password for controller
// The original ESP32 used for this is named "ORB036" Mac:a842e3a9d9d0
// Oberlin College WiFi Network: ObieConnect, Password: 122ElmStreet, IP: 10.17.192.136
// Petersen Home WiFi Network: London, Password: Petersen, IP: 192.168.0.236 
#ifndef STASSID
#define STASSID "ObieConnect"
#define STAPSK "122ElmStreet"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

// Relay pin connections - The order ESP32 outputs ("D4", "D5"... etc) that wire to the inputs of the 8 relay module from relay 1-8   
const int relayPins[] = {4, 5, 18, 19, 21, 22, 23, 25};
const int numRelays = sizeof(relayPins) / sizeof(relayPins[0]);

// Time each relay is on in milliseconds from relay 1 through relay 8 (e.g. 5000 - 5 seconds)
// Only one relay is on at a time and the relay's advance in order in a cycle. 
// The web controller allows any relay to be selected, but the time it is on is the same before it advances to the next relay
unsigned long onDurations[] = {60000, 10000, 10000, 10000, 7000, 30000, 10000, 10000};

// Time when all relay switching is turned off at night to save energy time in GMT hours (0 = midnight GMT).  
// Subtract 5 hours from ET to get GMT. Example: offStartHour = 5 means turn off at 12:00 midnight ET, offEndHour = 11 means turn on at 6:00 am
const int offStartHour = 0; // 0000 hrs GMT
const int offEndHour = 6;   // 0600 hrs GMT

// Code below is for the phone-optimized web page that shows relay status and allows each relay to be selected by the user
// This webpage is accessible by typing ESP32's IP# into a browser

WebServer server(80);

volatile bool webInteraction = false;
volatile int selectedRelay = -1;
unsigned long interactionStart = 0;
bool inDefaultLoop = true;

int lastActivatedRelay = -1; // Tracks the last activated relay

const char* htmlContent = "<!DOCTYPE HTML>"
                          "<html>"
                          "<head>"
                          //This sets the title of the webpage that appears in the browser tab
                          "<title>Choose Appliance</title>"
                          //This meta tag ensures that the webpage is responsive on mobile devices. 
                          //It tells the browser to set the width of the page to match the device's screen width 
                          //and maintain the initial zoom level.
                          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                          "<style>"
                          //This property sets the font for the text within the body element.
                          //Arial is set as the first choice for the font. 
                          //If for any reason this font isn't available, the browser will use sans-serif as a fallback
                          //sans-serif is a general font family type that doesn't have the decorative strokes at the ends of letters (called "serifs")
                          //"linear-gradient(135deg, #1f4037, #99f2c8)" Sets the background for the entire body of the page. 
                          //The gradient creates a smooth gradient from 1st color to 2nd
                          //#1f4037: The first color in the gradient (dark green).
                          //#99f2c8: The second color in the gradient (light cyan).
                          //135deg means it starts from the top-left corner and moves to the bottom-right corner (diagonal gradient).
                          //"color: #fff;" sets the text color for all elements within the body.
                          //#fff is shorthand for #ffffff, which represents white color
                          //This means that all text on the page will be white unless specifically overridden by other styles (like for buttons or links).
                          //"text-align: center;" This property aligns the text content of the body to the center. Can use :  left;
                          //All inline elements (like text) and inline-block elements inside the body will be horizontally centered.
                          //This is particularly useful for centering the title and other elements, like buttons and paragraphs, within the container.
                          "body { font-family: Arial, sans-serif; background: linear-gradient(135deg, #1f4037, #99f2c8); color: #fff; text-align: center; margin: 0; padding: 10px; }"
                          "h1 { margin: 20px 0; }"
                          ".button { display: block; margin: 10px auto; padding: 15px 30px; font-size: 16px; width: 32ch; background-color: #f39c12; color: white; border: none; border-radius: 5px; cursor: pointer; box-sizing: border-box; text-align: left; overflow: hidden; white-space: nowrap; text-overflow: ellipsis;}"
                          ".active { background-color: #2ecc71; }"
                          "</style>"
                          "<script>"
                          "function updateRelayStates() {"
                          "  fetch('/relayStates').then(response => response.json()).then(states => {"
                          "    for (let i = 0; i < states.length; i++) {"
                          "      const button = document.getElementById('relay' + (i + 1));"
                          "      if (states[i]) {"
                          "        button.classList.add('active');"
                          "      } else {"
                          "        button.classList.remove('active');"
                          "      }"
                          "    }"
                          "  });"
                          "}"
                          "setInterval(updateRelayStates, 1000);"
                          "function controlRelay(relayNum) {"
                          "  fetch('/relay?num=' + relayNum).then(() => { updateRelayStates(); });"
                          "}"
                          "</script>"
                          "</head>"
                          "<body onload=\"updateRelayStates()\">"
                          // Title of web page: Replace text between <h1> and </h1> with desired text to change
                          "<h1>Choose an Appliance!</h1>"
                          "<img src=\"/ElectricityButton.gif\" alt=\"Electricity Button\" style=\"max-width: 20%; height: auto; margin-top: 20px;\" />"
                          // Subtitle: Replace text between <p> and <p> with desired text to change
                          // Button names: Replace text at end of each line between \"> and </ with desired text to change
                          "<button id=\"relay1\" class=\"button\" onclick=\"controlRelay(1)\">1. Lava Lamp</button>"
                          "<button id=\"relay2\" class=\"button\" onclick=\"controlRelay(2)\">2. Light-Incandescent</button>"
                          "<button id=\"relay3\" class=\"button\" onclick=\"controlRelay(3)\">3. Light-Compact Flourescent</button>"
                          "<button id=\"relay4\" class=\"button\" onclick=\"controlRelay(4)\">4. Light-LED</button>"
                          "<button id=\"relay5\" class=\"button\" onclick=\"controlRelay(5)\">5. Hair Dryer</button>"
                          "<button id=\"relay6\" class=\"button\" onclick=\"controlRelay(6)\">6. Mini-Fridge (Heat Pump)</button>"
                          "<button id=\"relay7\" class=\"button\" onclick=\"controlRelay(7)\">7. Meters & Fairy Lights</button>"
                          "<button id=\"relay8\" class=\"button\" onclick=\"controlRelay(8)\">8. Coffee Maker</button>"
                          "</body>"
                          "</html>";

void controlRelay(int relayNum) {
  for (int i = 0; i < numRelays; i++) {
    digitalWrite(relayPins[i], (i == relayNum) ? LOW : HIGH);
  }
  Serial.printf("Relay %d is ON\n", relayNum + 1);
}

void turnOffAllRelays() {
  for (int i = 0; i < numRelays; i++) {
    digitalWrite(relayPins[i], HIGH);
  }
  Serial.println("All relays are OFF");
}

bool isGlobalOffTime() {
  time_t now = time(nullptr);
  struct tm* currentTime = localtime(&now);
  int hour = currentTime->tm_hour;
  return (hour >= offStartHour && hour < offEndHour);
}

bool isLavaLampAutoTime() {
  time_t now = time(nullptr);
  struct tm* currentTime = localtime(&now);
  int hour = currentTime->tm_hour;
  return (hour >= offStartHour && hour < offEndHour); // Same as global off time: midnight to 6 AM
}

void handleRoot() {
  // Get URL parameters
  String redirectURL = server.hasArg("redirectURl") ? server.arg("redirectURl") : "";
  String timeoutStr = server.hasArg("timeout") ? server.arg("timeout") : "300"; // Default 5 minutes
  
  if (redirectURL.length() > 0) {
    // Create HTML with inactivity-based redirect
    String htmlWithRedirect = "<!DOCTYPE HTML>"
                             "<html>"
                             "<head>"
                             "<title>Choose Appliance</title>"
                             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                             "<style>"
                             "body { font-family: Arial, sans-serif; background: linear-gradient(135deg, #1f4037, #99f2c8); color: #fff; text-align: center; margin: 0; padding: 10px; }"
                             "h1 { margin: 20px 0; }"
                             ".button { display: block; margin: 10px auto; padding: 15px 30px; font-size: 16px; width: 32ch; background-color: #f39c12; color: white; border: none; border-radius: 5px; cursor: pointer; box-sizing: border-box; text-align: left; overflow: hidden; white-space: nowrap; text-overflow: ellipsis;}"
                             ".active { background-color: #2ecc71; }"
                             "</style>"
                             "<script>"
                             "var redirectURL = '" + redirectURL + "';"
                             "var timeoutSeconds = " + timeoutStr + ";"
                             "var redirectTimer = null;"
                             "function startRedirectTimer() {"
                             "  if (redirectTimer) clearTimeout(redirectTimer);"
                             "  redirectTimer = setTimeout(function() { window.location.href = redirectURL; }, timeoutSeconds * 1000);"
                             "}"
                             "function resetRedirectTimer() { startRedirectTimer(); }"
                             "function setupInactivityListeners() {"
                             "  var events = ['mousemove','mousedown','click','keypress','scroll','touchstart'];"
                             "  events.forEach(function(event) { document.addEventListener(event, resetRedirectTimer, true); });"
                             "}"
                             "function updateRelayStates() {"
                             "  fetch('/relayStates').then(response => response.json()).then(states => {"
                             "    for (let i = 0; i < states.length; i++) {"
                             "      const button = document.getElementById('relay' + (i + 1));"
                             "      if (button) {"
                             "        if (states[i]) { button.classList.add('active'); }"
                             "        else { button.classList.remove('active'); }"
                             "      }"
                             "    }"
                             "  });"
                             "}"
                             "function controlRelay(relayNum) {"
                             "  fetch('/relay?num=' + relayNum).then(() => { updateRelayStates(); });"
                             "}"
                             "setInterval(updateRelayStates, 1000);"
                             "window.onload = function() {"
                             "  updateRelayStates();"
                             "  startRedirectTimer();"
                             "  setupInactivityListeners();"
                             "};"
                             "</script>"
                             "</head>"
                             "<body>"
                             "<h1>Choose an Appliance!</h1>"
                             "<img src=\"/ElectricityButton.gif\" alt=\"Electricity Button\" style=\"max-width: 20%; height: auto; margin-top: 20px;\" />"
                             "<button id=\"relay1\" class=\"button\" onclick=\"controlRelay(1)\">1. Lava Lamp</button>"
                             "<button id=\"relay2\" class=\"button\" onclick=\"controlRelay(2)\">2. Light-Incandescent</button>"
                             "<button id=\"relay3\" class=\"button\" onclick=\"controlRelay(3)\">3. Light-Compact Flourescent</button>"
                             "<button id=\"relay4\" class=\"button\" onclick=\"controlRelay(4)\">4. Light-LED</button>"
                             "<button id=\"relay5\" class=\"button\" onclick=\"controlRelay(5)\">5. Hair Dryer</button>"
                             "<button id=\"relay6\" class=\"button\" onclick=\"controlRelay(6)\">6. Mini-Fridge (Heat Pump)</button>"
                             "<button id=\"relay7\" class=\"button\" onclick=\"controlRelay(7)\">7. Meters & Fairy Lights</button>"
                             "<button id=\"relay8\" class=\"button\" onclick=\"controlRelay(8)\">8. Coffee Maker</button>"
                             "</body>"
                             "</html>";
    
    server.send(200, "text/html", htmlWithRedirect);
  } else {
    // No redirect parameters, send normal page
    server.send(200, "text/html", htmlContent);
  }
}

void handleRelayControl() {
  if (server.hasArg("num")) {
    int relayNum = server.arg("num").toInt() - 1;
    if (relayNum >= 0 && relayNum < numRelays) {
      controlRelay(relayNum);
      webInteraction = true;
      selectedRelay = relayNum;
      interactionStart = millis();
      inDefaultLoop = false;
      server.send(200, "text/plain", "Relay " + String(relayNum + 1) + " is ON");
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
    json += digitalRead(relayPins[i]) == LOW ? "true" : "false";
    if (i < numRelays - 1) json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiTimeout < 30000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWi-Fi connection failed");
    return;
  }
  Serial.println("\nWi-Fi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  int ntpRetries = 0;
  while (time(nullptr) < 100000 && ntpRetries < 10) {
    delay(500);
    ntpRetries++;
  }
  if (time(nullptr) < 100000) {
    Serial.println("NTP sync failed");
    return;
  }
  Serial.println("Time synchronized!");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  server.on("/", handleRoot);
  server.on("/relay", handleRelayControl);
  server.on("/relayStates", handleRelayStates);
  server.onNotFound(handleNotFound);

  // Serve the .gif file
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
}

void loop() {
  server.handleClient();

  if (isGlobalOffTime()) {
    // During global off time (midnight to 6 AM):
    // - Turn OFF all relays except lava lamp (relay 1)
    // - Turn ON lava lamp automatically
    for (int i = 0; i < numRelays; i++) {
      digitalWrite(relayPins[i], (i == 0) ? LOW : HIGH); // Relay 1 (index 0) ON, others OFF
    }
    delay(1000);
    return;
  }

  if (webInteraction) {
    if (!inDefaultLoop) {
      controlRelay(selectedRelay);
      inDefaultLoop = true;
    }

    if (millis() - interactionStart > onDurations[selectedRelay]) {
      webInteraction = false;
      inDefaultLoop = true;
      turnOffAllRelays();
      lastActivatedRelay = selectedRelay;
    }
    return;
  }

  if (inDefaultLoop && !webInteraction) {
    for (int i = (lastActivatedRelay + 1) % numRelays; i < numRelays; i++) {
      controlRelay(i);
      lastActivatedRelay = i;
      unsigned long startMillis = millis();

      while (millis() - startMillis < onDurations[i]) {
        server.handleClient();
        if (webInteraction || isGlobalOffTime()) {
          turnOffAllRelays();
          return;
        }
      }
    }
    lastActivatedRelay = -1;
  }
} 