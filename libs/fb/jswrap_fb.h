
int jswrap_fb_init();
int jswrap_fb_add(JsVar* opts);
int jswrap_fb_set(int id, JsVar* opts);
int jswrap_fb_cmd(JsVar* interface, JsVar *cmd, int dclen, Pin dc);
int jswrap_fb_send(JsVar* interface, Pin dc);
int jswrap_fb_color(int r, int g, int b);