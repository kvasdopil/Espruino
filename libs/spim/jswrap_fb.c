
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

uint16_t fb[240*240];
int fb_width = 240;
int fb_height = 240;
int fb_bpp = sizeof(uint16_t);

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "flip",
  "generate" : "jswrap_fb_flip",
  "params" : [],
  "return" : ["JsVar","nothing"]
}
Send framebuffer to SPIM3 interface
*/
JsVar *jswrap_fb_flip() {
  int half_length = fb_width * 120;
  uint8_t b1[] = {0x2A, 0, 0, fb_height >> 8, fb_height && 0xff};
  uint8_t b2[] = {0x2B, 0, 0, fb_width >> 8, fb_width && 0xff};
  uint8_t b3[] = {0x2C};

  // FIXME: add error check here
  // spim_send_sync(b1, 5, 1);
  // spim_send_sync(b2, 5, 1);
  spim_send_sync(b3, 1, 1);

  int result = spim_send_sync(fb, half_length * 2, 0);
  if (result) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
    return 0;
  }
  result = spim_send_sync(fb + half_length, half_length * 2, 0);
  if (result) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
    return 0;
  }

  return 0;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "clear",
  "generate" : "jswrap_fb_clear",
  "params" : [
  ],
  "return" : ["JsVar","nothing"]
}
Fill framebuffer with zeroes
*/
JsVar *jswrap_fb_clear() {
  uint16_t* end = fb + fb_width * fb_height;
  uint16_t* ptr = fb;
  while(ptr != end) {
    *ptr = 0;
    ptr++;
  }

  return 0;
}

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

  if (buf_size < (w * h * 2)) {
    jsExceptionHere(JSET_ERROR, "Buffer is too small, got %d need %d", buf_size, w * h * 2);
    return 0;
  }

  // const ymin = max(0, blit_y);
  // const ymax = min(blit_y + h, fb_height);
  // const xmin = max(0, blit_x);
  // const xmax = min(blit_x + w, fb_width);

  // TODO: optimize
  // for(int sy = ymin; sy < ymax; sy++) {
  //   int offy = sy - blit_y;
  //   for(int sx = xmin; sy < xmax; sx++) {
  //     int offx = sx - blit_x;
  //     uint16_t val = buf16[offx + offy * w];
  //     fb[sx + sy * fb_width] = val;
  //   }
  // }
  // for(int yy = 0; yy<h; yy++) {
  //   for(int xx = 0; xx<w; xx++) {
  //     fb[blit_x + xx + (blit_y + yy) * fb_width] = buf16[xx + yy * w];
  //   }
  // }

  for(int yy = 0;yy<h; yy++) {
    for(int xx=0; xx<w; xx++) {
      jswrap_fb_set(blit_x + xx, blit_y + yy, buf16[xx + yy * w]);
    }
  }

  return 0;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "setColor",
  "generate" : "jswrap_fb_set_color",
  "params" : [
    ["r","int","Red"],
    ["g","int","Green"],
    ["b","int","Blue"]
  ],
  "return" : ["JsVar","nothing"]
}
Draw a filled rect on a buffer
*/
static uint16_t current_color = 0;
JsVar* jswrap_fb_set_color(int r, int g, int b) {
  current_color = (r >> 3 << 11) + (g >> 2 << 5) + (b >> 3);
  uint8_t* bytes = &current_color;
  uint8_t bb = bytes[0];
  bytes[0] = bytes[1];
  bytes[1] = bb;
  return 0;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "fill",
  "generate" : "jswrap_fb_fill",
  "params" : [
    ["x","int","Start of fill"],
    ["y","int","Start of fill"],
    ["w","int","Width of fill"],
    ["h","int","Height of fill"]
  ],
  "return" : ["JsVar","nothing"]
}
Draw a filled rect on a buffer
*/
JsVar* jswrap_fb_fill(int x, int y, int w, int h) {
  // TODO: optimize
  const ymin = max(0, y);
  const ymax = min(y + h, fb_height);
  const xmin = max(0, x);
  const xmax = min(x + w, fb_width);

  for(int yy = ymin; yy<ymax; yy++) {
    for(int xx = xmin; xx<xmax; xx++) {
      fb[xx + yy * fb_width] = current_color;
    }
  }
  return 0;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "get",
  "generate" : "jswrap_fb_get",
  "params" : [
    ["x","int","Width of image"],
    ["y","int","Height of image"]
  ],
  "return" : ["int","pixel value"]
}
Get a pixel value of fb
*/
int jswrap_fb_get(int x, int y) {
  if (y < 0 || x < 0 || x >= fb_width || y >= fb_height) {
    return 0;
  }

  return fb[x + y * fb_width];
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "set",
  "generate" : "jswrap_fb_set",
  "params" : [
    ["x","int","Width of image"],
    ["y","int","Height of image"],
    ["c","int","Value of the pixel"]
  ],
  "return" : ["JsVar","nothing"]
}
Get a pixel value of fb
*/
JsVar* jswrap_fb_set(int x, int y, int c) {
  if (y < 0 || x < 0 || x >= fb_width || y >= fb_height) {
    return 0;
  }

  fb[x + y*fb_width] = c;

  return 0;
}