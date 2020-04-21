#include "jsutils.h"
#include "jsvar.h"
#include "jspin.h"
#include "jshardware.h"
#include "jswrap_spim.h"
#include "jsvariterator.h"
#include "jswrap_promise.h"

#include "stdio.h"

#include <nrfx_spim.h>

/*JSON{
  "type" : "library",
  "class" : "spim"
}
This library provides SPIM capability 
*/

#define SPI_INSTANCE 3                                           
static const nrfx_spim_t spi = NRFX_SPIM_INSTANCE(SPI_INSTANCE);  

static volatile bool spi_xfer_done;  /**< Flag used to indicate that SPI instance completed the transfer. */

bool useMiso = false;
JsVar* xfer_promise = 0; 
JsVar* last_xfer_output = 0;

void spim_event_handler(nrfx_spim_evt_t const * event, void * context)
{
  spi_xfer_done = true;

  if (xfer_promise) {
    jspromise_resolve(xfer_promise, last_xfer_output);
    jsvUnLock(xfer_promise);
    xfer_promise = 0;
  }
}

/*JSON{
  "type" : "staticmethod",
  "class" : "spim",
  "name" : "setup",
  "generate" : "jswrap_spim_setup",
  "params" : [
    ["options","JsVar","Options for SPIM interface"]
  ],
  "return" : ["JsVar","nothing"]
}
This initialises SPIM
*/
JsVar *jswrap_spim_setup(JsVar *options) {
  if (!jsvIsObject(options)) {
    jsExceptionHere(JSET_ERROR, "Expecting an options object, got %t", options);
    return 0;
  }

  nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
  int miso            = NRFX_SPIM_PIN_NOT_USED;
  int mosi            = NRFX_SPIM_PIN_NOT_USED;
  int sck             = NRFX_SPIM_PIN_NOT_USED;
  int dcx             = NRFX_SPIM_PIN_NOT_USED;
  int ss              = NRFX_SPIM_PIN_NOT_USED;
  bool use_hw_ss      = true;
  bool ss_active_high = false;
  JsVar *speed = 0;

  jsvConfigObject configs[] = {
    {"miso", JSV_INTEGER, &miso},
    {"mosi", JSV_INTEGER, &mosi},
    {"sck", JSV_INTEGER, &sck},
    {"dcx", JSV_INTEGER, &dcx},
    {"ss", JSV_INTEGER, &ss},
    {"use_hw_ss", JSV_BOOLEAN, &use_hw_ss},
    {"ss_active_high", JSV_BOOLEAN, &ss_active_high},
    {"speed", JSV_OBJECT, &speed},
    // TODO: ss_duration
    // TODO: irq_priority
    // TODO: orc
  };

  if (!jsvReadConfigObject(options, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Something wrong with params");
  }

  spi_config.ss_pin    = ss;
  spi_config.miso_pin  = miso;
  spi_config.mosi_pin  = mosi;
  spi_config.sck_pin   = sck;
  spi_config.dcx_pin   = dcx;
  spi_config.use_hw_ss = use_hw_ss;
  spi_config.ss_active_high = ss_active_high;
  if(jsvIsString(speed)) {
    if (jsvIsStringEqual(speed, "1m")) {
      spi_config.frequency = NRF_SPIM_FREQ_1M;
    } else if (jsvIsStringEqual(speed, "8m")) {
      spi_config.frequency = NRF_SPIM_FREQ_8M;
    } else if (jsvIsStringEqual(speed, "16m")) {
      spi_config.frequency = NRF_SPIM_FREQ_16M;
    } else if (jsvIsStringEqual(speed, "32m")) {
      spi_config.frequency = NRF_SPIM_FREQ_32M;
    } else {
      spi_config.frequency = NRF_SPIM_FREQ_1M;
    }
  }
  useMiso = miso != 0xff;

  int result = nrfx_spim_init(&spi, &spi_config, spim_event_handler, NULL);

  // reinitialize SPIM if already initialized
  if (result == NRFX_ERROR_INVALID_STATE) {
    nrfx_spim_uninit(&spi);
    result = nrfx_spim_init(&spi, &spi_config, spim_event_handler, NULL);
  }

  if (result == NRFX_SUCCESS) {
    return 0;
  }

  if (result == NRFX_ERROR_INVALID_STATE) {
    jsExceptionHere(JSET_ERROR, "Cannot initialize SPIM: driver was already initialized");
  }
  if (result == NRFX_ERROR_BUSY) {
    jsExceptionHere(JSET_ERROR, "Cannot initialize SPIM: some other peripheral with the same instance ID is already in use");
  }
  if (result == NRFX_ERROR_NOT_SUPPORTED) {
    jsExceptionHere(JSET_ERROR, "Cannot initialize SPIM: requested configuration is not supported by the SPIM instance.");
  }
  
  jsExceptionHere(JSET_ERROR, "Cannot initialize SPIM: Error %d", result);
  return 0;
}

// If buffer is Uint8Array or a string then we can just reuse it to store the response
// (rx_data will be equal to tx_data in this case)
// otherwise we create an array ourselves and use it as a response buffer
#define REUSE_BUFFER(IN_ARRAY, IN_LENGTH, OUT_ARRAY, OUT_LENGTH, OUT_PTR) \
  OUT_PTR = jsvGetDataPointer(IN_ARRAY, &OUT_LENGTH); \
  if (OUT_PTR) { \
    OUT_ARRAY = IN_ARRAY; \
  } else { \
    OUT_ARRAY = jsvNewTypedArray(ARRAYBUFFERVIEW_UINT8, IN_LENGTH); \
    if (OUT_ARRAY) { \
      OUT_PTR = jsvGetDataPointer(OUT_ARRAY, &OUT_LENGTH); \
      assert(OUT_PTR); \
    } \
  } 

/*JSON{
  "type" : "staticmethod",
  "class" : "spim",
  "name" : "sendSync",
  "generate" : "jswrap_spim_send_sync",
  "params" : [
    ["data","JsVar","Data to send to SPIM interface"],
    ["cmdBytes","int32","Number of command bytes in the buffer"]
  ],
  "return" : ["JsVar","nothing"]
}
Send data via SPIM interface
*/
JsVar *jswrap_spim_send_sync(JsVar *buffer, int cmdBytes) {
  JsVar* result = 0;
  size_t rx_size = 0;
  unsigned char *rx_data = NULL;

  JSV_GET_AS_CHAR_ARRAY(tx_data, tx_size, buffer);

  // If MISO pin is used, then we want to receive data from the device 
  if (useMiso) {
    REUSE_BUFFER(buffer, tx_size, result, rx_size, rx_data);
    if (!result) {
      jsExceptionHere(JSET_ERROR, "Cannot allocate array to store response"); 
      return 0;
    }
  }

  spi_xfer_done = false;

  nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_SINGLE_XFER(tx_data, tx_size, rx_data, rx_size);

  int xfer_result = nrfx_spim_xfer_dcx(&spi, &xfer_desc, 0, cmdBytes);
  if (xfer_result != NRFX_SUCCESS) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", xfer_result);
    return 0;
  }

  while (!spi_xfer_done)
  {
    __WFE();
  }

  return result;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "spim",
  "name" : "send",
  "generate" : "jswrap_spim_send",
  "params" : [
    ["data","JsVar","Data to send to SPIM interface"],
    ["cmdBytes","int32","Number of command bytes in the buffer"]
  ],
  "return" : ["JsVar","A promise, completed when transfer is finished"],
  "return_object":"Promise"
}
Asyncronously send data via SPIM interface
*/
JsVar *jswrap_spim_send(JsVar *buffer, int cmdBytes) {
  JsVar* result = 0;
  size_t rx_size = 0;
  unsigned char *rx_data = NULL;

  if (xfer_promise) {
    jsExceptionHere(JSET_ERROR, "Cannot send data, another transfer is in progress");
    return 0;
  }

  JSV_GET_AS_CHAR_ARRAY(tx_data, tx_size, buffer);

  // If MISO pin is used, then we want to receive data from the device 
  if (useMiso) {
    REUSE_BUFFER(buffer, tx_size, result, rx_size, rx_data);
    if (!result) {
      jsExceptionHere(JSET_ERROR, "Cannot allocate array to store response"); 
      return 0;
    }
  }

  spi_xfer_done = false;
  
  last_xfer_output = result; // store result in global var so the callback can put it in promise
  xfer_promise = jspromise_create();
  if (!xfer_promise) {
    jsExceptionHere(JSET_ERROR, "Cannot create promise");
    return 0;
  }

  nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_SINGLE_XFER(tx_data, tx_size, rx_data, rx_size);

  int xfer_result = nrfx_spim_xfer_dcx(&spi, &xfer_desc, 0, cmdBytes);
  if (xfer_result != NRFX_SUCCESS) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", xfer_result);
    return 0;
  }

  return jsvLockAgain(xfer_promise);
}

// fb code

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
  "name" : "init",
  "generate" : "jswrap_fb_init",
  "params" : [
    ["w","int","Width of framebuffer"],
    ["h","int","Height of framebuffer"],
    ["bpp","int","Bpp of framebuffer"]
  ],
  "return" : ["JsVar","nothing"]
}
Initialize framebuffer object
*/
JsVar *jswrap_fb_init(int w, int h, int bpp) {
  if (fb && fb_width != w && fb_width != h && fb_bpp != bpp) {
    jsExceptionHere(JSET_ERROR, "Framebuffer already allocated, need to free()");
    return 0;
  }

  if (bpp != 2) {
    jsExceptionHere(JSET_ERROR, "Only 16 bpp 5-6-5 framebuffers are supported, got %d %d %d", w, h, bpp);
    return 0;
  }

  // fb = calloc(FB_16BPP, w * h);
  // fb_bpp = bpp;
  // fb_width = w;
  // fb_height = h;
  jsExceptionHere(JSET_ERROR, "Not implemented");
  return 0;
}

/*JSON{
  "type" : "staticmethod",
  "class" : "fb",
  "name" : "flip",
  "generate" : "jswrap_fb_flip",
  "params" : [
  ],
  "return" : ["JsVar","nothing"]
}
Send framebuffer to SPIM3 interface
*/
JsVar *jswrap_fb_flip() {
  if (!fb) {
    jsExceptionHere(JSET_ERROR, "Framebuffer is not initialized");
    return 0;
  }

  spi_xfer_done = false;

  last_xfer_output = 0; // store result in global var so the callback can put it in promise
  xfer_promise = jspromise_create();
  if (!xfer_promise) {
    jsExceptionHere(JSET_ERROR, "Cannot create promise");
    return 0;
  }

  nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_SINGLE_XFER(fb, fb_width * fb_height * sizeof(uint16_t), NULL, 0);

  int xfer_result = nrfx_spim_xfer_dcx(&spi, &xfer_desc, 0, 0);
  if (xfer_result != NRFX_SUCCESS) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", xfer_result);
    return 0;
  }

  while (!spi_xfer_done)
  {
    __WFE();
  }

  return jsvLockAgain(xfer_promise);
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
  if (!fb) {
    jsExceptionHere(JSET_ERROR, "Framebuffer is not initialized");
    return 0;
  }

  uint16_t* end = fb + fb_width * fb_height;
  uint16_t* ptr = fb;
  while(ptr != end) {
    *ptr = 0;
    ptr++;
  }

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
JsVar *jswrap_fb_blit(JsVar* buffer, int w, int h) {
  if (!fb) {
    jsExceptionHere(JSET_ERROR, "Framebuffer is not initialized");
    return 0;
  }

  int x = 0;
  int y = 0;

  JSV_GET_AS_CHAR_ARRAY(buf_data, buf_size, buffer);
  uint16_t* buf16 = buf_data;

  if (buf_size < (w * h * 2)) {
    jsExceptionHere(JSET_ERROR, "Buffer is too small, got %d need %d", buf_size, w * h * 2);
    return 0;
  }

  for(int yy = 0; yy<h; yy++) {
    for(int xx = 0; xx<w; xx++) {
      fb[xx + yy * fb_width] = buf16[xx + yy * w];
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
  if (!fb) {
    jsExceptionHere(JSET_ERROR, "Framebuffer is not initialized");
    return 0;
  }

  if (x>=fb_width || y>=fb_height) {
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
  if (!fb) {
    jsExceptionHere(JSET_ERROR, "Framebuffer is not initialized");
    return 0;
  }

  if (x>=fb_width || y>=fb_height) {
    return 0;
  }

  fb[x + y*fb_width] = c;

  return 0;
}