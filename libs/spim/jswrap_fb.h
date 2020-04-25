JsVar *jswrap_fb_prepare_blit(int x, int y);
JsVar *jswrap_fb_blit(JsVar* buffer, int w, int h);

int jswrap_fb_init();
int jswrap_fb_create_rect(JsVar* opt);
int jswrap_fb_update_rect(int id, JsVar* opt);
int jswrap_fb_render();

JsVar* jswrap_fb_color(int r, int g, int b);