
//Function for Discovery mode
int Discover(int socket_num);

//Function for Autoconf mode
int Autoconf(int socket_num);

//Function for Address mode
int Change_address(int socket_num, unsigned int serial_number, unsigned char new_address);

//Function for Measuring mode
int Measure(int socket_num,unsigned char dev_addr);

//Function for Log mode
int Logging(int socket_num,unsigned char dev_addr);

//Function for Info mode
int Dev_info(int socket_num,unsigned char dev_addr);
