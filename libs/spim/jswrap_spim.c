#include "jsutils.h"
#include "jsvar.h"
#include "jspin.h"
#include "jshardware.h"
#include "jswrap_spim.h"
#include "jsvariterator.h"

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

void spim_event_handler(nrfx_spim_evt_t const * event, void * context)
{
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
  spi_config.frequency      = NRF_SPIM_FREQ_1M;
  spi_config.use_hw_ss      = true;
  spi_config.ss_active_high = false;

  jsvConfigObject configs[] = {
    {"miso", JSV_INTEGER, &spi_config.miso_pin},
    {"mosi", JSV_INTEGER, &spi_config.mosi_pin},
    {"sck", JSV_INTEGER, &spi_config.sck_pin},
    {"dc", JSV_INTEGER, &spi_config.dcx_pin},
    {"cs", JSV_INTEGER, &spi_config.ss_pin},
  };
  if (!jsvReadConfigObject(options, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Something wrong with params");
  }

  int result = nrfx_spim_init(&spi, &spi_config, spim_event_handler, 0);

  // reinitialize SPIM if already initialized
  if (result == NRFX_ERROR_INVALID_STATE) {
    nrfx_spim_uninit(&spi);
    result = nrfx_spim_init(&spi, &spi_config, spim_event_handler, 0);
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
  "name" : "send",
  "generate" : "jswrap_spim_send",
  "params" : [
    ["options","JsVar","Send data to SPIM interface"],
    ["cmdBytes","JsVar","Number of command bytes in the buffer"]
  ],
  "return" : ["JsVar","nothing"]
}
Send bytes via SPIM interface
*/
JsVar *jswrap_spim_send(JsVar *buffer, JsVar *cmdBytes) {
  JSV_GET_AS_CHAR_ARRAY(data, dataLen, buffer);
  
  nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(data, dataLen);
  int result = nrfx_spim_xfer_dcx(&spi, &xfer_desc, 0, 1);

  if (result == NRFX_SUCCESS) {
    return 0;
  }

  jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
  return 0;
}