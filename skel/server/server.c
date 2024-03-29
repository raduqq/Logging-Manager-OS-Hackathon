/**
 * Hackathon SO: LogMemCacher
 * (c) 2020-2021, Operating Systems
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/server.h"

#ifdef __unix__
#include <sys/socket.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

static struct lmc_cache **lmc_caches;
static size_t lmc_cache_count;
static size_t lmc_max_caches;

/* Server API */

/**
 * Initialize client cache list on the server.
 */
static void lmc_init_client_list(void)
{
	lmc_max_caches = LMC_DEFAULT_CLIENTS_NO;
	lmc_caches = malloc(lmc_max_caches * sizeof(*lmc_caches));
}

/**
 * Initialize server - allocate initial cache list and start listening on the
 * server's socket.
 */
static void lmc_init_server(void)
{
	lmc_init_client_list();
	lmc_init_server_os();
}

/**
 * Create a client connection structure. Called to allocate the client
 * connection structure by the code that handles the server loop.
 *
 * @param client_sock: Socket used to communicate with the client.
 *
 * @return: A pointer to a client connection structure on success,
 *          or NULL otherwise.
 */
struct lmc_client *lmc_create_client(SOCKET client_sock)
{
	struct lmc_client *client;

	client = malloc(sizeof(*client));
	client->client_sock = client_sock;
	client->cache = NULL;

	return client;
}

/**
 * Handle client connect.
 *
 * Locate a cache entry for the client and allot it to the client connection
 * (populate the cache field of the client connection structure).
 * If the client already has an existing connection (and respective cache) use
 * the same cache. Otherwise, locate a free cache.
 *
 * @param client: Client connection;
 * @param name: The name (identifier) of the client.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * TODO: Must be able to handle the case when all caches are occupied.
 */
static int lmc_add_client(struct lmc_client *client, char *name)
{
	int err = 0;
	size_t i;

	for (i = 0; i < lmc_cache_count; i++) {
		if (lmc_caches[i] == NULL)
			continue;
		if (lmc_caches[i]->service_name == NULL)
			continue;
		if (strcmp(lmc_caches[i]->service_name, name) == 0) {
			client->cache = lmc_caches[i];
			goto found;
		}
	}

	if (lmc_cache_count == lmc_max_caches) {
		return -1;
	}

	client->cache = malloc(sizeof(*client->cache));
	client->cache->service_name = strdup(name);
	lmc_caches[lmc_cache_count] = client->cache;
	lmc_cache_count++;

	err = lmc_init_client_cache(client->cache);
found:
	return err;
}
/**
 * Handle client connect
 *
 *
 * @param client Client structure
 * @param name Client name
 * @return 1 if error, 0 if good
 */
static int lmc_connect_client(struct lmc_client *client, char *name)
{
	int err = -1;
	size_t i;

	for (i = 0; i < lmc_cache_count; i++) {
		if (lmc_caches[i] == NULL)
			continue;
		if (lmc_caches[i]->service_name == NULL)
			continue;
		if (strcmp(lmc_caches[i]->service_name, name) == 0) {
			client->cache = lmc_caches[i];
			goto found;
		}
	}

	if (lmc_cache_count == lmc_max_caches) {
		return -1;
	}

	client->cache = malloc(sizeof(*client->cache));
	client->cache->service_name = strdup(name);
	lmc_caches[lmc_cache_count] = client->cache;
	lmc_cache_count++;

	err = lmc_init_client_cache(client->cache);
found:
	return err;
}

/**
 * Handle client disconnect.
 *
 * @param client: Client connection.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * Implement proper handling logic.
 */
static int lmc_disconnect_client(struct lmc_client *client)
{
	int err = -1;
	size_t i;

	printf("%s\n", client->cache->service_name);

	for (i = 0; i < lmc_cache_count; i++) {
		if (lmc_caches[i] == NULL)
			continue;
		if (lmc_caches[i]->service_name == NULL)
			continue;
		if (strcmp(lmc_caches[i]->service_name, client->cache->service_name) == 0) {
			// TODO Terminate session
			err = 0;
			goto found;
		}
	}
found:
	return err;
}

/**
 * Handle unsubscription requests.
 *
 * @param client: Client connection.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * Implement proper handling logic.
 */
static int lmc_unsubscribe_client(struct lmc_client *client)
{
	int err = -1;
	size_t i;

	printf("%s\n", client->cache->service_name);

	for (i = 0; i < lmc_cache_count; i++) {
		if (lmc_caches[i] == NULL)
			continue;
		if (lmc_caches[i]->service_name == NULL)
			continue;
		if (strcmp(lmc_caches[i]->service_name, client->cache->service_name) == 0) {
			// remove this from the array
			int j;
			for (j = i + 1; j < lmc_cache_count; j++) {
				lmc_caches[j - 1] = lmc_caches[j];
			}
			lmc_cache_count--;

			lmc_unsubscribe_os(client);
			err = 0;
			goto found;
		}
	}
found:
	return err;
}

/**
 * Add a log line to the client's cache.
 *
 * @param client: Client connection;
 * @param log: Log line to add to the cache.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * TODO DONE: Implement proper handling logic.
 */

static int lmc_add_log(struct lmc_client *client, struct lmc_client_logline *log)
{
	lmc_add_log_os(client, log);
	return 0;
}

/**
 * Flush client logs to disk.
 *
 * @param client: Client connection.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * TODO DONE: Implement proper handling logic.
 */
static int lmc_flush(struct lmc_client *client)
{
	lmc_flush_os(client);
	return 0;
}

/**
 * Send stats about the stored logs to the client. Must not send the actual
 * logs, but a string formatted in LMC_STATS_FORMAT. Should contain the current
 * time on the server, allocated memory in KBs and the number of log lines
 * stored.
 *
 * @param client: Client connection.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * TODO DONE: Implement proper handling logic.
 */
static int lmc_send_stats(struct lmc_client *client)
{
	// Get server time
	char time_buf[LMC_TIME_SIZE];

	// Get allocated memory
	int page_size = 0; // getpagesize();
	unsigned long used_memory = client->cache->pages * page_size;

	// Get number of log lines
	struct log_in_memory *lim = client->cache->ptr;

	unsigned long log_lines_cnt = lim->no_logs;

	char stats[LMC_STATUS_MAX_SIZE];

	int buf_len = 0;

	lmc_crttime_to_str(time_buf, LMC_TIME_SIZE, LMC_TIME_FORMAT);

	// Build stats

	memset(stats, 0, LMC_STATUS_MAX_SIZE);
	sprintf(stats, LMC_STATS_FORMAT, time_buf, used_memory, log_lines_cnt);

	// Send stats
	buf_len = strlen(stats);
	lmc_send(client->client_sock, stats, buf_len, LMC_SEND_FLAGS);

	return 0;
}

/**
 * Send the stored log lines to the client.
 * The server must first send the number of lines, and then the log lines,
 * one by one.
 *
 * @param client: Client connection.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * TODO DONE: Implement proper handling logic.
 */
static int lmc_send_loglines(struct lmc_client *client)
{
	struct log_in_memory *lim = client->cache->ptr;
	unsigned long number_of_lines = lim->no_logs;
	char buffer[128];
	int i;

	sprintf(buffer, "%ld", number_of_lines);
	lmc_send(client->client_sock, buffer, sizeof(buffer), LMC_SEND_FLAGS);

	for (i = 0; i < number_of_lines; i++) {
		lmc_send(client->client_sock, &(lim->list_of_logs[i]), sizeof(struct lmc_client_logline),
			 LMC_SEND_FLAGS);
	}

	return 0;
}

static int is_in_interval(char *time, char *start, char *end)
{
	if (end[0] == '\0') {
		// nu exista end
		return strcmp(time, start) >= 0;
	}
	return strcmp(time, start) >= 0 && strcmp(time, end) <= 0;
}

static int lmc_send_loglines_interval(struct lmc_client *client, char *args)
{

	char time1[21];
	char time2[21];

	struct log_in_memory *lim = client->cache->ptr;
	unsigned long number_of_lines = lim->no_logs;
	char buffer[128];
	int i;

	memset(time1, 0, sizeof(time1));
	memset(time2, 0, sizeof(time2));

	memcpy(time1, args, sizeof(time1) - 1);
	memcpy(time2, args, sizeof(time2) - 1);

	sprintf(buffer, "%ld", number_of_lines);
	lmc_send(client->client_sock, buffer, sizeof(buffer), LMC_SEND_FLAGS);

	for (i = 0; i < number_of_lines; i++) {
		if (is_in_interval(lim->list_of_logs[i].time, time1, time2)) {
			lmc_send(client->client_sock, &(lim->list_of_logs[i]), sizeof(struct lmc_client_logline),
				 LMC_SEND_FLAGS);
		}
	}

	return 0;
}

/**
 * Parse a command from the client. The command must be in the following format:
 * "cmd data", with a single space between the command and the associated data.
 *
 * @param cmd: Parsed command structure;
 * @param string: Command string;
 * @param datalen: The amount of data send with the command.
 */
static void lmc_parse_command(struct lmc_command *cmd, char *string, ssize_t *datalen)
{
	char *command, *line;

	command = strdup(string);
	line = strchr(command, ' ');

	cmd->data = NULL;
	if (line != NULL) {
		line[0] = '\0';
		cmd->data = strdup(line + 1);
		*datalen -= strlen(command) + 1;
	}

	cmd->op = lmc_get_op_by_str(command);

	printf("command = %s, line = %s\n", cmd->op->op_str, cmd->data ? cmd->data : "null");

	free(command);
}

/**
 * Validate command argument. The command argument (data) can only contain
 * printable ASCII characters.
 *
 * @param line: Command data;
 * @param len: Length of the data string.
 *
 * @return: 0 in case of success, or -1 otherwise.
 */
static int lmc_validate_arg(const char *line, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		if (!isprint(line[i]))
			return -1;

	return 0;
}

/**
 * @brief Creates a new logline
 *
 * @param cmd to extract log data from
 * @return struct lmc_client_logline* newly crated logline
 */
struct lmc_client_logline *lmc_create_logline(struct lmc_command cmd)
{
	// Alloc struct
	struct lmc_client_logline *log = malloc(sizeof(struct lmc_client_logline));

	// Copy data to fields
	memcpy(log->time, cmd.data, LMC_TIME_SIZE);
	log->time[LMC_TIME_SIZE - 1] = '\0';

	memcpy(log->logline, cmd.data + LMC_TIME_SIZE, LMC_LOGLINE_SIZE);
	log->logline[LMC_LOGLINE_SIZE - 1] = '\0';

	return log;
}

/**
 * Wait for a command from the client and handle it when it is received.
 * The server performs blocking receive operations in this function. When the
 * command is received, parse it and then call the appropriate handling
 * function, depending on the command.
 *
 * @param client: Client connection.
 *
 * @return: 0 in case of success, or -1 otherwise.
 */
int lmc_get_command(struct lmc_client *client)
{
	int err;
	ssize_t recv_size;
	char buffer[LMC_COMMAND_SIZE], response[LMC_LINE_SIZE];
	char *reply_msg;
	struct lmc_command cmd;
	struct lmc_client_logline *log;

	int flag = 0;

	err = -1;

	memset(&cmd, 0, sizeof(cmd));
	memset(buffer, 0, sizeof(buffer));
	memset(response, 0, sizeof(response));

	recv_size = lmc_recv(client->client_sock, buffer, sizeof(buffer), 0);
	if (recv_size <= 0)
		return -1;

	lmc_parse_command(&cmd, buffer, &recv_size);
	if (recv_size > LMC_LINE_SIZE) {
		reply_msg = "message too long";
		goto end;
	}

	if (cmd.op->requires_auth && client->cache->service_name == NULL) {
		reply_msg = "authentication required";
		goto end;
	}

	if (cmd.data != NULL) {
		err = lmc_validate_arg(cmd.data, recv_size);
		if (err != 0) {
			reply_msg = "invalid argument provided";
			goto end;
		}
	}

	switch (cmd.op->code) {
	case LMC_CONNECT:
		err = lmc_connect_client(client, cmd.data);
		break;
	case LMC_SUBSCRIBE:
		err = lmc_add_client(client, cmd.data);
		break;
	case LMC_STAT:
		err = lmc_send_stats(client);
		break;
	case LMC_ADD:
		/* Parse the client data and create a log line structure */
		log = lmc_create_logline(cmd);

		// Call command handler
		err = lmc_add_log(client, log);

		// Free aux resources
		free(log);
		break;
	case LMC_FLUSH:
		err = lmc_flush(client);
		break;
	case LMC_DISCONNECT:
		err = lmc_disconnect_client(client);
		flag = 1;
		break;
	case LMC_UNSUBSCRIBE:
		flag = 1;
		err = lmc_unsubscribe_client(client);
		break;
	case LMC_GETLOGS:
		if (cmd.data != NULL) {
			err = lmc_send_loglines_interval(client, cmd.data);
		} else {
			err = lmc_send_loglines(client);
		}
		break;
	default:
		/* unknown command */
		err = -1;
		break;
	}

	reply_msg = cmd.op->op_reply;

end:
	if (err == 0)
		sprintf(response, "%s", reply_msg);
	else
		sprintf(response, "FAILED: %s", reply_msg);

	if (cmd.data != NULL)
		free(cmd.data);
	if (flag == 0) {
		return lmc_send(client->client_sock, response, LMC_LINE_SIZE, LMC_SEND_FLAGS);
	}

	lmc_send(client->client_sock, response, LMC_LINE_SIZE, LMC_SEND_FLAGS);
	return -1;
}

int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);

	if (argc == 1)
		lmc_logfile_path = strdup("logs_lmc");
	else
		lmc_logfile_path = strdup(argv[1]);

	if (lmc_init_logdir(lmc_logfile_path) < 0)
		exit(-1);

	lmc_init_server();

	return 0;
}
