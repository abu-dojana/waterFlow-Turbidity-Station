#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// WiFi Hotspot Configuration (plantech style)
const char* ap_ssid = "ESP32-SENSORS";
const char* ap_password = "abudojana";

// IP Configuration (plantech style)
IPAddress apIP(192, 168, 10, 10);
DNSServer dnsServer;
WebServer server(80);

// Water Flow Sensor Configuration
volatile int flow_frequency = 0;
unsigned int l_hour = 0;
unsigned char flowsensor = 4; // GPIO 4
float flowRate = 0.0; // L/min
float totalVolume = 0.0; // Total liters
int flowRawValue = 0; // Raw pulse count
float flowVoltage = 0.0; // Flow sensor voltage (if analog)

// Turbidity Sensor Configuration
const int turbidityPin = 36; // GPIO 36 (A0)
float turbidityVoltage = 0;
float ntu = 0;
int turbidityRawValue = 0; // Raw ADC reading

// Calibration variables for turbidity sensor
float clearWaterVoltage = 1.46;
float blockedVoltage = 0.07;
float airVoltageMin = 1.15;
float airVoltageMax = 1.30;
float cleanWaterThreshold = 1.40;
float blockedThreshold = 0.10;
float maxNTU = 3000.0;
float minNTU = 0.0;
float cleanWaterMaxNTU = 50.0;
float turbidWaterMinNTU = 100.0;

// Data Collection Variables
bool dataCollectionActive = false;
unsigned long lastDataCollection = 0;
const unsigned long dataInterval = 500; // 500ms interval
String dataLog = "";
int dataPointCount = 0;
const int maxDataPoints = 1000; // Limit to prevent memory overflow

// Timing variables
unsigned long currentTime;
unsigned long cloopTime;

// Interrupt function for water flow sensor
void IRAM_ATTR flow() {
  flow_frequency++;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting ESP32 Water Sensors...");
  
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  // Setup WiFi Access Point (plantech style)
  setupAccessPoint();
  Serial.println("Access Point and Web Server started");
  
  // Initialize sensors
  pinMode(flowsensor, INPUT_PULLUP);
  pinMode(turbidityPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(flowsensor), flow, RISING);
  
  currentTime = millis();
  cloopTime = currentTime;
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println("ESP32 Sensor Server Ready");
  Serial.println("Connect to WiFi and visit: http://192.168.10.10");
}

void loop() {
  // DNS and server handling (plantech style)
  dnsServer.processNextRequest();
  server.handleClient();
  
  currentTime = millis();
  
  // Update flow sensor every second
  if(currentTime >= (cloopTime + 1000)) {
    cloopTime = currentTime;
    updateFlowSensor();
  }
  
  // Collect data every 500ms if active
  if(dataCollectionActive && (currentTime - lastDataCollection >= dataInterval)) {
    collectDataPoint();
    lastDataCollection = currentTime;
  }
  
  delay(10); // Small delay to prevent watchdog issues
}

void updateFlowSensor() {
  // Store raw pulse count
  flowRawValue = flow_frequency;
  
  // Calculate flow rate
  flowRate = flow_frequency / 7.5; // L/min
  l_hour = flowRate * 60; // L/hour
  
  // Add to total volume (convert to liters per minute)
  totalVolume += flowRate / 60.0; // Add liters per second
  
  // Calculate flow sensor voltage (assuming 3.3V supply when flowing)
  flowVoltage = (flow_frequency > 0) ? 3.3 : 0.0; // Simple estimation
  
  flow_frequency = 0; // Reset counter
}

void updateTurbiditySensor() {
  // Read multiple samples for stability
  int sensorValue = 0;
  for(int i = 0; i < 10; i++) {
    sensorValue += analogRead(turbidityPin);
    delay(1);
  }
  sensorValue = sensorValue / 10;
  
  // Store raw ADC value
  turbidityRawValue = sensorValue;
  
  // Convert to voltage
  turbidityVoltage = sensorValue * (3.3 / 4095.0);
  
  // Calculate NTU based on voltage
  if(turbidityVoltage < blockedThreshold) {
    ntu = maxNTU;
  } 
  else if(turbidityVoltage >= cleanWaterThreshold) {
    ntu = map(turbidityVoltage * 100, cleanWaterThreshold * 100, clearWaterVoltage * 100, cleanWaterMaxNTU, minNTU);
    if(ntu < minNTU) ntu = minNTU;
    if(ntu > cleanWaterMaxNTU) ntu = cleanWaterMaxNTU;
  } 
  else if(turbidityVoltage >= airVoltageMin && turbidityVoltage <= airVoltageMax) {
    ntu = 500; // Sensor in air
  } 
  else {
    ntu = map(turbidityVoltage * 100, blockedThreshold * 100, cleanWaterThreshold * 100, maxNTU, turbidWaterMinNTU);
    if(ntu > maxNTU) ntu = maxNTU;
    if(ntu < turbidWaterMinNTU) ntu = turbidWaterMinNTU;
  }
}

void collectDataPoint() {
  if(dataPointCount >= maxDataPoints) {
    return; // Prevent memory overflow
  }
  
  updateTurbiditySensor();
  
  // Create JSON data point with all sensor values
  DynamicJsonDocument doc(300);
  doc["timestamp"] = millis();
  doc["flowRate"] = flowRate;
  doc["totalVolume"] = totalVolume;
  doc["flowVoltage"] = flowVoltage;
  doc["flowRaw"] = flowRawValue;
  doc["turbidityVoltage"] = turbidityVoltage;
  doc["turbidityRaw"] = turbidityRawValue;
  doc["ntu"] = ntu;
  doc["lHour"] = l_hour;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  if(dataLog.length() > 0) {
    dataLog += ",";
  }
  dataLog += jsonString;
  dataPointCount++;
}

void setupWebServer() {
  // Serve main page
  server.on("/", handleRoot);
  
  // API endpoints
  server.on("/api/data", handleCurrentData);
  server.on("/api/start", handleStartCollection);
  server.on("/api/stop", handleStopCollection);
  server.on("/api/clear", handleClearData);
  server.on("/api/export/csv", handleExportCSV);
  server.on("/api/export/json", handleExportJSON);
  server.on("/api/export/txt", handleExportTXT);
  server.on("/api/calibrate", HTTP_POST, handleCalibration);
  server.on("/api/status", handleStatus);
  server.on("/api/datalog", handleDataLog);
  
  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = getMainHTML();
  server.send(200, "text/html", html);
}

void handleCurrentData() {
  updateTurbiditySensor();
  
  DynamicJsonDocument doc(400);
  doc["flowRate"] = flowRate;
  doc["totalVolume"] = totalVolume;
  doc["flowVoltage"] = flowVoltage;
  doc["flowRaw"] = flowRawValue;
  doc["lHour"] = l_hour;
  doc["turbidityVoltage"] = turbidityVoltage;
  doc["turbidityRaw"] = turbidityRawValue;
  doc["ntu"] = ntu;
  doc["timestamp"] = millis();
  doc["dataCollectionActive"] = dataCollectionActive;
  doc["dataPointCount"] = dataPointCount;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleStartCollection() {
  dataCollectionActive = true;
  lastDataCollection = millis();
  server.send(200, "text/plain", "Data collection started");
}

void handleStopCollection() {
  dataCollectionActive = false;
  server.send(200, "text/plain", "Data collection stopped");
}

void handleClearData() {
  dataLog = "";
  dataPointCount = 0;
  totalVolume = 0.0;
  server.send(200, "text/plain", "Data cleared");
}

void handleStatus() {
  DynamicJsonDocument doc(200);
  doc["dataCollectionActive"] = dataCollectionActive;
  doc["dataPointCount"] = dataPointCount;
  doc["uptime"] = millis();
  doc["freeHeap"] = ESP.getFreeHeap();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleDataLog() {
  String response = "[" + dataLog + "]";
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", response);
}

void handleCalibration() {
  if (server.hasArg("clearWater") && server.hasArg("blocked") && 
      server.hasArg("airMin") && server.hasArg("airMax")) {
    
    clearWaterVoltage = server.arg("clearWater").toFloat();
    blockedVoltage = server.arg("blocked").toFloat();
    airVoltageMin = server.arg("airMin").toFloat();
    airVoltageMax = server.arg("airMax").toFloat();
    cleanWaterThreshold = clearWaterVoltage - 0.06;
    blockedThreshold = blockedVoltage + 0.03;
    
    server.send(200, "text/plain", "Calibration updated successfully");
  } else {
    server.send(400, "text/plain", "Missing calibration parameters");
  }
}

void handleExportCSV() {
  String csv = "Timestamp,Flow Rate (L/min),Total Volume (L),L/Hour,Flow Voltage (V),Flow Raw,Turbidity Voltage (V),Turbidity Raw,NTU\n";
  
  // Parse JSON data and convert to CSV
  if(dataLog.length() > 0) {
    String jsonArray = "[" + dataLog + "]";
    
    // Allocate larger buffer for JSON parsing
    DynamicJsonDocument doc(dataLog.length() * 2 + 2000);
    DeserializationError error = deserializeJson(doc, jsonArray);
    
    if(!error) {
      JsonArray array = doc.as<JsonArray>();
      for(JsonObject item : array) {
        csv += String(item["timestamp"].as<unsigned long>()) + ",";
        csv += String(item["flowRate"].as<float>(), 3) + ",";
        csv += String(item["totalVolume"].as<float>(), 3) + ",";
        csv += String(item["lHour"].as<int>()) + ",";
        csv += String(item["flowVoltage"].as<float>(), 3) + ",";
        csv += String(item["flowRaw"].as<int>()) + ",";
        csv += String(item["turbidityVoltage"].as<float>(), 3) + ",";
        csv += String(item["turbidityRaw"].as<int>()) + ",";
        csv += String(item["ntu"].as<float>(), 1) + "\n";
      }
    } else {
      csv += "Error parsing data: ";
      switch(error.code()) {
        case DeserializationError::Ok: csv += "Ok"; break;
        case DeserializationError::InvalidInput: csv += "Invalid Input"; break;
        case DeserializationError::NoMemory: csv += "No Memory"; break;
        case DeserializationError::TooDeep: csv += "Too Deep"; break;
        default: csv += "Unknown"; break;
      }
      csv += "\n";
      csv += "Data length: " + String(dataLog.length()) + "\n";
      csv += "First 100 chars: " + dataLog.substring(0, 100) + "\n";
    }
  } else {
    csv += "No data collected yet\n";
  }
  
  server.sendHeader("Content-Disposition", "attachment; filename=sensor_data.csv");
  server.sendHeader("Content-Type", "text/csv");
  server.send(200, "text/csv", csv);
}

void handleExportJSON() {
  String response;
  
  if(dataLog.length() > 0) {
    response = "[" + dataLog + "]";
    
    // Validate JSON before sending
    DynamicJsonDocument testDoc(dataLog.length() * 2 + 2000);
    DeserializationError error = deserializeJson(testDoc, response);
    
    if(error) {
      // If JSON is invalid, send error info
      response = "{\"error\":\"JSON parsing failed\",\"code\":\"";
      switch(error.code()) {
        case DeserializationError::Ok: response += "Ok"; break;
        case DeserializationError::InvalidInput: response += "Invalid Input"; break;
        case DeserializationError::NoMemory: response += "No Memory"; break;
        case DeserializationError::TooDeep: response += "Too Deep"; break;
        default: response += "Unknown"; break;
      }
      response += "\",\"dataLength\":" + String(dataLog.length()) + "}";
    }
  } else {
    response = "{\"message\":\"No data collected yet\",\"dataPoints\":0}";
  }
  
  server.sendHeader("Content-Disposition", "attachment; filename=sensor_data.json");
  server.sendHeader("Content-Type", "application/json");
  server.send(200, "application/json", response);
}

void handleExportTXT() {
  String txt = "ESP32 Sensor Data Export\n";
  txt += "========================\n";
  txt += "Generated: " + String(millis()) + " ms since boot\n";
  txt += "Data Points: " + String(dataPointCount) + "\n\n";
  
  if(dataLog.length() > 0) {
    String jsonArray = "[" + dataLog + "]";
    DynamicJsonDocument doc(dataLog.length() * 2 + 2000);
    DeserializationError error = deserializeJson(doc, jsonArray);
    
    if(!error) {
      JsonArray array = doc.as<JsonArray>();
      int pointNum = 1;
      for(JsonObject item : array) {
        txt += "Data Point " + String(pointNum) + ":\n";
        txt += "  Timestamp: " + String(item["timestamp"].as<unsigned long>()) + " ms\n";
        txt += "  Flow Rate: " + String(item["flowRate"].as<float>(), 3) + " L/min\n";
        txt += "  Total Volume: " + String(item["totalVolume"].as<float>(), 3) + " L\n";
        txt += "  L/Hour: " + String(item["lHour"].as<int>()) + "\n";
        txt += "  Flow Voltage: " + String(item["flowVoltage"].as<float>(), 3) + " V\n";
        txt += "  Flow Raw: " + String(item["flowRaw"].as<int>()) + "\n";
        txt += "  Turbidity Voltage: " + String(item["turbidityVoltage"].as<float>(), 3) + " V\n";
        txt += "  Turbidity Raw: " + String(item["turbidityRaw"].as<int>()) + "\n";
        txt += "  NTU: " + String(item["ntu"].as<float>(), 1) + "\n";
        txt += "  -------------------\n";
        pointNum++;
      }
    } else {
      txt += "ERROR: Failed to parse data\n";
      txt += "Error Code: ";
      switch(error.code()) {
        case DeserializationError::Ok: txt += "Ok"; break;
        case DeserializationError::InvalidInput: txt += "Invalid Input"; break;
        case DeserializationError::NoMemory: txt += "No Memory"; break;
        case DeserializationError::TooDeep: txt += "Too Deep"; break;
        default: txt += "Unknown"; break;
      }
      txt += "\nData Length: " + String(dataLog.length()) + " bytes\n";
      txt += "Sample Data: " + dataLog.substring(0, min(200, (int)dataLog.length())) + "\n";
    }
  } else {
    txt += "No data has been collected yet.\n";
    txt += "Start data collection to see results here.\n";
  }
  
  server.sendHeader("Content-Disposition", "attachment; filename=sensor_data.txt");
  server.sendHeader("Content-Type", "text/plain");
  server.send(200, "text/plain", txt);
}

String getMainHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 Water Sensors</title>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += ".header { text-align: center; color: #333; margin-bottom: 30px; }";
  html += ".sensor-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 20px; }";
  html += ".sensor-card { background: #f8f9fa; padding: 15px; border-radius: 8px; border-left: 4px solid #007bff; }";
  html += ".sensor-value { font-size: 24px; font-weight: bold; color: #007bff; }";
  html += ".sensor-label { color: #666; font-size: 14px; }";
  html += ".controls { text-align: center; margin: 20px 0; }";
  html += ".btn { padding: 10px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
  html += ".btn-primary { background: #007bff; color: white; }";
  html += ".btn-success { background: #28a745; color: white; }";
  html += ".btn-danger { background: #dc3545; color: white; }";
  html += ".btn-secondary { background: #6c757d; color: white; }";
  html += ".btn:hover { opacity: 0.8; }";
  html += ".status { text-align: center; margin: 10px 0; padding: 10px; border-radius: 5px; }";
  html += ".status.active { background: #d4edda; color: #155724; }";
  html += ".status.inactive { background: #f8d7da; color: #721c24; }";
  html += ".data-table { width: 100%; border-collapse: collapse; margin: 20px 0; }";
  html += ".data-table th, .data-table td { border: 1px solid #ddd; padding: 8px; text-align: left; }";
  html += ".data-table th { background: #f2f2f2; }";
  html += ".calibration { background: #fff3cd; padding: 15px; border-radius: 8px; margin: 20px 0; }";
  html += ".input-group { margin: 10px 0; }";
  html += ".input-group label { display: inline-block; width: 150px; }";
  html += ".input-group input { padding: 5px; border: 1px solid #ccc; border-radius: 3px; width: 100px; }";
  html += "@media (max-width: 768px) { .sensor-grid { grid-template-columns: 1fr; } }";
  html += "</style></head><body>";
  html += "<div class=\"container\">";
  html += "<div class=\"header\">";
  html += "<h1>ESP32 Water Sensors Dashboard</h1>";
  html += "<p>Real-time monitoring of water flow and turbidity</p>";
  html += "</div>";
  html += "<div class=\"sensor-grid\">";
  html += "<div class=\"sensor-card\"><div class=\"sensor-label\">Flow Rate</div>";
  html += "<div class=\"sensor-value\" id=\"flowRate\">0.000 L/min</div></div>";
  html += "<div class=\"sensor-card\"><div class=\"sensor-label\">Total Volume</div>";
  html += "<div class=\"sensor-value\" id=\"totalVolume\">0.000 L</div></div>";
  html += "<div class=\"sensor-card\"><div class=\"sensor-label\">Flow Voltage</div>";
  html += "<div class=\"sensor-value\" id=\"flowVoltage\">0.000 V</div></div>";
  html += "<div class=\"sensor-card\"><div class=\"sensor-label\">Flow Raw</div>";
  html += "<div class=\"sensor-value\" id=\"flowRaw\">0</div></div>";
  html += "<div class=\"sensor-card\"><div class=\"sensor-label\">Turbidity</div>";
  html += "<div class=\"sensor-value\" id=\"ntu\">0.0 NTU</div></div>";
  html += "<div class=\"sensor-card\"><div class=\"sensor-label\">Turbidity Voltage</div>";
  html += "<div class=\"sensor-value\" id=\"turbidityVoltage\">0.000 V</div></div>";
  html += "<div class=\"sensor-card\"><div class=\"sensor-label\">Turbidity Raw</div>";
  html += "<div class=\"sensor-value\" id=\"turbidityRaw\">0</div></div>";
  html += "<div class=\"sensor-card\"><div class=\"sensor-label\">Flow Rate (Hourly)</div>";
  html += "<div class=\"sensor-value\" id=\"lHour\">0 L/h</div></div>";
  html += "</div>";
  html += "<div class=\"status\" id=\"status\">Data Collection: Inactive</div>";
  html += "<div class=\"controls\">";
  html += "<button class=\"btn btn-success\" onclick=\"startCollection()\">Start Data Collection</button>";
  html += "<button class=\"btn btn-danger\" onclick=\"stopCollection()\">Stop Data Collection</button>";
  html += "<button class=\"btn btn-secondary\" onclick=\"clearData()\">Clear Data</button>";
  html += "</div>";
  html += "<div class=\"controls\">";
  html += "<button class=\"btn btn-primary\" onclick=\"exportCSV()\">Export CSV</button>";
  html += "<button class=\"btn btn-primary\" onclick=\"exportJSON()\">Export JSON</button>";
  html += "<button class=\"btn btn-primary\" onclick=\"exportTXT()\">Export TXT</button>";
  html += "</div>";
  html += "<div class=\"calibration\">";
  html += "<h3>Turbidity Sensor Calibration</h3>";
  html += "<div class=\"input-group\">";
  html += "<label>Clear Water Voltage:</label>";
  html += "<input type=\"number\" id=\"clearWater\" step=\"0.01\" value=\"1.46\">";
  html += "<span>V</span></div>";
  html += "<div class=\"input-group\">";
  html += "<label>Blocked Voltage:</label>";
  html += "<input type=\"number\" id=\"blocked\" step=\"0.01\" value=\"0.07\">";
  html += "<span>V</span></div>";
  html += "<div class=\"input-group\">";
  html += "<label>Air Voltage Min:</label>";
  html += "<input type=\"number\" id=\"airMin\" step=\"0.01\" value=\"1.15\">";
  html += "<span>V</span></div>";
  html += "<div class=\"input-group\">";
  html += "<label>Air Voltage Max:</label>";
  html += "<input type=\"number\" id=\"airMax\" step=\"0.01\" value=\"1.30\">";
  html += "<span>V</span></div>";
  html += "<button class=\"btn btn-primary\" onclick=\"calibrate()\">Apply Calibration</button>";
  html += "</div>";
  html += "<div><h3>Recent Data Points: <span id=\"dataCount\">0</span></h3>";
  html += "<table class=\"data-table\" id=\"dataTable\">";
  html += "<thead><tr><th>Time</th><th>Flow (L/min)</th><th>Volume (L)</th><th>Flow V</th><th>Flow Raw</th><th>Turb V</th><th>Turb Raw</th><th>NTU</th></tr></thead>";
  html += "<tbody id=\"dataBody\"></tbody></table></div></div>";
  html += "<script>";
  html += "let isCollecting = false;";
  html += "function updateData() {";
  html += "fetch('/api/data').then(response => response.json()).then(data => {";
  html += "document.getElementById('flowRate').textContent = data.flowRate.toFixed(3) + ' L/min';";
  html += "document.getElementById('totalVolume').textContent = data.totalVolume.toFixed(3) + ' L';";
  html += "document.getElementById('flowVoltage').textContent = data.flowVoltage.toFixed(3) + ' V';";
  html += "document.getElementById('flowRaw').textContent = data.flowRaw;";
  html += "document.getElementById('lHour').textContent = data.lHour + ' L/h';";
  html += "document.getElementById('ntu').textContent = data.ntu.toFixed(1) + ' NTU';";
  html += "document.getElementById('turbidityVoltage').textContent = data.turbidityVoltage.toFixed(3) + ' V';";
  html += "document.getElementById('turbidityRaw').textContent = data.turbidityRaw;";
  html += "document.getElementById('dataCount').textContent = data.dataPointCount;";
  html += "const statusEl = document.getElementById('status');";
  html += "if(data.dataCollectionActive) {";
  html += "statusEl.textContent = 'Data Collection: Active (' + data.dataPointCount + ' points)';";
  html += "statusEl.className = 'status active';";
  html += "} else {";
  html += "statusEl.textContent = 'Data Collection: Inactive (' + data.dataPointCount + ' points)';";
  html += "statusEl.className = 'status inactive';}";
  html += "});";
  html += "if(isCollecting) { updateDataTable(); }}";
  html += "function updateDataTable() {";
  html += "fetch('/api/datalog').then(response => response.json()).then(data => {";
  html += "const tbody = document.getElementById('dataBody');";
  html += "tbody.innerHTML = '';";
  html += "const recentData = data.slice(-10);";
  html += "recentData.forEach(point => {";
  html += "const row = tbody.insertRow();";
  html += "const time = new Date(point.timestamp);";
  html += "row.insertCell(0).textContent = time.toLocaleTimeString();";
  html += "row.insertCell(1).textContent = point.flowRate.toFixed(3);";
  html += "row.insertCell(2).textContent = point.totalVolume.toFixed(3);";
  html += "row.insertCell(3).textContent = point.flowVoltage.toFixed(3);";
  html += "row.insertCell(4).textContent = point.flowRaw;";
  html += "row.insertCell(5).textContent = point.turbidityVoltage.toFixed(3);";
  html += "row.insertCell(6).textContent = point.turbidityRaw;";
  html += "row.insertCell(7).textContent = point.ntu.toFixed(1);";
  html += "});});}";
  html += "function startCollection() {";
  html += "fetch('/api/start').then(() => {";
  html += "isCollecting = true;";
  html += "alert('Data collection started!');});}";
  html += "function stopCollection() {";
  html += "fetch('/api/stop').then(() => {";
  html += "isCollecting = false;";
  html += "alert('Data collection stopped!');});}";
  html += "function clearData() {";
  html += "if(confirm('Are you sure you want to clear all data?')) {";
  html += "fetch('/api/clear').then(() => {";
  html += "alert('Data cleared!');";
  html += "document.getElementById('dataBody').innerHTML = '';";
  html += "});}}";
  html += "function exportCSV() { window.open('/api/export/csv', '_blank'); }";
  html += "function exportJSON() { window.open('/api/export/json', '_blank'); }";
  html += "function exportTXT() { window.open('/api/export/txt', '_blank'); }";
  html += "function calibrate() {";
  html += "const clearWater = document.getElementById('clearWater').value;";
  html += "const blocked = document.getElementById('blocked').value;";
  html += "const airMin = document.getElementById('airMin').value;";
  html += "const airMax = document.getElementById('airMax').value;";
  html += "const params = new URLSearchParams({";
  html += "clearWater: clearWater, blocked: blocked, airMin: airMin, airMax: airMax});";
  html += "fetch('/api/calibrate', {";
  html += "method: 'POST',";
  html += "headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
  html += "body: params";
  html += "}).then(() => { alert('Calibration updated successfully!'); });}";
  html += "setInterval(updateData, 1000);";
  html += "updateData();";
  html += "</script></body></html>";
  return html;
}

// WiFi Setup Function (plantech implementation)
void setupAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ap_ssid, ap_password);

  Serial.print("Access Point IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_password);
}