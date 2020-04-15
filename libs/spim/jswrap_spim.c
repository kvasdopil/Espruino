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

#define TEST_STRING { 0x9f, 0, 0, 0 }
static uint8_t       m_tx_buf[] = TEST_STRING;           /**< TX buffer. */
static uint8_t       m_rx_buf[sizeof(m_tx_buf) + 1];  /**< RX buffer. */
static const uint8_t m_length = sizeof(m_tx_buf);        /**< Transfer length. */

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

  spi_config.frequency      = NRF_SPIM_FREQ_1M;
  spi_config.use_hw_ss      = true;
  spi_config.ss_active_high = false;

  // uint miso = 21;
  // uint mosi = 20;
  // uint sck = 19;
  // uint dcx = 0xff;
  // uint ss = 17; 

  // jsvConfigObject configs[] = {
  //   {"miso", JSV_INTEGER, &miso},
  //   {"mosi", JSV_INTEGER, &mosi},
  //   {"sck", JSV_INTEGER, &sck},
  //   {"dcx", JSV_INTEGER, &dcx},
  //   {"ss", JSV_INTEGER, &ss},
  // };

  spi_config.ss_pin         = 17;
  spi_config.miso_pin       = 21;
  spi_config.mosi_pin       = 20;
  spi_config.sck_pin        = 19;

  // if (!jsvReadConfigObject(options, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
  //   jsExceptionHere(JSET_ERROR, "Something wrong with params");
  // }

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
    ["options","JsVar","Send data to SPIM interface"],
    ["cmdBytes","JsVar","Number of command bytes in the buffer"]
  ],
  "return" : ["JsVar","nothing"]
}
Send bytes via SPIM interface
*/
JsVar *jswrap_spim_send(JsVar *buffer, JsVar *cmdBytes) {
  JSV_GET_AS_CHAR_ARRAY(data, dataLen, buffer);

  nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TRX(data, dataLen, m_rx_buf, m_length);

  memset(m_rx_buf, 0, m_length);
  spi_xfer_done = false;

  int result = nrfx_spim_xfer_dcx(&spi, &xfer_desc, 0, 0);
  if (result != NRFX_SUCCESS) {
    jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
    return 0;
  }

  while (!spi_xfer_done)
  {
    __WFE();
  }

  if (m_rx_buf[0] == 0 && m_rx_buf[1] == 194 && m_rx_buf[2] == 32 && m_rx_buf[3] == 22) {
    jsExceptionHere(JSET_ERROR, "Correct data: %d %d %d %d", m_rx_buf[0], m_rx_buf[1], m_rx_buf[2], m_rx_buf[3]);
    return 0;
  }
      
  jsExceptionHere(JSET_ERROR, "Incorrect data: %d %d %d %d", m_rx_buf[0], m_rx_buf[1], m_rx_buf[2], m_rx_buf[3]);
  return 0;
  
  // spi_xfer_done = false;

  // nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TX(data, dataLen);
  // int result = nrfx_spim_xfer(&spi, &xfer_desc, 0);

  // if (result != NRFX_SUCCESS) {
  //   jsExceptionHere(JSET_ERROR, "Cannot send data: error %d", result);
  //   return 0;
  // }

  // int i = 0;
  // while (!spi_xfer_done)
  // {
  //   i++;
  //   __WFE();
  // }

  // jsExceptionHere(JSET_ERROR, "I waited for %d events", i);

  // return 0;
}