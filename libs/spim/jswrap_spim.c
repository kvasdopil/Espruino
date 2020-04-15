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

bool useMiso = false;

void spim_event_handler(nrfx_spim_evt_t const * event, void * context)
{
  spi_xfer_done = true;
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
  "name" : "send",
  "generate" : "jswrap_spim_send",
  "params" : [
    ["data","JsVar","Data to send to SPIM interface"],
    ["cmdBytes","int32","Number of command bytes in the buffer"]
  ],
  "return" : ["JsVar","nothing"]
}
Send data via SPIM interface
*/
JsVar *jswrap_spim_send(JsVar *buffer, int cmdBytes) {
  unsigned char *rx_data = NULL;

  JSV_GET_AS_CHAR_ARRAY(tx_data, tx_size, buffer);

  size_t rx_size = useMiso ? tx_size : 0;
  if (rx_size) {
    rx_data = (unsigned char *)alloca(rx_size);
    memset(rx_data, 0, tx_size);
  } 

  spi_xfer_done = false;

  nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_SINGLE_XFER(tx_data, tx_size, rx_data, rx_size);

  int result = nrfx_spim_xfer_dcx(&spi, &xfer_desc, 0, cmdBytes);
  if (result != NRFX_SUCCESS) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
    return 0;
  }

  while (!spi_xfer_done)
  {
    __WFE();
  }

  if (rx_size) {
    JsVar *array = jsvNewTypedArray(ARRAYBUFFERVIEW_UINT8, rx_size);
    if (!array) {
      jsExceptionHere(JSET_ERROR, "Cannot allocate array");
    }
    JsvArrayBufferIterator it;
    jsvArrayBufferIteratorNew(&it, array, 0);
    unsigned int i;
    for (i=0; i<(unsigned)rx_size; i++) {
      jsvArrayBufferIteratorSetByteValue(&it, (char)rx_data[i]);
      jsvArrayBufferIteratorNext(&it);
    }
    jsvArrayBufferIteratorFree(&it);
    return array;
  }
  
  return 0;
}