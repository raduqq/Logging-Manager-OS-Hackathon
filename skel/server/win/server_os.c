/**
 * Hackathon SO: LogMemCacher
 * (c) 2020-2021, Operating Systems
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")

#include "../../include/server.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

char *lmc_logfile_path;

/**
 * Client connection loop function. Creates the appropriate client connection
 * socket and receives commands from the client in a loop.
 *
 * @param client_sock: Socket to communicate with the client.
 *
 * @return: This function always returns 0.
 *
 * TODO: The lmc_get_command function executes blocking operations. The server
 * is unable to handle multiple connections simultaneously.
 */
static DWORD WINAPI lmc_client_function(SOCKET client_sock)
{
	int rc;
	struct lmc_client *client;

	client = lmc_create_client(client_sock);

	while (1) {
		rc = lmc_get_command(client);
		if (rc == -1)
			break;
	}

	closesocket(client_sock);
	free(client);

	return 0;
}

/**
 * Server main loop function. Opens a socket in listening mode and waits for
 * connections.
 */
void lmc_init_server_os(void)
{
	SOCKET sock, client_sock;
	int client_size;
	struct sockaddr_in server, client;
	WSADATA wsaData;
	int rc;
	int opten;

	memset(&server, 0, sizeof(struct sockaddr_in));

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc != NO_ERROR)
		return;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		WSACleanup();
		return;
	}

	opten = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opten, sizeof(opten));

	server.sin_family = AF_INET;
	server.sin_port = htons(LMC_SERVER_PORT);
	server.sin_addr.s_addr = inet_addr(LMC_SERVER_IP);

	if (bind(sock, (SOCKADDR *)&server, sizeof(server)) == SOCKET_ERROR) {
		closesocket(sock);
		perror("Could not bind");
		WSACleanup();
		exit(1);
	}

	if (listen(sock, LMC_DEFAULT_CLIENTS_NO) == SOCKET_ERROR) {
		perror("Error while listening");
		exit(1);
	}

	while (1) {
		memset(&client, 0, sizeof(struct sockaddr_in));
		client_size = sizeof(struct sockaddr_in);
		client_sock = accept(sock, (SOCKADDR *)&client, (socklen_t *)&client_size);
		if (client_sock == INVALID_SOCKET) {
			perror("Error while accepting clients");
			continue;
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
 * TODO: Implement proper handling logic.
 */
int lmc_init_client_cache(struct lmc_cache *cache) { 
	void *addr_res = VirtualAlloc(NULL, 4096, MEM_RESERVE, PAGE_READWRITE);
	void *addr = VirtualAlloc(addr_res, 4096, MEM_COMMIT, PAGE_READWRITE);
	//void *addr = mmap(NULL, sizeof(struct log_in_memory), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	cache->ptr = addr;
	((struct log_in_memory *)cache->ptr)->no_logs = 0;
	((struct log_in_memory *)cache->ptr)->no_logs_stored_on_disk = 0;
	((struct log_in_memory *)cache->ptr)->list_of_logs = NULL;
	cache->pages = 0;
	return 0; }

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
int lmc_add_log_os(struct lmc_client *client, struct lmc_client_logline *log) { 

	int page_size = 4096;

	struct log_in_memory *lim = client->cache->ptr;
	void *newAddr, *addr_res;
	
	if (lim->no_logs == 0) {
		addr_res = VirtualAlloc(NULL, page_size, MEM_RESERVE, PAGE_READWRITE);
		lim->list_of_logs = VirtualAlloc(addr_res, page_size, MEM_COMMIT, PAGE_READWRITE);
		client->cache->pages = 1;
	} else {
		if ((lim->no_logs + 1) * sizeof(struct lmc_client_logline) < client->cache->pages * page_size) {
			// imi incape in memorie pentru inca un log

		} else {
			// trebuie sa mai aloc o pagina
			addr_res = VirtualAlloc(NULL, (client->cache->pages + 1) * page_size, MEM_RESERVE, PAGE_READWRITE);
			newAddr = VirtualAlloc(addr_res, (client->cache->pages + 1) * page_size, MEM_COMMIT, PAGE_READWRITE);
			memcpy(newAddr, lim->list_of_logs, lim->no_logs * sizeof(struct lmc_client_logline));
			VirtualFree(lim->list_of_logs, client->cache->pages * page_size, MEM_DECOMMIT);
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
int lmc_flush_os(struct lmc_client *client) { 
	struct log_in_memory *lim = client->cache->ptr;
	char buffer[512];
	HANDLE fd;
	int i;
	int bytesWritten = 0;
	
	sprintf(buffer, "%s/%s.log", "logs_logmemcache", client->cache->service_name);
	lmc_init_logdir("logs_logmemcache");
	lmc_rotate_logfile(buffer);
	// int fd = open(buffer, O_WRONLY | O_CREAT);
	fd = CreateFile(buffer, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (fd < 0) {
		printf("eroare! %d\n", fd);
	}
	for (i = lim->no_logs_stored_on_disk; i < lim->no_logs; i++) {
		WriteFile(fd, &(lim->list_of_logs[i]), sizeof(lim->list_of_logs[i]), &bytesWritten, NULL);
		//write(fd, &(lim->list_of_logs[i]), sizeof(lim->list_of_logs[i]));
	}

	lim->no_logs_stored_on_disk = lim->no_logs;
	// close(fd);
	CloseHandle(fd);
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
int lmc_unsubscribe_os(struct lmc_client *client) { 

	int page_size = 4096;
	
	struct log_in_memory *lim;
	// flush them maybe?
	lmc_flush_os(client);

	// free the fields
	free(client->cache->service_name);

	// free cache with munmap
	lim = client->cache->ptr;
	VirtualFree(lim->list_of_logs, client->cache->pages * page_size, MEM_DECOMMIT);

	VirtualFree(lim, sizeof(struct log_in_memory), MEM_DECOMMIT);

	free(client->cache);
	return 0;

 }
