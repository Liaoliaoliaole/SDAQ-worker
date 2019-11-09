// enumerator for payload_type
enum time_stamp_mode
{
	absolute,
	relative,
	absolute_with_date
};

typedef struct option_flags{
	
	unsigned char timestamp_mode;
	char *timestamp_format;
	unsigned silent :1;
	unsigned verify :1;
	unsigned int timeout;
	
}opt_flags;


//Function for Discovery mode
int Discover(int socket_num, opt_flags usr_flag);

//Function for Autoconf mode
int Autoconfig(int socket_num, opt_flags usr_flag);

//Function for Address mode
int Change_address(int socket_num, unsigned int serial_number, unsigned char new_address, opt_flags usr_flag);

//Function for Measuring mode
int Measure(int socket_num,unsigned char dev_addr, opt_flags usr_flag);

//Function for Log mode
int Logging(int socket_num,unsigned char dev_addr, opt_flags usr_flag);

//Function for Info mode
int Dev_info(int socket_num,unsigned char dev_addr, opt_flags usr_flag);
