#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <glib.h>
#include <linux/can.h>

#include "../src/info.h"
#include "../src/SDAQ_xml.h"

extern volatile unsigned char TMR_exp;

static sdaq_calibration_points_data *append_point(SDAQ_info_cal_data *conf,
		unsigned int channel, float value, unsigned char point, unsigned char type)
{
	sdaq_calibration_points_data *data = new_SDAQ_cal_point_node();

	data->data_of_point = value;
	data->points_num = point;
	data->type = type;
	conf->Cal_points_data_lists[channel] = (struct GSList *)g_slist_append(
			(GSList *)conf->Cal_points_data_lists[channel], data);
	return data;
}

static void init_config(SDAQ_info_cal_data *conf, unsigned char max_points)
{
	date_list_data_of_node *date;

	memset(conf, 0, sizeof(*conf));
	conf->SDAQ_info.serial_number = 123456;
	conf->SDAQ_info.dev_type = dev_type_str[1];
	conf->SDAQ_info.firm_rev = 9;
	conf->SDAQ_info.hw_rev = 4;
	conf->SDAQ_info.num_of_ch = 1;
	conf->SDAQ_info.sample_rate = 10;
	conf->SDAQ_info.max_cal_point = max_points;
	conf->Cal_points_data_lists = calloc(1, sizeof(*conf->Cal_points_data_lists));
	g_assert_nonnull(conf->Cal_points_data_lists);

	date = new_SDAQ_date_node();
	date->ch_num = 1;
	date->year = 26;
	date->month = 8;
	date->day = 25;
	date->period = 12;
	date->amount_of_points = max_points;
	date->cal_unit = 28;
	conf->Calibration_date_list = (struct GSList *)g_slist_append(NULL, date);
}

static gchar *make_xml(const char *period, const char *used, const char *max_points,
		const char *measure0, const char *measure1, const char *gain0)
{
	const char *point1 = "";
	gchar *owned_point1 = NULL;
	gchar *xml;

	if(measure1)
	{
		owned_point1 = g_strdup_printf(
			"<Point_1><Measure>%s</Measure><Reference>2</Reference>"
			"<Offset>0</Offset><Gain>1</Gain><C2>0</C2><C3>0</C3></Point_1>",
			measure1);
		point1 = owned_point1;
	}
	xml = g_strdup_printf(
		"<?xml version=\"1.0\"?><SDAQ><SDAQ_info>"
		"<SerialNumber>123456</SerialNumber><Type>SDAQ-TC1</Type>"
		"<Firmware_Rev>9</Firmware_Rev><Hardware_Rev>4</Hardware_Rev>"
		"<Available_Channels>1</Available_Channels><Samplerate>10</Samplerate>"
		"<Max_num_of_cal_points>%s</Max_num_of_cal_points></SDAQ_info>"
		"<Calibration_Data><CH1><Calibration_date>2026/08/25</Calibration_date>"
		"<Calibration_Period>%s</Calibration_Period><Used_Points>%s</Used_Points>"
		"<Unit>\xC2\xB0" "C</Unit><Points>"
		"<Point_0><Measure>%s</Measure><Reference>1</Reference>"
		"<Offset>0</Offset><Gain>%s</Gain><C2>0</C2><C3>0</C3></Point_0>%s"
		"</Points></CH1></Calibration_Data></SDAQ>",
		max_points, period, used, measure0, gain0, point1);
	g_free(owned_point1);
	return xml;
}

static gchar *replace_once(const gchar *text, const gchar *old_value, const gchar *new_value)
{
	const gchar *position = strstr(text, old_value);

	g_assert_nonnull(position);
	return g_strdup_printf("%.*s%s%s", (int)(position - text), text, new_value,
			position + strlen(old_value));
}

static int parse_xml_text(const gchar *xml, SDAQ_info_cal_data *conf)
{
	gchar *path = NULL;
	int fd = g_file_open_tmp("sdaq-cal-test-XXXXXX.xml", &path, NULL);
	int result;

	g_assert_cmpint(fd, >=, 0);
	close(fd);
	g_assert_true(g_file_set_contents(path, xml, -1, NULL));
	result = XML_info_file_read_and_validate(path, conf);
	g_assert_cmpint(unlink(path), ==, 0);
	g_free(path);
	return result;
}

static void assert_parse_result(const gchar *xml, int expected)
{
	SDAQ_info_cal_data conf = {0};

	g_assert_cmpint(parse_xml_text(xml, &conf), ==, expected);
	free_SDAQ_info_cal_data(&conf);
}

static uint32_t float_bits(float value)
{
	uint32_t bits;

	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

static void test_struct_sizes_and_free(void)
{
	SDAQ_info_cal_data conf = {0};

	g_assert_cmpuint(sizeof(sdaq_calibration_points_data), ==, 6);
	g_assert_cmpuint(sizeof(date_list_data_of_node), ==, 7);
	conf.SDAQ_info.num_of_ch = 1;
	free_SDAQ_info_cal_data(&conf);

	init_config(&conf, 1);
	append_point(&conf, 0, 1.0f, 0, meas);
	free_SDAQ_info_cal_data(&conf);
}

static void test_xml_period_and_point_bounds(void)
{
	gchar *xml, *modified;

	const char *valid_periods[] = {"0", "1", "255"};
	for(guint i=0; i<G_N_ELEMENTS(valid_periods); i++)
	{
		xml = make_xml(valid_periods[i], "1", "1", "1", NULL, "1");
		assert_parse_result(xml, EXIT_SUCCESS);
		g_free(xml);
	}

	const char *bad_periods[] = {"", "256", "300", "-1", "12x"};
	for(guint i=0; i<G_N_ELEMENTS(bad_periods); i++)
	{
		xml = make_xml(bad_periods[i], "1", "1", "1", NULL, "1");
		assert_parse_result(xml, EXIT_FAILURE);
		g_free(xml);
	}

	xml = make_xml("12", "0", "1", "1", NULL, "1");
	assert_parse_result(xml, EXIT_SUCCESS);
	g_free(xml);
	xml = make_xml("12", "2", "1", "1", "2", "1");
	assert_parse_result(xml, EXIT_FAILURE);
	g_free(xml);
	xml = make_xml("12", "-1", "1", "1", NULL, "1");
	assert_parse_result(xml, EXIT_FAILURE);
	g_free(xml);
	xml = make_xml("12", "256", "1", "1", NULL, "1");
	assert_parse_result(xml, EXIT_FAILURE);
	g_free(xml);
	xml = make_xml("12", "1", "17", "1", NULL, "1");
	assert_parse_result(xml, EXIT_FAILURE);
	g_free(xml);

	xml = make_xml("12", "1", "1", "1", NULL, "1");
	const char *bad_serials[] = {"", " 5", "+5", " -5", "5 ", "5x", "4294967296"};
	for(guint i=0; i<G_N_ELEMENTS(bad_serials); i++)
	{
		modified = g_strdup_printf("<SerialNumber>%s</SerialNumber>", bad_serials[i]);
		gchar *candidate = replace_once(xml, "<SerialNumber>123456</SerialNumber>", modified);
		assert_parse_result(candidate, EXIT_FAILURE);
		g_free(candidate);
		g_free(modified);
	}
	modified = replace_once(xml, "<SerialNumber>123456</SerialNumber>",
			"<SerialNumber>4294967295</SerialNumber>");
	assert_parse_result(modified, EXIT_SUCCESS);
	g_free(modified);
	g_free(xml);
}

static void test_xml_float_validation(void)
{
	gchar *xml, *modified;

	const char *bad_floats[] = {"nan", "inf", "-inf", "1e999", "1e-999", "1x"};
	for(guint i=0; i<G_N_ELEMENTS(bad_floats); i++)
	{
		xml = make_xml("12", "1", "1", "1", NULL, bad_floats[i]);
		assert_parse_result(xml, EXIT_FAILURE);
		g_free(xml);
	}

	xml = make_xml("12", "2", "2", "1", "1.00000001", "1");
	assert_parse_result(xml, EXIT_FAILURE);
	g_free(xml);
	xml = make_xml("12", "2", "2", "2", "1", "1");
	assert_parse_result(xml, EXIT_FAILURE);
	g_free(xml);
	xml = make_xml("12", "2", "2", "1", "1.0000002", "1.2345678e-9");
	assert_parse_result(xml, EXIT_SUCCESS);
	g_free(xml);

	xml = make_xml("12", "1", "1", "1", NULL, "1");
	modified = replace_once(xml, "<Calibration_date>2026/08/25</Calibration_date>",
			"<Calibration_date>bad</Calibration_date>");
	assert_parse_result(modified, EXIT_FAILURE);
	g_free(modified);
	modified = replace_once(xml, "<Unit>\xC2\xB0" "C</Unit>", "<Unit>unknown</Unit>");
	assert_parse_result(modified, EXIT_FAILURE);
	g_free(modified);
	g_free(xml);
}

static void test_xml_float32_round_trip(void)
{
	SDAQ_info_cal_data source, parsed = {0};
	gchar *path = NULL;
	int fd = g_file_open_tmp("sdaq-cal-roundtrip-XXXXXX.xml", &path, NULL);
	const float values[] = {
		1.2345678e-9f, 1.00000011920928955078125f, -5300.125f,
		200.000030517578125f, -3.8e25f, 3.14159274f,
		27.6599998f, 28.1234569f, -0.000000123456789f,
		0.99999994f, 1.40129846e-45f, 3.40282347e38f
	};

	g_assert_cmpint(fd, >=, 0);
	close(fd);
	init_config(&source, 2);
	for(unsigned char point=0; point<2; point++)
		for(unsigned char type=meas; type<=C3; type++)
			append_point(&source, 0, values[point * MAX_DATA_ON_POINT + type - meas], point, type);

	g_assert_cmpint(XML_info_file_write(path, &source, 1), ==, EXIT_SUCCESS);
	g_assert_cmpint(XML_info_file_read_and_validate(path, &parsed), ==, EXIT_SUCCESS);
	g_assert_cmpuint(g_slist_length((GSList *)parsed.Cal_points_data_lists[0]), ==, G_N_ELEMENTS(values));
	for(guint i=0; i<G_N_ELEMENTS(values); i++)
	{
		sdaq_calibration_points_data *actual = g_slist_nth_data((GSList *)parsed.Cal_points_data_lists[0], i);
		g_assert_cmphex(float_bits(actual->data_of_point), ==, float_bits(values[i]));
	}

	free_SDAQ_info_cal_data(&source);
	free_SDAQ_info_cal_data(&parsed);
	g_assert_cmpint(unlink(path), ==, 0);
	g_free(path);
}

static void test_point_correlation_by_key(void)
{
	SDAQ_info_cal_data current, wanted;
	sdaq_calibration_points_data *changed;

	init_config(&current, 1);
	init_config(&wanted, 1);
	for(unsigned char type=meas; type<=C3; type++)
		append_point(&wanted, 0, 100.0f + type, 0, type);
	for(unsigned char type=C3; type>=meas; type--)
		append_point(&current, 0, 100.0f + type, 0, type);

	g_assert_cmpint(corr_SDAQ_info_and_calibration_data(&current, &wanted, POINTS), ==, EXIT_SUCCESS);
	changed = g_slist_nth_data((GSList *)current.Cal_points_data_lists[0], 0);
	changed->data_of_point += 1.0f;
	g_assert_cmpint(corr_SDAQ_info_and_calibration_data(&current, &wanted, POINTS), ==, EXIT_FAILURE);
	current.Cal_points_data_lists[0] = (struct GSList *)g_slist_delete_link(
			(GSList *)current.Cal_points_data_lists[0], (GSList *)current.Cal_points_data_lists[0]);
	free_SDAQ_cal_point_node(changed);
	g_assert_cmpint(corr_SDAQ_info_and_calibration_data(&current, &wanted, POINTS), ==, EXIT_FAILURE);

	free_SDAQ_info_cal_data(&current);
	free_SDAQ_info_cal_data(&wanted);
}

static void append_active_point_pair(SDAQ_info_cal_data *current, SDAQ_info_cal_data *wanted)
{
	for(unsigned char type=meas; type<=C3; type++)
	{
		append_point(wanted, 0, 100.0f + type, 0, type);
		append_point(current, 0, 100.0f + type, 0, type);
	}
}

static void set_used_points(SDAQ_info_cal_data *conf, unsigned char used)
{
	date_list_data_of_node *date = g_slist_nth_data((GSList *)conf->Calibration_date_list, 0);

	g_assert_nonnull(date);
	date->amount_of_points = used;
}

static void test_point_correlation_ignores_inactive_fields(void)
{
	SDAQ_info_cal_data current, wanted;

	init_config(&current, 2);
	init_config(&wanted, 2);
	set_used_points(&current, 1);
	set_used_points(&wanted, 1);
	append_active_point_pair(&current, &wanted);

	append_point(&current, 0, 1.19455e-15f, 1, gain);
	append_point(&current, 0, INFINITY, 1, C2);
	append_point(&current, 0, -0.0f, 1, C3);
	g_assert_cmpint(corr_SDAQ_info_and_calibration_data(&current, &wanted, POINTS), ==, EXIT_SUCCESS);

	free_SDAQ_info_cal_data(&current);
	free_SDAQ_info_cal_data(&wanted);
}

static void test_empty_target_ignores_inactive_fields(void)
{
	SDAQ_info_cal_data current, wanted;

	init_config(&current, 1);
	init_config(&wanted, 1);
	set_used_points(&current, 0);
	set_used_points(&wanted, 0);
	for(unsigned char type=meas; type<=C3; type++)
		append_point(&current, 0, type == gain ? 1.19455e-15f : 0.0f, 0, type);
	g_assert_cmpint(corr_SDAQ_info_and_calibration_data(&current, &wanted, POINTS), ==, EXIT_SUCCESS);

	free_SDAQ_info_cal_data(&current);
	free_SDAQ_info_cal_data(&wanted);
}

static void test_write_sequence_is_bounded(void)
{
	SDAQ_info_cal_data conf;
	struct can_frame frame;
	int sockets[2];
	unsigned int frame_count = 0;

	init_config(&conf, 1);
	for(unsigned char type=meas; type<=C3; type++)
		append_point(&conf, 0, 20.0f + type, 0, type);
	g_assert_cmpint(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets), ==, 0);
	g_assert_cmpint(set_SDAQ_info_and_calibration_data(sockets[0], 15, &conf), ==, EXIT_SUCCESS);

	while(recv(sockets[1], &frame, sizeof(frame), MSG_DONTWAIT) == sizeof(frame))
	{
		sdaq_can_id *id = (sdaq_can_id *)&frame.can_id;
		g_assert_cmpuint(id->device_addr, ==, 15);
		g_assert_cmpuint(id->channel_num, ==, 1);
		if(frame_count == 0 || frame_count == 7)
			g_assert_cmpuint(id->payload_type, ==, Write_calibration_Date);
		else
			g_assert_cmpuint(id->payload_type, ==, Write_calibration_Point_Data);
		frame_count++;
	}
	g_assert_cmpint(errno, ==, EAGAIN);
	g_assert_cmpuint(frame_count, ==, 8);

	close(sockets[0]);
	close(sockets[1]);
	free_SDAQ_info_cal_data(&conf);
}

struct responder_args {
	int fd;
	unsigned char address;
};

static struct can_frame point_frame(unsigned char address, unsigned char channel,
		unsigned char point, unsigned char type, float value, unsigned char dlc)
{
	struct can_frame frame = {0};
	sdaq_can_id *id = (sdaq_can_id *)&frame.can_id;
	sdaq_calibration_points_data *data = (sdaq_calibration_points_data *)frame.data;

	id->flags = 4;
	id->protocol_id = PROTOCOL_ID;
	id->payload_type = Calibration_Point_Data;
	id->device_addr = address;
	id->channel_num = channel;
	frame.can_dlc = dlc;
	data->data_of_point = value;
	data->points_num = point;
	data->type = type;
	return frame;
}

static struct can_frame date_frame(unsigned char address, unsigned char channel,
		unsigned char used_points)
{
	struct can_frame frame = {0};
	sdaq_can_id *id = (sdaq_can_id *)&frame.can_id;
	sdaq_calibration_date *date = (sdaq_calibration_date *)frame.data;

	id->flags = 4;
	id->protocol_id = PROTOCOL_ID;
	id->payload_type = Calibration_Date;
	id->device_addr = address;
	id->channel_num = channel;
	frame.can_dlc = sizeof(*date);
	date->year = 26;
	date->month = 8;
	date->day = 25;
	date->period = 12;
	date->amount_of_points = used_points;
	date->cal_units = 28;
	return frame;
}

static struct can_frame status_frame(unsigned char address)
{
	struct can_frame frame = {0};
	sdaq_can_id *id = (sdaq_can_id *)&frame.can_id;
	sdaq_status *status = (sdaq_status *)frame.data;

	id->flags = 4;
	id->protocol_id = PROTOCOL_ID;
	id->payload_type = Device_status;
	id->device_addr = address;
	frame.can_dlc = sizeof(*status);
	status->dev_sn = 123456;
	status->dev_type = 1;
	return frame;
}

static struct can_frame info_frame(unsigned char address)
{
	struct can_frame frame = {0};
	sdaq_can_id *id = (sdaq_can_id *)&frame.can_id;
	sdaq_info *info = (sdaq_info *)frame.data;

	id->flags = 4;
	id->protocol_id = PROTOCOL_ID;
	id->payload_type = Device_info;
	id->device_addr = address;
	frame.can_dlc = sizeof(*info);
	info->dev_type = 1;
	info->firm_rev = 9;
	info->hw_rev = 4;
	info->num_of_ch = 1;
	info->sample_rate = 10;
	info->max_cal_point = 1;
	return frame;
}

static void send_frame(int fd, const struct can_frame *frame)
{
	g_assert_cmpint(write(fd, frame, sizeof(*frame)), ==, sizeof(*frame));
}

static void *respond_to_calibration_query(void *opaque)
{
	struct responder_args *args = opaque;
	struct can_frame query;
	struct can_frame frame;

	g_assert_cmpint(read(args->fd, &query, sizeof(query)), ==, sizeof(query));

	frame = point_frame(args->address + 1, 1, 0, meas, -1.0f, sizeof(sdaq_calibration_points_data));
	send_frame(args->fd, &frame);
	frame = point_frame(args->address, 2, 0, meas, -2.0f, sizeof(sdaq_calibration_points_data));
	send_frame(args->fd, &frame);
	frame = point_frame(args->address, 1, 1, meas, -3.0f, sizeof(sdaq_calibration_points_data));
	send_frame(args->fd, &frame);
	frame = point_frame(args->address, 1, 0, meas, -4.0f, sizeof(sdaq_calibration_points_data) - 1);
	send_frame(args->fd, &frame);

	frame = point_frame(args->address, 1, 0, meas, 10.0f, sizeof(sdaq_calibration_points_data));
	send_frame(args->fd, &frame);
	send_frame(args->fd, &frame);
	for(unsigned char type=C3; type>meas; type--)
	{
		frame = point_frame(args->address, 1, 0, type, 10.0f + type, sizeof(sdaq_calibration_points_data));
		send_frame(args->fd, &frame);
	}
	frame = date_frame(args->address, 1, 1);
	send_frame(args->fd, &frame);
	send_frame(args->fd, &frame);
	return NULL;
}

struct setinfo_responder_args {
	int fd;
	unsigned char address;
	gboolean inject_residue;
	gboolean mismatch_date;
};

static void assert_request_type(int fd, unsigned char expected_type)
{
	struct can_frame frame;
	sdaq_can_id *id = (sdaq_can_id *)&frame.can_id;

	g_assert_cmpint(read(fd, &frame, sizeof(frame)), ==, sizeof(frame));
	g_assert_cmpuint(id->payload_type, ==, expected_type);
}

static void *respond_to_zero_point_setinfo(void *opaque)
{
	struct setinfo_responder_args *args = opaque;
	struct can_frame frame;

	assert_request_type(args->fd, Query_Dev_info);
	frame = status_frame(args->address);
	send_frame(args->fd, &frame);
	frame = info_frame(args->address);
	send_frame(args->fd, &frame);
	frame = date_frame(args->address, 1, 0);
	send_frame(args->fd, &frame);

	assert_request_type(args->fd, Write_calibration_Date);
	assert_request_type(args->fd, Query_Calibration_Data);
	for(unsigned char type=meas; type<=C3; type++)
	{
		float value = args->inject_residue && type == gain ? 1.19455e-15f : NAN;

		frame = point_frame(args->address, 1, 0, type, value,
				sizeof(sdaq_calibration_points_data));
		send_frame(args->fd, &frame);
	}
	frame = date_frame(args->address, 1, 0);
	if(args->mismatch_date)
		((sdaq_calibration_date *)frame.data)->day = 24;
	send_frame(args->fd, &frame);
	return NULL;
}

static int run_zero_point_setinfo(gboolean inject_residue, gboolean mismatch_date)
{
	struct setinfo_responder_args args = {
		.address = 15,
		.inject_residue = inject_residue,
		.mismatch_date = mismatch_date
	};
	struct itimerval stop_timer = {0};
	opt_flags options = {.silent = 1, .verify = 1, .timeout = 1};
	pthread_t responder;
	gchar *xml = make_xml("12", "0", "1", "1", NULL, "1");
	gchar *path = NULL;
	int fd = g_file_open_tmp("sdaq-setinfo-test-XXXXXX.xml", &path, NULL);
	int sockets[2];
	int result;

	g_assert_cmpint(fd, >=, 0);
	close(fd);
	g_assert_true(g_file_set_contents(path, xml, -1, NULL));
	g_assert_cmpint(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets), ==, 0);
	args.fd = sockets[1];
	options.info_file = path;
	g_assert_cmpint(pthread_create(&responder, NULL, respond_to_zero_point_setinfo, &args), ==, 0);
	result = setinfo(sockets[0], args.address, &options);
	g_assert_cmpint(pthread_join(responder, NULL), ==, 0);
	setitimer(ITIMER_REAL, &stop_timer, NULL);

	close(sockets[0]);
	close(sockets[1]);
	g_assert_cmpint(unlink(path), ==, 0);
	g_free(path);
	g_free(xml);
	return result;
}

static void test_setinfo_verifies_zero_point_metadata(void)
{
	g_assert_cmpint(run_zero_point_setinfo(FALSE, FALSE), ==, EXIT_SUCCESS);
	g_assert_cmpint(run_zero_point_setinfo(TRUE, FALSE), ==, EXIT_SUCCESS);
	g_assert_cmpint(run_zero_point_setinfo(TRUE, TRUE), ==, EXIT_FAILURE);
}

static void alarm_handler(int signum)
{
	(void)signum;
	TMR_exp = 0;
}

static void test_can_receive_filtering_and_unique_keys(void)
{
	SDAQ_info_cal_data conf;
	struct responder_args args = {.address = 15};
	pthread_t responder;
	int sockets[2];
	struct itimerval stop_timer = {0};

	init_config(&conf, 1);
	g_slist_free_full((GSList *)conf.Cal_points_data_lists[0], free_SDAQ_cal_point_node);
	conf.Cal_points_data_lists[0] = NULL;
	g_assert_cmpint(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets), ==, 0);
	args.fd = sockets[1];
	signal(SIGALRM, alarm_handler);
	g_assert_cmpint(pthread_create(&responder, NULL, respond_to_calibration_query, &args), ==, 0);

	g_assert_cmpint(get_SDAQ_calibration_data(sockets[0], args.address, 1, &conf, NULL), ==, EXIT_SUCCESS);
	g_assert_cmpint(pthread_join(responder, NULL), ==, 0);
	setitimer(ITIMER_REAL, &stop_timer, NULL);
	g_assert_cmpuint(g_slist_length((GSList *)conf.Cal_points_data_lists[0]), ==, MAX_DATA_ON_POINT);
	for(unsigned char type=meas; type<=C3; type++)
	{
		sdaq_calibration_points_data key = {.type = type, .points_num = 0};
		GSList *found = g_slist_find_custom((GSList *)conf.Cal_points_data_lists[0], &key,
				SDAQ_point_node_with_type_and_num_find);
		g_assert_nonnull(found);
	}

	close(sockets[0]);
	close(sockets[1]);
	free_SDAQ_info_cal_data(&conf);
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/calibration/struct-sizes-and-free", test_struct_sizes_and_free);
	g_test_add_func("/calibration/xml/period-and-point-bounds", test_xml_period_and_point_bounds);
	g_test_add_func("/calibration/xml/float-validation", test_xml_float_validation);
	g_test_add_func("/calibration/xml/float32-round-trip", test_xml_float32_round_trip);
	g_test_add_func("/calibration/point-correlation-by-key", test_point_correlation_by_key);
	g_test_add_func("/calibration/point-correlation/ignores-inactive-fields", test_point_correlation_ignores_inactive_fields);
	g_test_add_func("/calibration/point-correlation/empty-target-ignores-inactive-fields", test_empty_target_ignores_inactive_fields);
	g_test_add_func("/calibration/write-sequence-is-bounded", test_write_sequence_is_bounded);
	g_test_add_func("/calibration/can/filtering-and-unique-keys", test_can_receive_filtering_and_unique_keys);
	g_test_add_func("/calibration/setinfo/verifies-zero-point-metadata", test_setinfo_verifies_zero_point_metadata);
	return g_test_run();
}
