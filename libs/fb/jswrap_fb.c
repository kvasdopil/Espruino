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
  JsVar* data;
  void* next;
} fb_rect;

fb_rect* cache = NULL;
fb_rect* root = NULL;
uint16_t last_id = 0;

#define PACK_RGB8_TO_565(r,g,b) ( \
  ((b & 0b11111000) << 5) + \
   (r & 0b11111000) + \
  ((g & 0b11100000) >> 5) + ((g & 0b11100) << 11))
#define UNPACK_565_TO_RGB8(val,r,g,b) \
  int r = (val & 0b11111000); \
  int b = (val >> 5) & 0b11111000; \
  int g = (((val & 0b111) << 5) + ((val & 0b1110000000000000) >> 11))
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
  return PACK_RGB8_TO_565(r, g, b);
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
  int x = 0, y = 0, w = 0, h = 0, c = 0;
  JsVar* data = 0;
  jsvConfigObject configs[] = {
    {"x", JSV_INTEGER, &x},
    {"y", JSV_INTEGER, &y},
    {"w", JSV_INTEGER, &w},
    {"h", JSV_INTEGER, &h},
    {"c", JSV_INTEGER, &c},
    {"data", JSV_OBJECT, &data}
  };
  if (!jsvReadConfigObject(opts, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Invalid options object");
    return 0;
  }

  fb_rect* rect;
  if (cache) {
    rect = cache;
    cache = cache->next;
  } else {
    rect = calloc(1, sizeof(fb_rect));
  }

  rect->x = x;
  rect->y = y;
  rect->w = w;
  rect->h = h;
  rect->c = c;
  rect->id = last_id++;
  rect->next = NULL;
  rect->data = data;

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
  while(root) {
    fb_rect* item = root;
    root = root->next;

    item->next = cache;
    cache = item;
  }
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

void render_image(fb_rect* rect, uint16_t* line, int lineY) {
  // if (rect->y > lineY) return;
  // if ((rect->y + rect->w) < y) return;
  // int x1 = rect->x < 0 ? 0 : rect->x;
  // int x2 = rect->x + rect->w >= 240 ? 240 : rect->x + rect->w;
  
  // int c = 0xff;
  // line += x1;
  // for(; x1<x2; x1++) {
  //   line[0] = c;
  //   line++;
  // }

  JSV_GET_AS_CHAR_ARRAY(buf, len, rect->data);
  int pt = 0;
  pt++; // len1
  pt++; // len2
  int type = buf[pt++]; // type 
  if(type != 11) {
    jsExceptionHere(JSET_ERROR, "unknown type %d", type);
    return;
  }; 
  pt++; // id 
  rect->w = buf[pt++]; // width 
  rect->h = buf[pt++]; // height
 
  pt++; // x offset
  pt++; // y offset
  pt++; // x advance
  int numKerns = buf[pt++]; // number of kernings
  while(numKerns) {
    pt+=2;
    numKerns--;
  }

  int color = 0;
  int rle = 0;
  for(int y = 0; y < rect->h; y++) {
    for(int x = 0; x < rect->w; x++) {
      if (rle == 0) {
        color = buf[pt++];
        if (color & 0b10000000) {
          color &= 0b111111;
          rle = buf[pt++] - 1;
        }
      } else {
        rle--;
      }

      if (y + rect->y == lineY) {
        UNPACK_565_TO_RGB8(line[x], or, og, ob);
        int rr = or + (color << 2);
        int gg = og + (color << 2);
        int bb = ob + (color << 2);
        rr = rr > 0xff ? 0xff : rr;
        gg = gg > 0xff ? 0xff : gg;
        bb = bb > 0xff ? 0xff : bb;
        // gggR RRRR BBBB BGGG
        line[x] = PACK_RGB8_TO_565(rr, gg, bb);
      }
    }
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
      if (pt->data) {
        render_image(pt, line, y);
      } else {
        render_rect(pt, line, y);
      }
      pt = pt->next;
    }

    jshSPISendMany(device, line, NULL, 240, NULL);
    jshSPISendMany(device, line+120, NULL, 240, NULL);
  }

  return 0;
}
