/*
SPIM interface for NRF devices
*/
JsVar *jswrap_spim_setup(JsVar *options);
JsVar *jswrap_spim_send(JsVar *buffer, int cmdBytes);
JsVar *jswrap_spim_send_sync(JsVar *buffer, int cmdBytes);

int spim_send_sync(uint8_t *buf, int len, int cmdBytes);