
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

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "prepareBlit",
  "generate" : "jswrap_fb_prepare_blit",
  "params" : [
    ["x","int","X coord to blit to"],
    ["y","int","Y coord to blit to"]
  ],
  "return" : ["JsVar","nothing"]
}
Blit an image to a framebuffer 
*/
static int blit_x = 0;
static int blit_y = 0;

JsVar *jswrap_fb_prepare_blit(int x, int y) {
  blit_x = x;
  blit_y = y;
  return 0;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "blit",
  "generate" : "jswrap_fb_blit",
  "params" : [
    ["buffer","JsVar","Bitmap to blit into framebuffer"],
    ["w","int","Width of image"],
    ["h","int","Height of image"]
  ],
  "return" : ["JsVar","nothing"]
}
Blit an image to a framebuffer 
*/
// TODO: use JsVarArray
JsVar *jswrap_fb_blit(JsVar* buffer, int w, int h) {
  JSV_GET_AS_CHAR_ARRAY(buf_data, buf_size, buffer);
  uint16_t* buf16 = buf_data;

  if (buf_size < (w * h * FB_BPP)) {
    jsExceptionHere(JSET_ERROR, "Buffer is too small, got %d need %d", buf_size, w * h * FB_BPP);
    return 0;
  }

  for(int y = 0; y < h; y++) {
    for(int x = 0; x < w; x++) {
      int tx = blit_x + x;
      int ty = blit_y + y;
      if (tx < 0 || ty < 0 || tx >= FB_WIDTH || ty >= FB_HEIGHT) {
        continue;
      }

      fb[tx + ty*FB_WIDTH] = buf16[x + y * w];
    }
  }

  return 0;
}

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
static uint16_t current_color = 0;
JsVar* jswrap_fb_color(int r, int g, int b) {
  uint16_t color = (r >> 3 << 11) + (g >> 2 << 5) + (b >> 3);
  uint8_t* bytes = &color;
  uint8_t bb = bytes[0];
  bytes[0] = bytes[1];
  bytes[1] = bb;
  return color;
}

typedef struct {
  int x;
  int y;
  int w;
  int h;
  int c;
  void* next;
  int id;
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
  "name" : "createRect",
  "generate" : "jswrap_fb_create_rect",
  "params" : [
    ["opt","JsVar","rect params"]
  ],
  "return" : ["int","rect id"]
}
Create a rect resource
*/
int jswrap_fb_create_rect(JsVar* opt) {
  fb_rect* rect = calloc(sizeof(fb_rect), 1);

  rect->id = next_id++;
  rect->x = 0;
  rect->y = 0;
  rect->w = 0;
  rect->h = 0;
  rect->c = 0;
  rect->next = root;
  root = rect;

  jsvConfigObject configs[] = {
      {"x", JSV_INTEGER, &(rect->x)},
      {"y", JSV_INTEGER, &(rect->y)},
      {"w", JSV_INTEGER, &(rect->w)},
      {"h", JSV_INTEGER, &(rect->h)},
      {"c", JSV_INTEGER, &(rect->c)},
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
  "name" : "updateRect",
  "generate" : "jswrap_fb_update_rect",
  "params" : [
    ["id","int","rect id"],
    ["opt","JsVar","rect params"]
  ],
  "return" : ["int","nothing"]
}
Update a rect resource
*/
int jswrap_fb_update_rect(int id, JsVar* opt) {
  fb_rect* rect = root;

  // find rect
  while(rect && rect->id != id) {
    rect = rect->next;
  }

  if (!rect) {
    jsExceptionHere(JSET_ERROR, "Resource not found");
    return 0;
  }

  jsvConfigObject configs[] = {
      {"x", JSV_INTEGER, &(rect->x)},
      {"y", JSV_INTEGER, &(rect->y)},
      {"w", JSV_INTEGER, &(rect->w)},
      {"h", JSV_INTEGER, &(rect->h)},
      {"c", JSV_INTEGER, &(rect->c)},
  };
  
  if (!jsvReadConfigObject(opt, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Invalid arguments for rect");
    return 0;
  }

  return 0;
}

void fb_clear(int color) {
  uint16_t* end = fb + FB_WIDTH * FB_HEIGHT;
  uint16_t* ptr = fb;
  while(ptr != end) {
    *ptr = color;
    ptr++;
  }
}

void fb_fill(int x, int y, int w, int h) {
  // TODO: optimize
  const ymin = max(0, y);
  const ymax = min(y + h, FB_HEIGHT);
  const xmin = max(0, x);
  const xmax = min(x + w, FB_WIDTH);

  for(int yy = ymin; yy<ymax; yy++) {
    for(int xx = xmin; xx<xmax; xx++) {
      fb[xx + yy * FB_WIDTH] = current_color;
    }
  }
}

void fb_flip() {
  int half_length = FB_WIDTH * 120;
  uint8_t b1[] = {0x2A, 0, 0, FB_HEIGHT >> 8, FB_HEIGHT && 0xff};
  uint8_t b2[] = {0x2B, 0, 0, FB_WIDTH >> 8, FB_WIDTH && 0xff};
  uint8_t b3[] = {0x2C};

  // FIXME: add error check here
  // spim_send_sync(b1, 5, 1);
  // spim_send_sync(b2, 5, 1);
  spim_send_sync(b3, 1, 1);

  int result = spim_send_sync(fb, half_length * FB_BPP, 0);
  if (result) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
    return;
  }
  result = spim_send_sync(fb + half_length, half_length * FB_BPP, 0);
  if (result) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
    return;
  }
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
int jswrap_fb_render(int id) {
  // clear framebuffer
  fb_clear(0);

  // render all rects
  fb_rect* rect = root;
  while(rect) {
    current_color = rect->c;
    fb_fill(rect->x, rect->y, rect->w, rect->h);

    rect = rect->next;
  }

  // send data to display
  fb_flip();
  return 0;
}

