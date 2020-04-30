
#include "jsutils.h"
#include "jsvar.h"
#include "jspin.h"
#include "jshardware.h"
#include "jswrap_fb.h"
#include "jswrap_spim.h"
#include "jsvariterator.h"
#include "jswrap_promise.h"

#include "stdio.h"

/*JSON{
  "type" : "library",
  "class" : "fb"
}
This library provides functions to work with in-memory framebuffer
*/

#define FB_WIDTH 240
#define FB_HEIGHT 240
#define FB_BPP sizeof(uint16_t)
uint16_t fb[FB_WIDTH * FB_HEIGHT];

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

// packed as gggbbbbbrrrrrggg
#define PACK_RGB6_TO_565(r,g,b) ((b >> 1 << 8) + (r >> 1 << 3) + (g >> 3) + (g & 0b111 << 13))
#define UNPACK_565_TO_RGB6(val,r,g,b) \
  int r = (val >> 2) & 0b111110; \
  int b = (val >> 7) & 0b111110; \
  int g = ((val << 3) + (val >> 13)) & 0b111111;

// flag that fb object was modified since last render
bool fb_was_modified = true;

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "color",
  "generate" : "jswrap_fb_color",
  "params" : [
    ["r","int","Red"],
    ["g","int","Green"],
    ["b","int","Blue"]
  ],
  "return" : ["int","color"]
}
Convert color from [r,g,b] to int value
*/
int jswrap_fb_color(int r, int g, int b) {
  return PACK_RGB6_TO_565(r >> 2, g >> 2, b >> 2);
}

typedef struct {
  int id;
  void* next;

  int x;
  int y;
  int w;
  int h;
  int c;

  JsVar* buf;
  JsVar* index;
} fb_rect;

fb_rect* root = NULL;
int next_id = 1;

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "init",
  "generate" : "jswrap_fb_init",
  "params" : [
  ],
  "return" : ["int","nothingd"]
}
Create a rect resource
*/
int jswrap_fb_init() {
  root = NULL;
  next_id = 1;
  return 0;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "add",
  "generate" : "jswrap_fb_add",
  "params" : [
    ["opt","JsVar","rect params"]
  ],
  "return" : ["int","rect id"]
}
Create a rect resource
*/
int jswrap_fb_add(JsVar* opt) {
  fb_was_modified = true;
  fb_rect* rect = calloc(sizeof(fb_rect), 1);

  rect->id = next_id++;
  rect->x = 0;
  rect->y = 0;
  rect->w = 0;
  rect->h = 0;
  rect->c = 0;
  rect->buf = 0;
  rect->index = 0;
  rect->next = NULL;

  // add to end of linked list
  if (!root) {
    root = rect;
  } else {
    fb_rect* last = root;
    while(last->next) {
      last = last->next;
    }
    last->next = rect;
  }

  jsvConfigObject configs[] = {
    {"x", JSV_INTEGER, &(rect->x)},
    {"y", JSV_INTEGER, &(rect->y)},
    {"w", JSV_INTEGER, &(rect->w)},
    {"h", JSV_INTEGER, &(rect->h)},
    {"c", JSV_INTEGER, &(rect->c)},
    {"buf", JSV_ARRAY, &(rect->buf)},
    {"index", JSV_ARRAY, &(rect->index)},
  };
  
  if (!jsvReadConfigObject(opt, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Invalid arguments for rect");
    return 0;
  }

  return rect->id;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "set",
  "generate" : "jswrap_fb_set",
  "params" : [
    ["id","int","rect id"],
    ["opt","JsVar","rect params"]
  ],
  "return" : ["int","nothing"]
}
Update a rect resource
*/
int jswrap_fb_set(int id, JsVar* opt) {
  fb_rect* rect = root;

  // find rect
  while(rect && rect->id != id) {
    rect = rect->next;
  }

  if (!rect) {
    jsExceptionHere(JSET_ERROR, "Resource not found");
    return 0;
  }

  fb_was_modified = true;
  jsvConfigObject configs[] = {
    {"x", JSV_INTEGER, &(rect->x)},
    {"y", JSV_INTEGER, &(rect->y)},
    {"w", JSV_INTEGER, &(rect->w)},
    {"h", JSV_INTEGER, &(rect->h)},
    {"c", JSV_INTEGER, &(rect->c)},
    {"buf", JSV_ARRAY, &(rect->buf)},
    {"index", JSV_ARRAY, &(rect->index)},
  };
  
  if (!jsvReadConfigObject(opt, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Invalid arguments for rect");
    return 0;
  }

  return 0;
}


/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "remove",
  "generate" : "jswrap_fb_remove",
  "params" : [
    ["id","int","rect id"]
  ],
  "return" : ["int","nothing"]
}
Remove a rect resource
*/
int jswrap_fb_remove(int id) {
  // TODO: free()
  fb_rect** rect = &root;
  while (true) {
    if ((*rect) == NULL) {
      return 0;
    }

    if ((*rect)->id == id) {
      *rect = (*rect)->next;
      return 0; 
    }

    rect = &((*rect)->next);
  }
}

void fb_clear(int color) {
  uint16_t* end = fb + FB_WIDTH * FB_HEIGHT;
  uint16_t* ptr = fb;
  while(ptr != end) {
    *ptr = color;
    ptr++;
  }
}

void fb_fill(int x, int y, int w, int h, int color) {
  const int ymin = max(0, y);
  const int ymax = min(y + h, FB_HEIGHT);
  const int xmin = max(0, x);
  const int xmax = min(x + w, FB_WIDTH);
  if (ymax <= ymin || xmax <= xmin) {
    return;
  }

  // TODO: optimize
  for (int yy = ymin; yy<ymax; yy++) {
    for (int xx = xmin; xx<xmax; xx++) {
      fb[xx + yy * FB_WIDTH] = color;
    }
  }
}

int fb_blit(int blit_x, int blit_y, int wcrop, int hcrop, JsVar* buffer, int index, int tint) {
  JSV_GET_AS_CHAR_ARRAY(buf_data, buf_size, buffer);

  // skip all previous glyphs
  uint length = 0;
  while(true) { 
    if (buf_size <= 2) {
      jsExceptionHere(JSET_ERROR, "Buffer too small: %d < 2, glyph %d", buf_size);
      return 0; // something weird happened, buffer is too small
    }
    length = (buf_data[0] << 8) + buf_data[1];
    buf_data += 2;
    buf_size -= 2;
    if (buf_size < length) {
      jsExceptionHere(JSET_ERROR, "Buffer too small: %d < %d", buf_size, length);
      return 0; // something weird happened, buffer is too small
    }
    if (index == 0) {
      break;
    }

    buf_data += length;
    buf_size -= length;
    index--;
  }

  uint8_t glyph_type = buf_data[0];
  if (glyph_type != 1) { // 1 is grayscale, RLE-zipped
    jsExceptionHere(JSET_ERROR, "Unknown type of bitmap: %d", glyph_type);
    return 0;
  }
  uint8_t w = buf_data[1]; // glyph width

  int x = 0;
  int y = 0;

  UNPACK_565_TO_RGB6(tint, tint_r, tint_g, tint_b);
  int br = 0;
  int rle = 0;
  
  uint8_t* glyph_end = buf_data + length;
  buf_data += 2; // skip header, now we are pointing at RLE-compressed data
  while(buf_data < glyph_end) {
    if (rle <= 0) {
      br = buf_data[0];
      buf_data++;
      if (br & 0b10000000) { // rle flag
        br &= 0b111111; // set color value
        rle = buf_data[0] - 1; // length of zipped fragment
        buf_data++;
      }
    } else {
      rle--;
    }

    int tx = blit_x + x;
    int ty = blit_y + y;
    if (tx >= 0 && ty >= 0 && tx < FB_WIDTH && ty < FB_HEIGHT) {
      fb[tx + ty*FB_WIDTH] = PACK_RGB6_TO_565((br * tint_r) >> 6, (br  * tint_g) >> 6, (br * tint_b) >> 6); 
    }
  
    x++;
    if (x == w) {
      x = 0;
      y++;
    }
  }

  return w;
}

bool fb_flip() {
  int half_length = FB_WIDTH * 120;
  uint8_t b1[] = {0x2A, 0, 0, FB_HEIGHT && 0xff, FB_HEIGHT >> 8};
  uint8_t b2[] = {0x2B, 0, 0, FB_WIDTH && 0xff, FB_WIDTH >> 8};
  uint8_t b3[] = {0x2C};

  // FIXME: add error check here
  spim_send_sync(b1, 5, 1);
  spim_send_sync(b2, 5, 1);
  spim_send_sync(b3, 1, 1);

  int result = spim_send_sync(fb, half_length * FB_BPP, 0);
  if (result) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
    return false;
  }
  result = spim_send_sync(fb + half_length, half_length * FB_BPP, 0);
  if (result) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
    return false;
  }

  return true;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "render",
  "generate" : "jswrap_fb_render",
  "return" : ["int","nothing"]
}
Update a rect resource
*/
int jswrap_fb_render() {
  if (!fb_was_modified) {
    return 0;
  }

  // clear framebuffer
  fb_clear(0);

  // render all rects
  fb_rect* rect = root;
  while (rect) {
    if (rect->buf) {
      JSV_GET_AS_CHAR_ARRAY(buf_data, buf_size, rect->index);
      int x = rect->x;
      for(uint i = 0; i<buf_size; i++) {
        x += fb_blit(x, rect->y, rect->w, rect->h, rect->buf, buf_data[i], rect->c);
        x += 2; // kerning
      }
    } else {
      fb_fill(rect->x, rect->y, rect->w, rect->h, rect->c);
    }

    rect = rect->next;
  }

  // send data to display
  fb_flip();

  fb_was_modified = false;
  return 0;
}
