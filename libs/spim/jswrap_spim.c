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
  unsigned char *rx_data = NULL;
  JsVar* result = 0;

  JSV_GET_AS_CHAR_ARRAY(tx_data, tx_size, buffer);

  // If MISO pin is used, then we want to receive data from the device 
  size_t rx_size = useMiso ? tx_size : 0;
  if (rx_size) {
    // If buffer is Uint8Array or a string, then we can just reuse it to store the response
    int length;
    rx_data = jsvGetDataPointer(buffer, &length); // NOTE: rx_data will be equal to tx_data
    if (rx_data) {
      result = buffer; // function will return the same buffer
    } else {
      // otherwise we create an array ourselves and use it as a response buffer
      result = jsvNewTypedArray(ARRAYBUFFERVIEW_UINT8, rx_size);
      if (!result) {
        jsExceptionHere(JSET_ERROR, "Cannot allocate array to store result");
        return 0;
      }
      rx_data = jsvGetDataPointer(result, &length);
      assert(rx_data);
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
  unsigned char *rx_data = NULL;
  JsVar* result = 0;

  if (xfer_promise) {
    jsExceptionHere(JSET_ERROR, "Cannot send data, another transfer is in progress");
    return 0;
  }

  JSV_GET_AS_CHAR_ARRAY(tx_data, tx_size, buffer);

  // If MISO pin is used, then we want to receive data from the device 
  size_t rx_size = useMiso ? tx_size : 0;
  if (rx_size) {
    // If buffer is Uint8Array or a string, then we can just reuse it to store the response
    int length;
    rx_data = jsvGetDataPointer(buffer, &length); // NOTE: rx_data will be equal to tx_data
    if (rx_data) {
      result = buffer; // function will return the same buffer
    } else {
      // otherwise we create an array ourselves and use it as a response buffer
      result = jsvNewTypedArray(ARRAYBUFFERVIEW_UINT8, rx_size);
      if (!result) {
        jsExceptionHere(JSET_ERROR, "Cannot allocate array to store result");
        return 0;
      }
      rx_data = jsvGetDataPointer(result, &length);
      assert(rx_data);
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