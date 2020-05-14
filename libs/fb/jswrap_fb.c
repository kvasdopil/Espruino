#include "jsutils.h"
#include "jsvar.h"
#include "jspin.h"
#include "jswrap_fb.h"
#include "jswrap_spi_i2c.h"
#include "jsspi.h"
#include "jsdevices.h"
#include "jsinteractive.h"

/*JSON{
  "type" : "library",
  "class" : "fb"
}
Framebuffer library
*/

typedef struct {
  uint16_t id;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  int16_t c;
  void* next;
} fb_rect;

fb_rect* root = NULL;
uint16_t last_id = 0;

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "color",
  "generate" : "jswrap_fb_color",
  "params" : [
    ["r","int","r color"],
    ["g","int","g color"],
    ["b","int","b color"]
  ],
  "return" : ["int","result"]
}
*/
int jswrap_fb_color(int r, int g, int b) {
  return (r & 0b11111000) + (g >> 5) + ((g & 0b11100) << 11) + ((b & 0b11111000) << 5);
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "add",
  "generate" : "jswrap_fb_add",
  "params" : [
    ["opts","JsVar","Rect options"]
  ],
  "return" : ["int","None"]
}
*/
int jswrap_fb_add(JsVar* opts) {
  int x, y, w, h, c;
  jsvConfigObject configs[] = {
    {"x", JSV_INTEGER, &x},
    {"y", JSV_INTEGER, &y},
    {"w", JSV_INTEGER, &w},
    {"h", JSV_INTEGER, &h},
    {"c", JSV_INTEGER, &c},
  };
  if (!jsvReadConfigObject(opts, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Invalid options object");
    return 0;
  }

  fb_rect* rect = calloc(1, sizeof(fb_rect));
  rect->x = x;
  rect->y = y;
  rect->w = w;
  rect->h = h;
  rect->c = c;
  rect->id = last_id++;
  rect->next = NULL;

  // add rect to list
  if(root == 0) {
    root = rect;
  } else {
    fb_rect* pt = root;
    while(pt->next) {
      pt = pt->next;
    }
    pt->next = rect;
  }

  return rect->id;
}


/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "init",
  "generate" : "jswrap_fb_init",
  "params" : [
  ],
  "return" : ["int","None"]
}
*/
int jswrap_fb_init() {
  root = NULL; // FIXME: reuse rects
  return 0;
}

void execSpiCmd(IOEventFlags device, uint8_t* data, int len, int dclen, Pin dc) {
  jshPinOutput(dc, false);
  jshSPISendMany(device, data, NULL, dclen, NULL);
  jshSPIWait(device);
  jshPinOutput(dc, true);

  if (len > dclen) {
    jshSPISendMany(device, data + dclen, NULL, len - dclen, NULL);
  }
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "cmd",
  "generate" : "jswrap_fb_cmd",
  "params" : [
    ["spi","JsVar","SPI interface to send data"],
    ["cmd", "JsVar", "Data"],
    ["dclen", "int", "Dc bytes length"],
    ["dc", "pin", "dc pin"]
  ],
  "return" : ["int","None"]
}
*/
int jswrap_fb_cmd(JsVar* interface, JsVar *cmd1, int dclen, Pin dc) {
  IOEventFlags device = jsiGetDeviceFromClass(interface);

  JSV_GET_AS_CHAR_ARRAY(cmdData, cmdLen, cmd1);
  execSpiCmd(device, cmdData, cmdLen, dclen, dc);
  
  return 0;
}

inline void render_rect(fb_rect* rect, uint16_t* line, int y) {
  if (rect->y > y) return;
  if ((rect->y + rect->w) < y) return;
  int x1 = rect->x < 0 ? 0 : rect->x;
  int x2 = rect->x + rect->w >= 240 ? 240 : rect->x + rect->w;
  int c = rect->c;
  line += x1;
  for(; x1<x2; x1++) {
    line[0] = c;
    line++;
  }
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "send",
  "generate" : "jswrap_fb_send",
  "params" : [
    ["spi","JsVar","SPI interface to send data"],
    ["cmd", "JsVar", "Data"]
  ],
  "return" : ["int","None"]
}
*/
int jswrap_fb_send(JsVar* interface, JsVar *cmd1) {
  IOEventFlags device = jsiGetDeviceFromClass(interface);

  uint16_t line[240];
  uint16_t color;

  for(int y = 0; y < 240; y++) {
    for(int x = 0; x < 240; x++) {
      line[x] = 0;
    }

    fb_rect* pt = root;
    while(pt) {
      render_rect(pt, line, y);
      pt = pt->next;
    }

    // line[x] = color;
    jshSPISendMany(device, line, NULL, 240, NULL);
    jshSPISendMany(device, line+120, NULL, 240, NULL);
  }

  return 0;
}
