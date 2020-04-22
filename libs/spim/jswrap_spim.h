/*
SPIM interface for NRF devices
*/
JsVar *jswrap_spim_setup(JsVar *options);
JsVar *jswrap_spim_send(JsVar *buffer, int cmdBytes);
JsVar *jswrap_spim_send_sync(JsVar *buffer, int cmdBytes);

JsVar *jswrap_fb_init(int w, int h, int bpp);
JsVar *jswrap_fb_flip();
JsVar *jswrap_fb_clear();
JsVar *jswrap_fb_blit(JsVar* buffer, int w, int h, int x);
int jswrap_fb_get(int x, int y);
JsVar* jswrap_fb_set(int x, int y, int c);
JsVar* jswrap_fb_fill(int x, int y, int w, int h);
