#include "ESPUI.h"

#include "uploadDataIndex.h"

#include "uploadDataNormalize.h"
#include "uploadDataStyle.h"

#include "uploadDataControls.h"
#include "uploadDataSlider.h"
#include "uploadDataZepto.h"

#include <ESPAsyncWebServer.h>
#include <functional>

// ################# Spiffs functions
#if defined(ESP32)
void listDir(const char *dirname, uint8_t levels) {
  ESPUI.log("Listing directory: " + String(dirname));

  File root = SPIFFS.open(dirname);

  if (!root) {
    ESPUI.log("Failed to open directory");
    return;
  }

  if (!root.isDirectory()) {
    ESPUI.log("Not a directory");
    return;
  }

  File file = root.openNextFile();

  while (file) {
    if (file.isDirectory()) {
      ESPUI.log("  DIR : " + String(file.name()));
      if (levels) {
        listDir(file.name(), levels - 1);
      }
    } else {
      ESPUI.log("  FILE: " + String(file.name()));
      ESPUI.log("  SIZE: " + String(file.size()));
    }

    file = root.openNextFile();
  }
}
#else

void listDir(const char *dirname, uint8_t levels) {
  // ignoring levels for esp8266
  ESPUI.log("Listing directory: " + String(dirname));

  String str = "";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    ESPUI.log("  FILE: " + String(dir.fileName()));
    ESPUI.log("  SIZE: " + String(dir.fileSize()));
  }
}

#endif

void ESPUIClass::list() {
  if (!SPIFFS.begin()) {
    ESPUI.log("SPIFFS Mount Failed");
    return;
  }
  listDir("/", 1);
#if defined(ESP32)
  ESPUI.log(String(SPIFFS.totalBytes()));
  ESPUI.log(String(SPIFFS.usedBytes()));
#else
  FSInfo fs_info;
  SPIFFS.info(fs_info);

  ESPUI.log(String(fs_info.totalBytes));
  ESPUI.log(String(fs_info.usedBytes));
#endif
}

void deleteFile(const char *path) {
  Serial.print(SPIFFS.exists(path));
  if (!SPIFFS.exists(path)) {
    ESPUI.log("File: " + String(path) + " does not exist, not deleting");
    return;
  }

  ESPUI.log("Deleting file: " + String(path));

  if (SPIFFS.remove(path)) {
    ESPUI.log("File deleted");
  } else {
    ESPUI.log("Delete failed");
  }
}

void writeFile(const char *path, const char *data) {
  ESPUI.log("Writing file: " + String(path));

  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    ESPUI.log("Failed to open file for writing");
    return;
  }
  if (file.print(FPSTR(data))) {
    ESPUI.log("File written");
  } else {
    ESPUI.log("Write failed");
  }
  file.close();
}

// end Spiffs functions

void ESPUIClass::prepareFileSystem() {
  // this function should only be used once

  log("About to prepare filesystem...");

#if defined(ESP32)
  SPIFFS.format();
  if (!SPIFFS.begin(true)) {
    log("SPIFFS Mount Failed");
    return;
  }
  listDir("/", 1);
  log("SPIFFS Mount ESP32 Done");
#else
  SPIFFS.format();
  SPIFFS.begin();
  log("SPIFFS Mount ESP8266 Done");
#endif

  // TODO: This is a workaround, have to find out why SPIFFS on ESP32 behaves
  // incredibly strangely, see issue #6
  /*
  deleteFile("/index.htm");

  deleteFile("/css/style.css");
  deleteFile("/css/normalize.css");

  deleteFile("/js/zepto.min.js");
  deleteFile("/js/controls.js");
  deleteFile("/js/slider.js");
  */

  log("Cleanup done");

  // Now write
  writeFile("/index.htm", HTML_INDEX);

  writeFile("/css/style.css", CSS_STYLE);
  writeFile("/css/normalize.css", CSS_NORMALIZE);

  writeFile("/js/zepto.min.js", JS_ZEPTO);
  writeFile("/js/controls.js", JS_CONTROLS);
  writeFile("/js/slider.js", JS_SLIDER);

  log("Done Initializing filesystem :-)");

#if defined(ESP32)
  listDir("/", 1);
#endif

  SPIFFS.end();
}

// Handle Websockets Communication
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
  case WS_EVT_DISCONNECT:
    ESPUI.log("Disconnected!\n");
    break;
  case WS_EVT_CONNECT: {
    ESPUI.log("Connected: " + String(client->id()));

    ESPUI.jsonDom(client);
    ESPUI.log("JSON Data Sent to Client!");
  } break;
  case WS_EVT_DATA:
    String msg = "";
    for (size_t i = 0; i < len; i++) {
      msg += (char)data[i];
    }
    int id = msg.substring(msg.lastIndexOf(':') + 1).toInt();
    if (id >= ESPUI.cIndex) {
      ESPUI.log("Malformated id in websocket message");
      return;
    }
    Control *c =
        ESPUI.controls[msg.substring(msg.lastIndexOf(':') + 1).toInt()];

    if (msg.startsWith("bdown:")) {
      c->callback(*c, B_DOWN);
    } else if (msg.startsWith("bup:")) {
      c->callback(*c, B_UP);
    } else if (msg.startsWith("pfdown:")) {
      c->callback(*c, P_FOR_DOWN);
    } else if (msg.startsWith("pfup:")) {
      c->callback(*c, P_FOR_UP);
    } else if (msg.startsWith("pldown:")) {
      c->callback(*c, P_LEFT_DOWN);
    } else if (msg.startsWith("plup:")) {
      c->callback(*c, P_LEFT_UP);
    } else if (msg.startsWith("prdown:")) {
      c->callback(*c, P_RIGHT_DOWN);
    } else if (msg.startsWith("prup:")) {
      c->callback(*c, P_RIGHT_UP);
    } else if (msg.startsWith("pbdown:")) {
      c->callback(*c, P_BACK_DOWN);
    } else if (msg.startsWith("pbup:")) {
      c->callback(*c, P_BACK_UP);
    } else if (msg.startsWith("pcdown:")) {
      c->callback(*c, P_CENTER_DOWN);
    } else if (msg.startsWith("pcup:")) {
      c->callback(*c, P_CENTER_UP);
    } else if (msg.startsWith("sactive:")) {
      ESPUI.updateSwitcher(c->id, true);
      c->callback(*c, S_ACTIVE);
    } else if (msg.startsWith("sinactive:")) {
      ESPUI.updateSwitcher(c->id, false);
      c->callback(*c, S_INACTIVE);
    } else if (msg.startsWith("slvalue:")) {
      int value =
          msg.substring(msg.indexOf(':') + 1, msg.lastIndexOf(':')).toInt();
      ESPUI.updateSlider(c->id, value, client->id());
      c->callback(*c, SL_VALUE);
    }
    break;
  }
}

void ESPUIClass::label(const char *label, int color, String value) {
  if (labelExists(label)) {
    log("UI ERROR: Element " + String(label) +
                     " exists, skipping creating element!");
    return;
  }

  Control *newL = new Control();
  newL->type = UI_LABEL;
  newL->label = label;
  newL->color = color;
  if (value != "")
    newL->value = value; // Init with labeltext
  else
    newL->value = String(label);
  newL->callback = NULL;
  newL->id = cIndex;
  controls[cIndex] = newL;
  cIndex++;
}

// TODO: this still needs a range setting
void ESPUIClass::slider(const char *label, void (*callBack)(Control, int),
                        int color, String value) {
  if (labelExists(label)) {
    log("UI ERROR: Element " + String(label) +
                     " exists, skipping creating element!");
    return;
  }

  Control *newSL = new Control();
  newSL->type = UI_SLIDER;
  newSL->label = label;
  newSL->color = color;
  if (value != "")
    newSL->value = value;
  else
    newSL->value = ""; // TODO: init with half value
  newSL->callback = callBack;
  newSL->id = cIndex;
  controls[cIndex] = newSL;
  cIndex++;
}

void ESPUIClass::button(const char *label, void (*callBack)(Control, int),
                        int color, String value) {
  if (labelExists(label)) {
    log("UI ERROR: Element " + String(label) +
                     " exists, skipping creating element!");
    return;
  }

  Control *newB = new Control();
  newB->type = UI_BUTTON;
  newB->label = label;
  newB->color = color;

  if (value != "")
    newB->value = value; // Init with labeltext
  else
    newB->value = String(label);

  newB->callback = callBack;
  newB->id = cIndex;
  controls[cIndex] = newB;
  cIndex++;
}

void ESPUIClass::switcher(const char *label, bool startState,
                          void (*callBack)(Control, int), int color) {
  if (labelExists(label)) {
    log("UI ERROR: Element " + String(label) +
                     " exists, skipping creating element!");
    return;
  }

  Control *newS = new Control();
  newS->type = UI_SWITCHER;
  newS->label = label;
  newS->color = color;
  newS->value = String(startState);
  newS->callback = callBack;
  newS->id = cIndex;
  controls[cIndex] = newS;
  cIndex++;
}

void ESPUIClass::pad(const char *label, bool center,
                     void (*callBack)(Control, int), int color) {
  if (labelExists(label)) {
    log("UI ERROR: Element " + String(label) +
                     " exists, skipping creating element!");
    return;
  }

  Control *newP = new Control();
  if (center)
    newP->type = UI_CPAD;
  else
    newP->type = UI_PAD;
  newP->label = label;
  newP->color = color;
  newP->callback = callBack;
  newP->id = cIndex;
  controls[cIndex] = newP;
  cIndex++;
}

void ESPUIClass::print(int id, String value) {
  if (id < cIndex && controls[id]->type == UI_LABEL) {
    controls[id]->value = value;
    String json;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["type"] = UPDATE_LABEL;
    root["value"] = value;
    root["id"] = String(id);
    root.printTo(json);
    this->ws->textAll(json);
  } else {
    log(String("Error: ") + String(id) + String(" is no label"));
  }
}

void ESPUIClass::print(String label, String value) {
  if (!labelExists(label)) {
    log("UI ERROR: Element does not " + String(label) +
                     " exist, cannot update!");
    return;
  }
  print(getIdByLabel(label), value);
}

void ESPUIClass::updateSwitcher(int id, bool nValue, int clientId) {
  if (id < cIndex && controls[id]->type == UI_SWITCHER) {
    controls[id]->value = nValue ? 1 : 0;
    String json;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["type"] = UPDATE_SWITCHER;
    root["value"] = nValue ? 1 : 0;
    root["id"] = String(id);
    root.printTo(json);
    textThem(json, clientId);
  } else {
    log(String("Error: ") + String(id) +
                     String(" is no switcher"));
  }
}

void ESPUIClass::updateSlider(int id, int nValue, int clientId) {
  if (id < cIndex && controls[id]->type == UI_SLIDER) {
    controls[id]->value = nValue;
    String json;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["type"] = UPDATE_SLIDER;
    root["value"] = nValue;
    root["id"] = String(id);
    root.printTo(json);
    textThem(json, clientId);
  } else {
    log(String("Error: ") + String(id) + String(" is no slider"));
  }
}

void ESPUIClass::updateSwitcher(String label, bool nValue, int clientId) {
  if (!labelExists(label)) {
    log("UI ERROR: Element does not " + String(label) +
                     " exist, cannot update!");
    return;
  }
  updateSwitcher(getIdByLabel(label), nValue, clientId);
}

// This is a hacky workaround because ESPAsyncWebServer does not have a function
// like this and it's clients array is private
void ESPUIClass::textThem(String text, int clientId) {
  int tryId = 0;
  for (int count = 0; count < this->ws->count();) {
    if (this->ws->hasClient(tryId)) {
      if (clientId != tryId) {
        this->ws->client(tryId)->text(text);
      }
      count++;
    }
    tryId++;
  }
}

int ESPUIClass::getIdByLabel(String label) {
  for (int i = 0; i < cIndex; i++) {
    if (String(controls[i]->label) == label)
      return i;
  }
  return -1; // failed, nonexistant
}

bool ESPUIClass::labelExists(String label) {
  for (int i = 0; i < cIndex; i++) {
    if (String(controls[i]->label) == label)
      return true;
  }
  return false;
}

// Convert & Transfer Arduino elements to JSON elements
void ESPUIClass::jsonDom(AsyncWebSocketClient *client) {
  for (int i = -1; i < cIndex; i++) {
    String json;
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    if (i == -1) {
      root["type"] = UI_TITEL;
      root["label"] = String(ui_title);
    } else {
      root["type"] = controls[i]->type;
      root["label"] = String(controls[i]->label);
      root["value"] = String(controls[i]->value);
      root["color"] = String(controls[i]->color);
      root["id"] = String(i);
    }
    root.printTo(json);
    client->text(json);
  }
}

void ESPUIClass::begin(const char *_title) {
	begin(_title, true);
}

void ESPUIClass::begin(const char *_title, bool debug) {
  is_debug = debug;
  ui_title = _title;
  server = new AsyncWebServer(80);
  ws = new AsyncWebSocket("/ws");

  if (!SPIFFS.begin()) {
    log("SPIFFS Mount Failed, PLEASE CHECK THE README ON HOW TO "
                   "PREPARE YOUR ESP!!!!!!!");
    return;
  }
	if (debug)
  	listDir("/", 1);

  if (!SPIFFS.exists("/index.htm")) {
    log("Please read the README!!!!!!!, Make sure to "
                   "ESPUI.prepareFileSystem() once in an empty sketch");
    return;
  }

  ws->onEvent(onWsEvent);
  server->addHandler(ws);
  server->serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

  // Heap for general Servertest
  server->on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server->onNotFound(
      [](AsyncWebServerRequest *request) { request->send(404); });

  server->begin();
  log("UI Initialized");
}

void ESPUIClass::log(String str)
{
	if (is_debug)
		Serial.println(str);
}

ESPUIClass ESPUI;
