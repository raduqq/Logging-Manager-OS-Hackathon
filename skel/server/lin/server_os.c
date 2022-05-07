/**
 * Hackathon SO: LogMemCacher
 * (c) 2020-2021, Operating Systems
 */
#include "../../include/server.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

char *lmc_logfile_path;

/**
 * Client connection loop function. Creates the appropriate client connection
 * socket and receives commands from the client in a loop.
 *
 * @param client_sock: Socket to communicate with the client.
 *
 * @return: This function always returns 0.
 *
 * The lmc_get_command function executes blocking operations. The server
 * is unable to handle multiple connections simultaneously.
 *
 * Server deals with this problem by creating a process for every client
 */
static int lmc_client_function(SOCKET client_sock)
{
	pid_t client_pid = fork();

	int rc;
	struct lmc_client *client;

	switch (client_pid) {
	case -1:
		// Fork error
		DIE(client_pid, "fork client_pid");
		break;
	case 0:
		// Client process

		client = lmc_create_client(client_sock);

		while (1) {
			rc = lmc_get_command(client);
			if (rc == -1)
				break;
		}

		close(client_sock);
		free(client);
	default:
		// Parent process
		// wait_ret = waitpid(client_pid, &status, 0);
		// DIE(wait_ret < 0, "waitpid client_pid");
		break;
	}

	return 0;
}

/**
 * Server main loop function. Opens a socket in listening mode and waits for
 * connections.
 */
void lmc_init_server_os(void)
{
	int sock, client_size, client_sock;
	struct sockaddr_in server, client;
	int opten;

	memset(&server, 0, sizeof(struct sockaddr_in));

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return;

	opten = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opten, sizeof(opten));

	server.sin_family = AF_INET;
	server.sin_port = htons(LMC_SERVER_PORT);
	server.sin_addr.s_addr = inet_addr(LMC_SERVER_IP);

	if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("Could not bind");
		exit(1);
	}

	if (listen(sock, LMC_DEFAULT_CLIENTS_NO) < 0) {
		perror("Error while listening");
		exit(1);
	}

	while (1) {
		memset(&client, 0, sizeof(struct sockaddr_in));
		client_size = sizeof(struct sockaddr_in);
		client_sock = accept(sock, (struct sockaddr *)&client, (socklen_t *)&client_size);

		if (client_sock < 0) {
			perror("Error while accepting clients");
		}

		lmc_client_function(client_sock);
	}
}

/**
 * OS-specific client cache initialization function.
 *
 * @param cache: Cache structure to initialize.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * Implement proper handling logic.
 *
 * Initialize uninitialized fields
 */

int lmc_init_client_cache(struct lmc_cache *cache)
{
	void *addr = mmap(NULL, sizeof(struct log_in_memory), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	cache->ptr = addr;
	((struct log_in_memory *)cache->ptr)->no_logs = 0;
	((struct log_in_memory *)cache->ptr)->no_logs_stored_on_disk = 0;
	((struct log_in_memory *)cache->ptr)->list_of_logs = NULL;
	cache->pages = 0;
	return 0;
}

/**
 * OS-specific function that handles adding a log line to the cache.
 *
 * @param client: Client connection;
 * @param log: Log line to add to the cache.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * TODO: Implement proper handling logic. Must be able to dynamically resize the
 * cache if it is full.
 */
int lmc_add_log_os(struct lmc_client *client, struct lmc_client_logline *log)
{
	int page_size = getpagesize();

	struct log_in_memory *lim = client->cache->ptr;

	if (lim->no_logs == 0) {
		lim->list_of_logs = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
		client->cache->pages = 1;
	} else {
		if ((lim->no_logs + 1) * sizeof(struct lmc_client_logline) < client->cache->pages * page_size) {
			// imi incape in memorie pentru inca un log

		} else {
			// trebuie sa mai aloc o pagina
			void *newAddr = mmap(NULL, (client->cache->pages + 1) * page_size, PROT_READ | PROT_WRITE,
					     MAP_ANON | MAP_SHARED, -1, 0);
			memcpy(newAddr, lim->list_of_logs, lim->no_logs * sizeof(struct lmc_client_logline));
			munmap(lim->list_of_logs, client->cache->pages * page_size);
			lim->list_of_logs = newAddr;
		}
	}

	memcpy(&(lim->list_of_logs[lim->no_logs]), log, sizeof(struct lmc_client_logline));
	lim->no_logs++;

	return 0;
}

/**
 * OS-specific function that handles flushing the cache to disk,
 *
 * @param client: Client connection.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * TODO: Implement proper handling logic.
 */
int lmc_flush_os(struct lmc_client *client)
{
	//
	struct log_in_memory *lim = client->cache->ptr;
	char buffer[512];
	sprintf(buffer, "%s/%s.log", "logs_logmemcache", client->cache->service_name);
	lmc_init_logdir("logs_logmemcache");
	lmc_rotate_logfile(buffer);
	int fd = open(buffer, O_WRONLY);

	for (int i = lim->no_logs_stored_on_disk; i < lim->no_logs; i++) {
		write(fd, &(lim->list_of_logs[i]), sizeof(lim->list_of_logs[i]));
	}

	lim->no_logs_stored_on_disk = lim->no_logs;

	return 0;
}

/**
 * OS-specific function that handles client unsubscribe requests.
 *
 * @param client: Client connection.
 *
 * @return: 0 in case of success, or -1 otherwise.
 *
 * TODO: Implement proper handling logic. Must flush the cache to disk and
 * deallocate any structures associated with the client.
 */
int lmc_unsubscribe_os(struct lmc_client *client) { return 0; }
