void esp8266_setup(char *ssid, char *password, void (*ready_callback)());
void open_socket(char * dest, char * port, unsigned int id, void (*data_received_callback)(unsigned int bytes, char * data));
