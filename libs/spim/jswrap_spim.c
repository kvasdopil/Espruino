#include "jsutils.h"
#include "jsvar.h"
#include "jspin.h"
#include "jswrap_spim.h"

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

JsVar *jswrap_spim_setup(JsVar *options) {
  //if (!jsvIsObject(options)) {
    jsExceptionHere(JSET_ERROR, "Expecting an options object, got %t", options);
    return 0;
  //}
  // JsVar *tvType = jsvObjectGetChild(options, "type",0);
  // if (jsvIsStringEqual(tvType, "pal")) {
  //   jsvUnLock(tvType);
  //   tv_info_pal inf;
  //   tv_info_pal_init(&inf);
  //   jsvConfigObject configs[] = {
  //       {"type", 0, 0},
  //       {"video", JSV_PIN, &inf.pinVideo},
  //       {"sync", JSV_PIN, &inf.pinSync},
  //       {"width", JSV_INTEGER, &inf.width},
  //       {"height", JSV_INTEGER, &inf.height},
  //   };
  //   if (jsvReadConfigObject(options, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
  //     return tv_setup_pal(&inf);
  //   }
  //   return 0;
  // } else if (jsvIsStringEqual(tvType, "vga")) {
  //   jsvUnLock(tvType);
  //   tv_info_vga inf;
  //   tv_info_vga_init(&inf);
  //   jsvConfigObject configs[] = {
  //       {"type", 0, 0},
  //       {"video", JSV_PIN, &inf.pinVideo},
  //       {"hsync", JSV_PIN, &inf.pinSync},
  //       {"vsync", JSV_PIN, &inf.pinSyncV},
  //       {"width", JSV_INTEGER, &inf.width},
  //       {"height", JSV_INTEGER, &inf.height},
  //       {"repeat", JSV_INTEGER, &inf.lineRepeat},
  //   };
  //   if (jsvReadConfigObject(options, configs, sizeof(configs) / sizeof(jsvConfigObject))) {
  //     return tv_setup_vga(&inf);
  //   }
  //   return 0;
  //}

  //jsExceptionHere(JSET_ERROR, "Unknown TV output type %q", tvType);
  //jsvUnLock(tvType);
  //return 0;
}
