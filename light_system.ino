// Required Libraries for ESP32 Web Server
#include <WiFi.h> // For ESP32 Wi-Fi functionalities
#include <AsyncTCP.h> // Asynchronous TCP library, a dependency for ESPAsyncWebServer
#include <ESPAsyncWebServer.h> // Asynchronous Web Server library for ESP32

// --- Wi-Fi Configuration ---
// Define the SSID (network name) for your ESP32's Access Point
const char* ssid = "Ceas Shelby";
// Define the password for your ESP32's Access Point. IMPORTANT: Change this to a strong, unique password!
const char* password = "CeasShelby"; 

// --- LED Pin Definitions (Based on Schematic) ---
// These GPIO pins control the BASE of the NPN transistors that switch the LEDs.
const int LED1_CTRL_PIN = 27; // Connected to base of Q1 for LED 1
const int LED2_CTRL_PIN = 26; // Connected to base of Q2 for LED 2
const int LED3_CTRL_PIN = 25; // Connected to base of Q3 for LED 3

// --- LDR Pin Definition (Based on Schematic) ---
const int LDR_PIN = 34; // Connected to GPIO 34 for analog reading

// --- Automatic Control Settings ---
// Threshold for determining night/day based on LDR reading.
// Values range from 0 (darkest) to 4095 (brightest) for a 12-bit ADC.
// You will likely need to CALIBRATE this value for your specific LDR and environment.
// Lower value = darker for "night" detection.
const int NIGHT_THRESHOLD = 800; // Example: Below 800 is considered night. ADJUST THIS!

// Delay between automatic light checks (in milliseconds)
const long AUTO_CHECK_INTERVAL = 10000; // Check every 10 seconds

// --- Web Server Object ---
// Create an instance of the AsyncWebServer on port 80 (standard HTTP port)
AsyncWebServer server(80);

// --- LED State Variables ---
// Boolean variables to keep track of the current state of each LED (true for ON, false for OFF)
bool led1State = false;
bool led2State = false;
bool led3State = false;

// --- Automatic Mode State Variable ---
bool autoModeEnabled = false;

// --- Timing Variable for Automatic Control ---
unsigned long lastAutoCheckMillis = 0;

// --- Helper function to set LED state (turns transistor ON/OFF) ---
void setLED(int pin, bool state) {
  // Since we are driving NPN transistors in common-emitter configuration:
  // HIGH on base turns transistor ON -> LED ON
  // LOW on base turns transistor OFF -> LED OFF
  digitalWrite(pin, state ? HIGH : LOW);
}

// --- HTML Content for the Web Dashboard ---
String getDashboardHtml() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Ceas Shelby - Intelligent Lighting Control</title>
        <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;660&display=swap" rel="stylesheet">
        <style>
            /* Basic Reset & Body Styling */
            body {
                font-family: 'Inter', sans-serif;
                background-color: #e0f2f7; /* Light blue background */
                display: flex;
                flex-direction: column;
                align-items: center;
                justify-content: center;
                min-height: 100vh;
                margin: 0;
                color: #333;
                line-height: 1.6;
            }

            /* Main Container Styling */
            .container {
                background-color: #ffffff;
                padding: 35px;
                border-radius: 15px;
                box-shadow: 0 8px 20px rgba(0,0,0,0.15); /* Softer, larger shadow */
                text-align: center;
                width: 95%; /* More fluid width */
                max-width: 550px; /* Slightly larger max-width */
                box-sizing: border-box; /* Include padding in width */
                margin: 20px auto; /* Center with margin */
            }

            /* Header Styling */
            h1 {
                color: #007bff; /* Primary blue */
                margin-bottom: 30px;
                font-weight: 660; /* Semi-bold */
                font-size: 2.2em; /* Larger heading */
                line-height: 1.2;
            }

            /* Control Section Styling */
            .control-section {
                margin-bottom: 25px;
                padding: 15px;
                border: 1px solid #e0e0e0;
                border-radius: 10px;
                background-color: #f8f9fa; /* Very light grey for sections */
                display: flex;
                flex-direction: column;
                align-items: center;
            }
            .control-section h2 {
                color: #0056b3; /* Darker blue for subheadings */
                margin-top: 0;
                margin-bottom: 15px;
                font-size: 1.6em;
            }
            .control-section p {
                margin-top: 5px;
                font-size: 1.1em;
            }

            /* Button Styling */
            button {
                background-color: #6c757d; /* Muted grey for OFF state */
                color: white;
                padding: 14px 30px;
                border: none;
                border-radius: 8px; /* More rounded */
                font-size: 1.2em;
                cursor: pointer;
                transition: background-color 0.3s ease, transform 0.2s ease; /* Add transform for subtle click effect */
                width: 80%; /* Slightly narrower buttons */
                max-width: 250px;
                margin-bottom: 12px;
                box-shadow: 0 4px 8px rgba(0,0,0,0.1); /* Button shadow */
            }

            button:hover {
                background-color: #5a6268; /* Darker grey on hover */
                transform: translateY(-2px); /* Lift effect on hover */
            }

            button:active {
                transform: translateY(0); /* Press effect */
                box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            }

            /* ON state for buttons */
            button.on {
                background-color: #28a745; /* Green for ON state */
            }

            button.on:hover {
                background-color: #218838; /* Darker green on hover */
            }

            /* Status Indicator Styling */
            .status-indicator {
                display: inline-block;
                width: 20px; /* Larger indicator */
                height: 20px;
                border-radius: 50%;
                background-color: #ccc; /* Default grey */
                margin-left: 15px;
                vertical-align: middle;
                border: 2px solid rgba(0,0,0,0.1); /* Subtle border */
                box-shadow: inset 0 1px 3px rgba(0,0,0,0.2); /* Inner shadow for depth */
            }

            .status-indicator.on {
                background-color: #28a745; /* Green for ON */
                box-shadow: 0 0 8px rgba(40, 167, 69, 0.6), inset 0 1px 3px rgba(0,0,0,0.2); /* Glow effect */
            }

            .status-indicator.off {
                background-color: #dc3545; /* Red for OFF */
                box-shadow: 0 0 8px rgba(220, 53, 69, 0.6), inset 0 1px 3px rgba(0,0,0,0.2); /* Glow effect */
            }

            /* Footer Styling */
            footer {
                margin-top: 40px;
                font-size: 0.85em;
                color: #555;
                text-align: center;
                padding: 15px;
                background-color: #f0f0f0; /* Light grey footer background */
                width: 100%;
                box-sizing: border-box;
                border-top: 1px solid #ddd;
            }
            footer p {
                margin: 5px 0;
            }

            /* Responsive Adjustments */
            @media (max-width: 480px) {
                .container {
                    padding: 20px;
                }
                h1 {
                    font-size: 1.8em;
                }
                button {
                    font-size: 1.1em;
                    padding: 12px 20px;
                    width: 90%;
                }
                .status-indicator {
                    width: 18px;
                    height: 18px;
                }
            }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>Ceas Shelby<br> Intelligent Lighting Control</h1>

            <div class="control-section">
                <h2>Automatic Mode</h2>
                <button id="autoModeButton" onclick="toggleAutoMode()">Toggle Automatic Mode</button>
                <span id="autoModeStatus" class="status-indicator off"></span>
                <p>Light Sensor Reading: <span id="ldrValue">---</span></p>
            </div>

            <div class="control-section">
                <h2>Manual LED Control</h2>
                <div class="light-control">
                    <h3>LED 1</h3>
                    <button id="led1Button" onclick="toggleLED(1)">Toggle LED 1</button>
                    <span id="led1Status" class="status-indicator off"></span>
                </div>

                <div class="light-control">
                    <h3>LED 2</h3>
                    <button id="led2Button" onclick="toggleLED(2)">Toggle LED 2</button>
                    <span id="led2Status" class="status-indicator off"></span>
                </div>

                <div class="light-control">
                    <h3>LED 3</h3>
                    <button id="led3Button" onclick="toggleLED(3)">Toggle LED 3</button>
                    <span id="led3Status" class="status-indicator off"></span>
                </div>
            </div>
        </div>

        <footer>
            <p>&copy;Ceas Shelby</p>
            <p></p>
        </footer>

        <script>
            // Function to send a request to the ESP32 to toggle an LED
            async function toggleLED(ledNum) {
                const button = document.getElementById(`led${ledNum}Button`);
                const statusIndicator = document.getElementById(`led${ledNum}Status`);

                try {
                    const response = await fetch(`/led${ledNum}/toggle`);
                    const data = await response.text();
                    console.log(`Response for LED${ledNum}:`, data);
                    updateUI(ledNum, data.includes("ON")); // Update UI based on response
                } catch (error) {
                    console.error('Error toggling LED:', error);
                    alert('Could not connect to ESP32. Please ensure it is powered on and you are connected to its Wi-Fi network.');
                }
            }

            // Function to toggle Automatic Mode
            async function toggleAutoMode() {
                const button = document.getElementById('autoModeButton');
                const statusIndicator = document.getElementById('autoModeStatus');

                try {
                    const response = await fetch('/automode/toggle');
                    const data = await response.json(); // Expecting JSON: { "autoModeEnabled": true/false }
                    console.log('Auto Mode Toggled:', data);
                    updateAutoModeUI(data.autoModeEnabled);
                } catch (error) {
                    console.error('Error toggling Auto Mode:', error);
                    alert('Could not connect to ESP32 to toggle Auto Mode.');
                }
            }

            // Function to fetch the current status of all LEDs and Auto Mode from the ESP32
            async function updateAllStatus() {
                try {
                    const response = await fetch('/status');
                    const data = await response.json();
                    console.log("Current System Status:", data);

                    updateUI(1, data.led1);
                    updateUI(2, data.led2);
                    updateUI(3, data.led3);
                    updateAutoModeUI(data.autoModeEnabled);
                    document.getElementById('ldrValue').innerText = data.ldrValue; // Update LDR value

                } catch (error) {
                    console.error('Error fetching system status:', error);
                }
            }

            // Helper function to update the UI elements for a specific LED
            function updateUI(ledNum, state) {
                const button = document.getElementById(`led${ledNum}Button`);
                const statusIndicator = document.getElementById(`led${ledNum}Status`);

                if (state) { // If state is true (LED is ON)
                    button.classList.add('on');
                    statusIndicator.classList.remove('off');
                    statusIndicator.classList.add('on');
                } else { // If state is false (LED is OFF)
                    button.classList.remove('on');
                    statusIndicator.classList.remove('on');
                    statusIndicator.classList.add('off');
                }
            }

            // Helper function to update the UI for Automatic Mode
            function updateAutoModeUI(state) {
                const button = document.getElementById('autoModeButton');
                const statusIndicator = document.getElementById('autoModeStatus');

                if (state) {
                    button.classList.add('on');
                    button.innerText = "Automatic Mode: ON";
                    statusIndicator.classList.remove('off');
                    statusIndicator.classList.add('on');
                } else {
                    button.classList.remove('on');
                    button.innerText = "Automatic Mode: OFF";
                    statusIndicator.classList.remove('on');
                    statusIndicator.classList.add('off');
                }
            }

            // Call updateAllStatus when the window finishes loading
            window.onload = function() {
                updateAllStatus();
                // Periodically refresh the entire system status to keep the dashboard in sync
                setInterval(updateAllStatus, 3000); // Refresh every 3 seconds
            };
        </script>
    </body>
    </html>
  )rawliteral";
  return html;
}

// --- Arduino Setup Function ---
void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting ESP32 Intelligent Lighting System...");

  // Set LED control pins as OUTPUTs
  pinMode(LED1_CTRL_PIN, OUTPUT);
  pinMode(LED2_CTRL_PIN, OUTPUT);
  pinMode(LED3_CTRL_PIN, OUTPUT);
  
  // Set LDR pin as INPUT (implicitly done for analogRead, but good practice)
  pinMode(LDR_PIN, INPUT);

  // Initialize all LEDs to OFF state
  setLED(LED1_CTRL_PIN, LOW);
  setLED(LED2_CTRL_PIN, LOW);
  setLED(LED3_CTRL_PIN, LOW);
  led1State = false;
  led2State = false;
  led3State = false;

  // Start the ESP32 in Access Point (AP) mode
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point (AP) IP address: ");
  Serial.println(IP);
  Serial.print("Connect to Wi-Fi network: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.println("Then open a web browser and go to the IP address above.");

  // --- Web Server Route Handlers ---

  // Route for the root URL ("/") - serves the main HTML dashboard
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Client requested root URL '/'");
    request->send(200, "text/html", getDashboardHtml());
  });

  // Route to toggle LED 1
  server.on("/led1/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    led1State = !led1State;
    setLED(LED1_CTRL_PIN, led1State);
    Serial.printf("LED 1 toggled to: %s\n", led1State ? "ON" : "OFF");
    request->send(200, "text/plain", led1State ? "LED1_ON" : "LED1_OFF");
  });

  // Route to toggle LED 2
  server.on("/led2/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    led2State = !led2State;
    setLED(LED2_CTRL_PIN, led2State);
    Serial.printf("LED 2 toggled to: %s\n", led2State ? "ON" : "OFF");
    request->send(200, "text/plain", led2State ? "LED2_ON" : "LED2_OFF");
  });

  // Route to toggle LED 3
  server.on("/led3/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    led3State = !led3State;
    setLED(LED3_CTRL_PIN, led3State);
    Serial.printf("LED 3 toggled to: %s\n", led3State ? "ON" : "OFF");
    request->send(200, "text/plain", led3State ? "LED3_ON" : "LED3_OFF");
  });

  // Route to toggle Automatic Mode
  server.on("/automode/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    autoModeEnabled = !autoModeEnabled;
    Serial.printf("Automatic Mode toggled to: %s\n", autoModeEnabled ? "ENABLED" : "DISABLED");
    String jsonResponse = "{ \"autoModeEnabled\": " + String(autoModeEnabled ? "true" : "false") + " }";
    request->send(200, "application/json", jsonResponse);
  });

  // Route to get the current status of all LEDs, Auto Mode, and LDR value as JSON
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    // Serial.println("Client requested system status '/status'"); // Uncomment for more verbose logging
    int ldrValue = analogRead(LDR_PIN); // Read LDR value
    
    String jsonResponse = "{";
    jsonResponse += "\"led1\":" + String(led1State ? "true" : "false") + ",";
    jsonResponse += "\"led2\":" + String(led2State ? "true" : "false") + ",";
    jsonResponse += "\"led3\":" + String(led3State ? "true" : "false") + ",";
    jsonResponse += "\"autoModeEnabled\":" + String(autoModeEnabled ? "true" : "false") + ",";
    jsonResponse += "\"ldrValue\":" + String(ldrValue);
    jsonResponse += "}";
    request->send(200, "application/json", jsonResponse);
  });

  // Start the web server
  server.begin();
  Serial.println("Web server started.");
}

// --- Arduino Loop Function ---
// This function runs repeatedly after setup()
void loop() {
  // Check for automatic light control if enabled
  if (autoModeEnabled) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastAutoCheckMillis >= AUTO_CHECK_INTERVAL) {
      lastAutoCheckMillis = currentMillis;

      int ldrValue = analogRead(LDR_PIN);
      Serial.printf("LDR Value: %d, Threshold: %d\n", ldrValue, NIGHT_THRESHOLD);

      if (ldrValue < NIGHT_THRESHOLD) { // It's dark (LDR value is low)
        Serial.println("It's NIGHT - Turning ALL LEDs ON automatically.");
        if (!led1State) { led1State = true; setLED(LED1_CTRL_PIN, HIGH); }
        if (!led2State) { led2State = true; setLED(LED2_CTRL_PIN, HIGH); }
        if (!led3State) { led3State = true; setLED(LED3_CTRL_PIN, HIGH); }
      } else { // It's bright (LDR value is high)
        Serial.println("It's DAY - Turning ALL LEDs OFF automatically.");
        if (led1State) { led1State = false; setLED(LED1_CTRL_PIN, LOW); }
        if (led2State) { led2State = false; setLED(LED2_CTRL_PIN, LOW); }
        if (led3State) { led3State = false; setLED(LED3_CTRL_PIN, LOW); }
      }
    }
  }
}
