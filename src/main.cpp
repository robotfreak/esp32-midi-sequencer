// === ESP1 Master Sequencer mit Webinterface und MIDI-über-ESP-NOW Broadcast ===

#include <WiFi.h>
#include <esp_now.h>
#include <ESPAsyncWebServer.h>

// === WLAN (für Webinterface) ===
const char* ssid = "BeatBot";
const char* password = "12345678";
AsyncWebServer server(80);

// === MIDI- und Pattern-Daten ===
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t step = 0;
unsigned long lastStepTime = 0;
unsigned long stepInterval = 200;
bool kick[8]  = {1,0,0,0,1,0,0,0};
bool snare[8] = {0,0,1,0,0,0,1,0};
bool hat[8]   = {1,1,1,1,1,1,1,1};
bool clap[8]  = {0,0,0,0,0,0,0,0};
bool isRecording = false;

// === MIDI senden ===
void sendMidiNote(uint8_t note, uint8_t velocity) {
  uint8_t data[] = {0x99, note, velocity};
  esp_now_send(broadcastAddress, data, sizeof(data));
}

void playStep(uint8_t s) {
  if (kick[s]) sendMidiNote(36, 127);   // Kick
  if (snare[s]) sendMidiNote(38, 127);  // Snare
  if (hat[s]) sendMidiNote(42, 100);    // Closed Hihat
  if (clap[s]) sendMidiNote(39, 100);   // Clap
}

void parsePattern(String data, bool *track) {
  for (int i = 0; i < 8 && i < data.length(); i++) {
    track[i] = (data[i] == '1');
  }
}

String htmlPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<style>body{font-family:sans-serif;} .step{width:30px;height:30px;margin:2px;display:inline-block;text-align:center;line-height:30px;border:1px solid #ccc;cursor:pointer;} .on{background:#0c0;color:#fff;} button{margin:5px;}</style>"
                "<script>
                let kick = [" + String(kick[0])+","+String(kick[1])+","+String(kick[2])+","+String(kick[3])+","+String(kick[4])+","+String(kick[5])+","+String(kick[6])+","+String(kick[7])+ "];
                let snare = [" + String(snare[0])+","+String(snare[1])+","+String(snare[2])+","+String(snare[3])+","+String(snare[4])+","+String(snare[5])+","+String(snare[6])+","+String(snare[7])+ "];
                let hat = [" + String(hat[0])+","+String(hat[1])+","+String(hat[2])+","+String(hat[3])+","+String(hat[4])+","+String(hat[5])+","+String(hat[6])+","+String(hat[7])+ "];
                let clap = [" + String(clap[0])+","+String(clap[1])+","+String(clap[2])+","+String(clap[3])+","+String(clap[4])+","+String(clap[5])+","+String(clap[6])+","+String(clap[7])+ "];
                function toggle(track,i){eval(track)[i]=eval(track)[i]?0:1;draw();}
                function draw(){
                  ['kick','snare','hat','clap'].forEach(track=>{
                    let html='';
                    for(let i=0;i<8;i++){
                      html += `<div class='step ${eval(track)[i] ? 'on' : ''}' onclick='toggle("${track}",${i})'>${i+1}</div>`;
                    }
                    document.getElementById(track).innerHTML = html;
                  });
                }
                function send(){
                  fetch('/update',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
                  body:`kick=${kick.join('')}&snare=${snare.join('')}&hat=${hat.join('')}&clap=${clap.join('')}`});
                }
                function rec(){fetch('/record');}
                window.onload=draw;
                </script></head><body>
                <h2>Beat Sequencer</h2>
                <h3>Kick</h3><div id='kick'></div>
                <h3>Snare</h3><div id='snare'></div>
                <h3>HiHat</h3><div id='hat'></div>
                <h3>Clap</h3><div id='clap'></div>
                <br><button onclick='send()'>Speichern</button>
                <button onclick='rec()'>Aufnahme an/aus</button>
                </body></html>";
  return html;
}

void setupWebUI() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", htmlPage());
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("kick", true)) parsePattern(request->getParam("kick", true)->value(), kick);
    if (request->hasParam("snare", true)) parsePattern(request->getParam("snare", true)->value(), snare);
    if (request->hasParam("hat", true)) parsePattern(request->getParam("hat", true)->value(), hat);
    if (request->hasParam("clap", true)) parsePattern(request->getParam("clap", true)->value(), clap);
    request->send(200, "text/plain", "OK");
  });
  server.on("/record", HTTP_GET, [](AsyncWebServerRequest *request){
    isRecording = !isRecording;
    request->send(200, "text/plain", isRecording ? "Recording ON" : "Recording OFF");
  });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  setupWebUI();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init failed");
    return;
  }
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
}

void loop() {
  unsigned long now = millis();
  if (now - lastStepTime > stepInterval) {
    lastStepTime = now;
    playStep(step);
    step = (step + 1) % 8;
  }
}
