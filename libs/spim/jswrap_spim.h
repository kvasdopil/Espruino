/*
SPIM interface for NRF devices
*/
JsVar *jswrap_spim_setup(JsVar *options);
JsVar *jswrap_spim_send(JsVar *buffer, int cmdBytes);
