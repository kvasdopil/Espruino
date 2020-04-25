JsVar *jswrap_fb_flip();
JsVar *jswrap_fb_clear();
int jswrap_fb_get(int x, int y);
JsVar* jswrap_fb_set(int x, int y, int c);
JsVar* jswrap_fb_fill(int x, int y, int w, int h);
JsVar* jswrap_fb_set_color(int r, int g, int b);
JsVar *jswrap_fb_prepare_blit(int x, int y);
JsVar *jswrap_fb_blit(JsVar* buffer, int w, int h);

int jswrap_fb_create_rect(JsVar* opt);
int jswrap_fb_update_rect(JsVar* opt);
int jswrap_fb_render_rect();