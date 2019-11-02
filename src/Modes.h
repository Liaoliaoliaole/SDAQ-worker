

//Function for Discovery mode
void Discovery(int socket_num);

//Function for Discovery mode
void Autoconf(int socket_num);

//Function for Discovery mode
void Change_address(int socket_num, unsigned int serial_number, unsigned char new_address);

//Function for Measuring mode
void Measure(int socket_num,unsigned char dev_addr);

//Function for Log mode
void Logging(int socket_num,unsigned char dev_addr);

//Function for Info mode
void Dev_info(int socket_num,unsigned char dev_addr);
