/*
SPIM interface for NRF devices
*/
JsVar *jswrap_spim_setup(JsVar *options);
JsVar *jswrap_spim_send(JsVar *buffer, int cmdBytes);
JsVar *jswrap_spim_send_sync(JsVar *buffer, int cmdBytes);

JsVar *jswrap_fb_init(int w, int h, int bpp);
JsVar *jswrap_fb_flip(int y, int len);
JsVar *jswrap_fb_clear();
int jswrap_fb_get(int x, int y);
JsVar* jswrap_fb_set(int x, int y, int c);
JsVar* jswrap_fb_fill(int x, int y, int w, int h);
JsVar* jswrap_fb_set_color(int r, int g, int b);
JsVar *jswrap_fb_prepare_blit(int x, int y);
JsVar *jswrap_fb_blit(JsVar* buffer, int w, int h);
