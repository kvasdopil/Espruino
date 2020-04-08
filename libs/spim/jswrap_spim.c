#include "jsutils.h"
#include "jsvar.h"
#include "jspin.h"
#include "jshardware.h"
#include "jswrap_spim.h"

#include <nrfx_spim.h>

/*JSON{
  "type" : "library",
  "class" : "spim"
}
This library provides SPIM capability 
*/

/*JSON{
  "type" : "staticmethod",
  "class" : "spim",
  "name" : "setup",
  "generate" : "jswrap_spim_setup",
  "params" : [
    ["options","JsVar","Various options for the TV output"],
    ["width","int",""]
  ],
  "return" : ["JsVar","A graphics object"]
}
This initialises SPIM
*/

typedef struct {
  int miso;
  int mosi;
  int sck;      
  int dc;    
  int cs;
} spim_config;

void spim_handler(nrfx_spim_evt_t const *p_event, void *context) {

}

JsVar *jswrap_spim_setup(JsVar *options) {
  if (!jsvIsObject(options)) {
    jsExceptionHere(JSET_ERROR, "Expecting an options object, got %t", options);
    return 0;
  }

  /*spim_config config;
  jsvConfigObject configs[] = {
    {"miso", JSV_INTEGER, &inf.pinVideo},
    {"mosi", JSV_INTEGER, &inf.pinSync},
    {"sck", JSV_INTEGER, &inf.pinSyncV},
    {"dc", JSV_INTEGER, &inf.width},
    {"cs", JSV_INTEGER, &inf.height},
  };

  if (!jsvReadConfigObject(options, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
    jsExceptionHere(JSET_ERROR, "Something wrong with params");
  }*/

  nrfx_spim_t const instance;
  nrfx_spim_config_t const config = {
    .sck_pin = 2,
    .mosi_pin = 29,
    .miso_pin = NRFX_SPIM_PIN_NOT_USED ,
    .ss_pin = 47,
    .ss_active_high = false,
    .irq_priority = APP_IRQ_PRIORITY_LOWEST,
    .orc = 0xFF,
    .frequency = NRF_SPIM_FREQ_8M,
    .mode = NRF_SPIM_MODE_0,
    .bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST,
    .dcx_pin = 31,
    .rx_delay = 100,
    .use_hw_ss = true,
    .ss_duration = 100,
  };
  return nrfx_spim_init(&instance, &config, spim_handler, 0);
}
