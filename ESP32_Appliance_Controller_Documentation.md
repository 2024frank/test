# ESP32 Appliance Controller Documentation

## Overview

This ESP32-based appliance controller manages 8 electrical appliances through relays, providing both automatic cycling and web-based manual control. The system includes time-based scheduling, energy-saving features, and remote redirect capabilities.

---

## Hardware Configuration

### ESP32 Board
- **Model**: ESP32 DevKit or compatible
- **MAC Address**: a842e3a9d9d0 (example for ORB036)

### Relay Module
- **Type**: 8-channel relay module
- **Connection**: ESP32 GPIO pins to relay inputs
- **Relay Logic**: LOW = ON, HIGH = OFF (active low)

### Pin Assignments
```cpp
const int relayPins[] = {4, 5, 18, 19, 21, 22, 23, 25};
```

| Relay # | GPIO Pin | Appliance |
|---------|----------|-----------|
| 1 | 4 | Lava Lamp |
| 2 | 5 | Light-Incandescent |
| 3 | 18 | Light-Compact Fluorescent |
| 4 | 19 | Light-LED |
| 5 | 21 | Hair Dryer |
| 6 | 22 | Mini-Fridge (Heat Pump) |
| 7 | 23 | Meters & Fairy Lights |
| 8 | 25 | Coffee Maker |

---

## Network Configuration

### WiFi Settings
```cpp
#define STASSID "ObieConnect"
#define STAPSK "122ElmStreet"
```

### Network Details
- **Primary Network**: ObieConnect (Oberlin College WiFi)
- **Expected IP**: 10.17.192.136
- **Alternative Network**: London/Petersen (Petersen Home WiFi)
- **Alternative IP**: 192.168.0.236

---

## Core Functionality

### 1. Automatic Cycling System

The system automatically cycles through all 8 relays in sequence when no web interaction is occurring.

#### Cycle Durations (in milliseconds)
```cpp
unsigned long onDurations[] = {60000, 10000, 10000, 10000, 7000, 30000, 10000, 10000};
```

| Relay # | Appliance | Duration |
|---------|-----------|----------|
| 1 | Lava Lamp | 60 seconds |
| 2 | Light-Incandescent | 10 seconds |
| 3 | Light-Compact Fluorescent | 10 seconds |
| 4 | Light-LED | 10 seconds |
| 5 | Hair Dryer | 7 seconds |
| 6 | Mini-Fridge (Heat Pump) | 30 seconds |
| 7 | Meters & Fairy Lights | 10 seconds |
| 8 | Coffee Maker | 10 seconds |

#### Cycling Behavior
- **Only ONE relay active at a time**
- Cycles: 1→2→3→4→5→6→7→8→1→2...
- Each relay runs for its specified duration
- Continues indefinitely unless interrupted

### 2. Web Interface Control

Users can override automatic cycling by clicking appliance buttons on the web interface.

#### Web Interaction Behavior
1. **Button Click**: Selected relay turns ON, all others turn OFF
2. **Duration Override**: Relay stays ON for its programmed duration
3. **Return to Cycling**: After duration expires, automatic cycling resumes
4. **Interruption Handling**: Web interactions immediately stop automatic cycling

### 3. Time-Based Scheduling

#### Global Off Time (Energy Saving)
- **Period**: 12:00 AM to 6:00 AM (GMT)
- **Behavior**: All relays turn OFF except lava lamp
- **Purpose**: Energy conservation during low-usage hours

#### Lava Lamp Auto-On Schedule  
- **Period**: 12:00 AM to 6:00 AM (GMT)
- **Behavior**: Lava lamp automatically turns ON
- **Priority**: Overrides all other scheduling for relay 1
- **Rationale**: Lava lamps need extended heating time

#### Time Functions
```cpp
bool isGlobalOffTime()      // Checks if in 12 AM - 6 AM period
bool isLavaLampAutoTime()   // Checks if lava lamp should auto-turn on
```

---

## Web Interface Features

### 1. Standard Interface
**URL**: `http://10.17.192.136/`

#### Elements
- **Title**: "Choose an Appliance!"
- **Electricity GIF**: Visual indicator
- **8 Appliance Buttons**: Individual relay controls
- **Status Indicators**: Green highlight when relay is active

#### JavaScript Functionality
- **Real-time Status Updates**: Polls relay states every second
- **Button Feedback**: Visual confirmation of relay states
- **Responsive Design**: Mobile-friendly interface

### 2. Redirect Interface
**URL**: `http://10.17.192.136/?redirectURl=<URL>&timeout=<seconds>`

#### Parameters
- **redirectURl**: Target URL for automatic redirect
- **timeout**: Time in seconds before redirect (default: 300)

#### Examples
```
http://10.17.192.136/?redirectURl=https://google.com&timeout=60
http://10.17.192.136/?redirectURl=https://some-app.vercel.app&timeout=300
```

#### Behavior
- **Silent Operation**: No countdown or cancel options
- **Functional Interface**: All appliance controls work normally
- **Automatic Redirect**: Occurs after specified timeout
- **Background Process**: Uses JavaScript `setTimeout()`

---

## API Endpoints

### Web Server Routes
```cpp
server.on("/", handleRoot);                    // Main page or redirect page
server.on("/relay", handleRelayControl);       // Relay control endpoint
server.on("/relayStates", handleRelayStates);  // Status JSON endpoint
server.on("/ElectricityButton.gif", ...);      // Static GIF file
server.onNotFound(handleNotFound);             // 404 handler
```

### Relay Control API
**Endpoint**: `/relay?num=<1-8>`
- **Method**: GET
- **Parameter**: `num` (relay number 1-8)
- **Response**: Status message
- **Action**: Activates specified relay, deactivates others

### Relay Status API  
**Endpoint**: `/relayStates`
- **Method**: GET
- **Response**: JSON array of boolean values
- **Format**: `[true, false, false, false, false, false, false, false]`
- **Update Frequency**: Polled every 1000ms by web interface

---

## System States and Logic

### State Variables
```cpp
volatile bool webInteraction = false;    // True when user controls relay
volatile int selectedRelay = -1;         // Currently selected relay (0-7)
unsigned long interactionStart = 0;     // Timestamp of last interaction
bool inDefaultLoop = true;               // True when in automatic cycling
int lastActivatedRelay = -1;            // Last relay in cycle sequence
```

### State Transitions

#### Normal Operation (Automatic Cycling)
```
webInteraction = false
inDefaultLoop = true
→ Cycle through relays based on onDurations[]
```

#### User Interaction
```
Button Click
→ webInteraction = true
→ inDefaultLoop = false  
→ Selected relay ON for its duration
→ Return to automatic cycling
```

#### Global Off Time (Midnight - 6 AM)
```
isGlobalOffTime() = true
→ Lava lamp (relay 1) = ON
→ All other relays = OFF
→ Override all other logic
```

---

## Key Functions Reference

### Core Control Functions
```cpp
void controlRelay(int relayNum)     // Turn ON specific relay, turn OFF others
void turnOffAllRelays()             // Turn OFF all relays
bool isGlobalOffTime()              // Check if in global off period
bool isLavaLampAutoTime()           // Check if lava lamp should auto-turn on
```

### Web Server Handlers
```cpp
void handleRoot()                   // Serve main page or redirect page
void handleRelayControl()           // Process relay control requests
void handleRelayStates()            // Return current relay states as JSON
void handleNotFound()               // Handle 404 errors
```

### Setup and Loop
```cpp
void setup()                        // Initialize hardware, WiFi, web server
void loop()                         // Main program loop with state management
```

---

## Timing and Scheduling

### Time Synchronization
- **NTP Servers**: pool.ntp.org, time.nist.gov
- **Timezone**: GMT (no offset applied)
- **Retry Logic**: Up to 10 attempts for NTP sync
- **Fallback**: System continues without time if NTP fails

### Schedule Priorities (Highest to Lowest)
1. **Global Off Time**: 12 AM - 6 AM (affects all relays)
2. **Lava Lamp Auto-On**: 12 AM - 6 AM (relay 1 only)
3. **Web Interaction**: User button clicks
4. **Automatic Cycling**: Default behavior

---

## File System (SPIFFS)

### Stored Files
- **ElectricityButton.gif**: Animated electricity icon
- **Future expansion**: Additional web assets

### File Serving
```cpp
server.on("/ElectricityButton.gif", HTTP_GET, []() {
  File file = SPIFFS.open("/ElectricityButton.gif", "r");
  if (file) {
    server.streamFile(file, "image/gif");
    file.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
});
```

---

## Error Handling and Recovery

### WiFi Connection
- **Timeout**: 30 seconds maximum connection attempt
- **Failure Behavior**: System continues without network functionality
- **Status**: Connection status logged to Serial

### NTP Synchronization
- **Retry Limit**: 10 attempts
- **Failure Behavior**: System operates without time-based features
- **Fallback**: Manual time scheduling disabled if NTP fails

### SPIFFS Mounting
- **Failure Behavior**: Static files unavailable, system continues
- **Impact**: GIF image won't display, core functionality preserved

---

## Serial Monitor Output

### Debug Information
- WiFi connection status and IP address
- NTP synchronization confirmation
- Relay state changes with timestamps
- Web server startup confirmation
- Error messages for failed components

### Example Output
```
ESP32 Relay Controller Starting...
Connecting to WiFi: ObieConnect
Wi-Fi connected successfully!
IP address: 10.17.192.136
Time synchronized!
HTTP server started
Relay 1 is ON
```

---

## Customization Guide

### Changing Relay Durations
Modify the `onDurations[]` array:
```cpp
unsigned long onDurations[] = {
  60000,  // Lava Lamp: 60 seconds
  10000,  // Light-Incandescent: 10 seconds
  // ... modify as needed
};
```

### Changing Global Off Time
Modify the time constants:
```cpp
const int offStartHour = 0;   // Start hour (0 = midnight)
const int offEndHour = 6;     // End hour (6 = 6 AM)
```

### Adding New Appliances
1. Add GPIO pin to `relayPins[]` array
2. Add duration to `onDurations[]` array
3. Update `numRelays` calculation (automatic)
4. Add button to HTML content
5. Update documentation

### Changing WiFi Networks
```cpp
#define STASSID "YourNetworkName"
#define STAPSK "YourPassword"
```

---

## Troubleshooting

### Common Issues

#### 1. Web Interface Not Accessible
- **Check WiFi Connection**: Verify ESP32 connected to network
- **Check IP Address**: Use Serial Monitor to confirm IP
- **Network Access**: Ensure client device on same network

#### 2. Relays Not Responding
- **Check Wiring**: Verify GPIO connections to relay module
- **Power Supply**: Ensure relay module properly powered
- **Relay Logic**: Confirm active-low operation (LOW = ON)

#### 3. Time Scheduling Not Working
- **NTP Connection**: Check internet connectivity
- **Time Zone**: Verify GMT time calculation
- **Serial Output**: Monitor time synchronization messages

#### 4. Automatic Cycling Issues
- **Web Interaction State**: Check if stuck in webInteraction mode
- **Duration Values**: Verify onDurations[] array values
- **Loop Logic**: Monitor Serial output for state changes

---

## Security Considerations

### Network Security
- **Open WiFi**: Controller accessible to all network users
- **No Authentication**: Web interface has no login protection
- **Local Network Only**: Not exposed to internet by default

### Electrical Safety
- **Relay Ratings**: Ensure relays rated for connected appliances
- **Circuit Protection**: Use appropriate fuses/breakers
- **Ground Fault Protection**: Consider GFCI protection for wet locations

---

## Future Enhancements

### Potential Improvements
1. **Web Authentication**: Add login system for security
2. **HTTPS Support**: Encrypt web communications
3. **Mobile App**: Native iOS/Android control interface
4. **Scheduling UI**: Web-based time schedule configuration
5. **Usage Logging**: Track appliance usage statistics
6. **Voice Control**: Integration with Alexa/Google Assistant
7. **Energy Monitoring**: Add current/power measurement
8. **Remote Access**: VPN or cloud-based control

### Code Expansion Areas
- **JSON Configuration**: Replace hardcoded values with config file
- **OTA Updates**: Over-the-air firmware updates
- **Multiple Networks**: WiFi network failover capability
- **Real-time Clock**: Battery-backed RTC for offline operation

---

## Version History

### Current Version Features
- 8-relay automatic cycling with custom durations
- Web interface with real-time status updates
- Time-based global off scheduling (12 AM - 6 AM)
- Lava lamp auto-on during global off period
- Silent redirect functionality with URL parameters
- Mobile-responsive web design
- SPIFFS file system support
- NTP time synchronization

### Previous Iterations
- Basic relay cycling
- Simple web interface
- Manual schedule configuration
- Countdown timers (removed for silent operation)
- Complex state management (simplified)

---

*Last Updated: [Current Date]*
*Code Version: Latest commit on main branch* 