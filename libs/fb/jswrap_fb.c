#include "jsutils.h"
#include "jsvar.h"
#include "jspin.h"
#include "jswrap_fb.h"
#include "jswrap_spi_i2c.h"
#include "jsspi.h"
#include "jsdevices.h"
#include "jsinteractive.h"
#include "nrf_drv_spi.h"

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

// TODO: fillRect alignment

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
  uint8_t a; // FIXME: pack?
  JsVar* data;
  JsVar* text;
  void* next;
} fb_rect;



fb_rect* cache = NULL;
fb_rect* root = NULL;
uint16_t last_id = 0;
int changed_y1 = 0;
int changed_y2 = 240; 

#define PACK_RGB8_TO_565(r,g,b) ( \
  ((b & 0b11111000) << 5) + \
  (r & 0b11111000) + \
  ((g & 0b11100000) >> 5) + ((g & 0b11100) << 11))

#define UNPACK_565_TO_RGB8(val,r,g,b) \
  int r = (val & 0b11111000); \
  int b = ((val >> 5) & 0b11111000); \
  int g = (((val & 0b111) << 5) + ((val & 0b1110000000000000) >> 11)); 

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

// buf[0] - len1
// buf[1] - len2
// buf[2] - type
// buf[3] - id
uint8_t* find_glyph(uint8_t* buf, uint8_t id) {
  while(true) {
    if(buf[2] != 11) { // type 
      jsExceptionHere(JSET_ERROR, "unknown type %d", buf[2]);
      return 0;
    }; 
    if (buf[3] == id) {
      return buf + 4;
    }
    buf += 2 + (buf[0] << 8) + buf[1];
  }
  return 0;
}

void recalc_text_size(fb_rect* rect, uint16_t* w, uint16_t* h) {
  JSV_GET_AS_CHAR_ARRAY(dataBuf, dataLen, rect->data);
  JSV_GET_AS_CHAR_ARRAY(textBuf, textLen, rect->text);

  w[0] = 0;
  h[0] = 0;
  while(textLen) {
    uint8_t* glyph = find_glyph(dataBuf, textBuf[0]);

    // set height
    int glyphH = glyph[1] + glyph[3]; // height + yoffset
    if (glyphH > h[0]) {
      h[0] = glyphH;
    }

    // set width
    w[0] += glyph[4]; // xadvance

    // TODO: kerning
    textBuf++;
    textLen--;
  }
}

void recalc_image_size(fb_rect* rect) {
  JSV_GET_AS_CHAR_ARRAY(buf, len, rect->data);
  int pt = 0;
  pt++; // len1
  pt++; // len2
  int type = buf[pt++]; // type 
  if(type != 11) {
    jsExceptionHere(JSET_ERROR, "Unknown type %d", type);
    return;
  }; 
  pt++; // id 
  rect->w = buf[pt++]; // width 
  rect->h = buf[pt++]; // height
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
  int x = 0, y = 0, w = 0, h = 0, c = 0, a = 0;
  JsVar* data = 0, *text = 0;
  jsvConfigObject configs[] = {
    {"x", JSV_INTEGER, &x},
    {"y", JSV_INTEGER, &y},
    {"w", JSV_INTEGER, &w},
    {"h", JSV_INTEGER, &h},
    {"c", JSV_INTEGER, &c},
    {"a", JSV_INTEGER, &a},
    {"data", JSV_OBJECT, &data},
    {"text", JSV_OBJECT, &text},
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
  rect->a = a;
  rect->id = last_id++;
  rect->next = NULL;
  rect->data = data;
  rect->text = text;

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

  if (rect->data) {
    if(rect->text) {
      recalc_text_size(rect, &rect->w, &rect->h);
    } else {
      recalc_image_size(rect);
    }
  }

  changed_y1 = min(rect->y, changed_y1);
  changed_y2 = max(rect->y + rect->h, changed_y2);

  return rect->id;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "set",
  "generate" : "jswrap_fb_set",
  "params" : [
    ["id", "int", "id"],
    ["opts","JsVar","Rect options"]
  ],
  "return" : ["int","None"]
}
*/
int jswrap_fb_set(int id, JsVar* opts) {
  fb_rect* rect = root;
  while(rect) {
    if (rect->id == id) break;
    rect = rect->next;
  }
  if (!rect) {
    jsExceptionHere(JSET_ERROR, "Cannot find rect");
    return;
  }

  int x = rect->x;
  int y = rect->y;
  int w = rect->w;
  int h = rect->h;
  int c = rect->c;
  int a = rect->a;
  JsVar* data = rect->data;
  JsVar* text = rect->text;

  jsvConfigObject configs[] = {
    {"x", JSV_INTEGER, &x},
    {"y", JSV_INTEGER, &y},
    {"w", JSV_INTEGER, &w},
    {"h", JSV_INTEGER, &h},
    {"c", JSV_INTEGER, &c},
    {"a", JSV_INTEGER, &a},
    {"data", JSV_OBJECT, &data},
    {"text", JSV_OBJECT, &text},
  };
  if (!jsvReadConfigObject(opts, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Invalid options object");
    return 0;
  }
  
  rect->x = x;
  rect->y = y;
  rect->w = w;
  rect->h = h;
  rect->c = c;
  rect->a = a;
  rect->data = data;
  rect->text = text;

  // TODO: only recalc size if values have changed

  if (rect->data) {
    if(rect->text) {
      recalc_text_size(rect, &rect->w, &rect->h);
    } else {
      recalc_image_size(rect);
    }
  }

  changed_y1 = min(rect->y, changed_y1);
  changed_y2 = max(rect->y + rect->h, changed_y2);

  return;
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
  if ((rect->y + rect->h) < y) return;
  int x1 = rect->x < 0 ? 0 : rect->x;
  int x2 = rect->x + rect->w >= 240 ? 240 : rect->x + rect->w;
  int c = rect->c;
  line += x1;
  for(; x1<x2; x1++) {
    line[0] = c;
    line++;
  }
}

typedef struct {
  uint8_t* pt;
  uint8_t rle;
  uint16_t color;
} img_info;

img_info structs[50];
int struct_counter = 0;

// buf[0] - width
// buf[1] - height
// buf[2] - x offset
// buf[3] - y offset
// buf[4] - x advance
// buf[5] - kernings count
int render_buffer(int X, int Y, int C, uint8_t* buf, uint8_t prevId, uint16_t* line, int lineY, img_info *info) {
  int W = buf[0];
  int H = buf[1];
  X += buf[2];
  Y += buf[3];
  int xadv = buf[4];
  
  if (lineY < Y) return xadv;
  if (lineY == Y) {
    int numKerns = buf[5]; // number of kernings
    info->pt = buf + 6 + numKerns * 2;
  // TODO: kernings
    info->color = 0;
    info->rle = 0;
  } else {
    if (lineY >= Y + H) return xadv;
  }

  UNPACK_565_TO_RGB8(C, cr, cg, cb);

  int color = info->color;
  int rle = info->rle;
  buf = info->pt;

  for(int x = 0; x < W; x++) {
    if (rle == 0) {
      color = *buf++;
      if (color & 0b10000000) {
        color &= 0b111111;
        rle = *buf++ - 1;
      }
    } else {
      rle--;
    }

    int screenX = x + X;
    if (screenX < 0) continue;
    if (screenX >= 240) continue;

    int cl = color << 2;
    int ncl = (0b111111 - color) << 2;
    UNPACK_565_TO_RGB8(line[screenX], or, og, ob);
    int rr = ((ncl * or) + (cr * cl)) >> 8;
    int gg = ((ncl * og) + (cg * cl)) >> 8;
    int bb = ((ncl * ob) + (cb * cl)) >> 8;
    line[screenX] = PACK_RGB8_TO_565(rr, gg, bb);
  }

  info->color = color;
  info->rle = rle;
  info->pt = buf;

  return xadv;
}

void render_image(fb_rect* rect, uint16_t* line, int lineY) {
  struct_counter++;
  if (rect->y > lineY) return;
  if ((rect->y + rect->h) < lineY) return;

  JSV_GET_AS_CHAR_ARRAY(buf, len, rect->data);
    // rect align
  int xstart = rect->x;
  if (rect->a == 1) {
    xstart -= (rect->w >> 1);
  }
  if (rect->a == 2) {
    xstart -= rect->w;
  }

  uint8_t* glyph = find_glyph(buf, 0);
  render_buffer(xstart, rect->y, rect->c, glyph, 0, line, lineY, &structs[struct_counter - 1]);
}

void render_text(fb_rect* rect, uint16_t* line, int lineY) {
  int ss = struct_counter;
  JSV_GET_AS_CHAR_ARRAY(text, textLen, rect->text);
  struct_counter+=textLen;

  if (rect->y > lineY) return;
  if ((rect->y + rect->h) < lineY) return;
  JSV_GET_AS_CHAR_ARRAY(data, dataLen, rect->data);

  // rect align
  int xstart = rect->x;
  if (rect->a == 1) {
    xstart -= (rect->w >> 1);
  }
  if (rect->a == 2) {
    xstart -= rect->w;
  }

  int prevId = 0;
  while(textLen) {
    uint8_t* glyph = find_glyph(data, text[0]);
    xstart += render_buffer(xstart, rect->y, rect->c, glyph, prevId, line, lineY, &structs[ss++]);
    prevId = text[0];
    text++;
    textLen--;
  }
}

void callback() {};

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "send",
  "generate" : "jswrap_fb_send",
  "params" : [
    ["spi","JsVar","SPI interface to send data"],
    ["cmd", "pin", "dc"]
  ],
  "return" : ["int","None"]
}
*/
int jswrap_fb_send(JsVar* interface, Pin dc) {
  IOEventFlags device = jsiGetDeviceFromClass(interface);

  if (changed_y2 < changed_y1) return;
  changed_y1 = max(0, changed_y1);
  changed_y2 = min(240, changed_y2);

  uint8_t ch[5] = { 0x2B, 0, changed_y1, 0, changed_y2 };
  jshPinOutput(dc, false);
  jshSPISendMany(device, ch, NULL, 1, NULL);
  jshPinOutput(dc, true);
  jshSPISendMany(device, ch+1, NULL, 4, NULL);

  ch[0] = 0x2C;
  jshPinOutput(dc, false);
  jshSPISendMany(device, ch, NULL, 1, callback);
  jshPinOutput(dc, true);

  uint16_t line1[240];
  uint16_t line2[240];
  uint16_t color;

  for(int y = changed_y1; y < changed_y2; y++) {
    uint16_t* line = y & 0b1 ? line1 : line2;
    for(int x = 0; x < 240; x++) {
      line[x] = 0;
    }

    struct_counter = 0;

    fb_rect* pt = root;
    while(pt) {
      if (pt->data) {
        if (pt->text) {
          render_text(pt, line, y);
        } else {
          render_image(pt, line, y);
        }
      } else {
        render_rect(pt, line, y);
      }
      pt = pt->next;
    }

    jshSPISendMany(device, line, NULL, 480,  callback);
  }

  changed_y1 = 240;
  changed_y2 = 0;

  return struct_counter;
}
