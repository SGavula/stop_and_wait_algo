// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//
#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <chrono>
#include <thread>
#include "ws2tcpip.h"
#include <boost/crc.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md5.h"
#include "md5.c"

#define TARGET_IP "127.0.0.1"
#define SENDER
//#define RECEIVER

#ifdef SENDER
#define TARGET_PORT 14000
#define LOCAL_PORT 15001
#endif // SENDER

#ifdef RECEIVER
#define TARGET_PORT 8888
#define LOCAL_PORT 5000
#endif // RECEIVER

#define BUFFERS_LEN 1024
#define BUFFERS_LEN_WITHOUT_CRC 1020
#define BUFFERS_FILE_LEN 1016
#define BUFFERS_FILE_DATA_LEN 1012
#define HASH_LEN 16


void tochararr(unsigned int x, unsigned char* res)
{
	/*Function writes each byte of int x into char given array*/

	res[0] = res[1] = res[2] = res[3] = 0;
	for (int i = 0; i < 32; i++) {
		int b = ((1 << i) & x);
		b >>= ((i / 8) * 8);
		res[i / 8] |= b;
	}
	return;
}

int to_int(unsigned char letter0, unsigned char letter1, unsigned char letter2, unsigned char letter3)
{
	/*Function decodes 4 bytes into single integer*/

	int res = letter0 | (letter1 << 8) | (letter2 << 16) | (letter3 << 24);
	return res;
}

void print_hash(uint8_t* p) {
	for (unsigned int i = 0; i < 16; ++i) {
		printf("%02x", p[i]);
	}
	printf("\n");
}

unsigned long get_crc(char buffer[BUFFERS_LEN]) {
	boost::crc_32_type crc32;
	unsigned long crc_value = 0;
	crc32.process_bytes(buffer, BUFFERS_LEN_WITHOUT_CRC);
	crc_value = crc32.checksum();
	crc32.reset();
	return crc_value;
}

void add_pack_num_to_buffer(char buffer_tx[BUFFERS_LEN], int pack_num) {
	unsigned char num[4] = { 0 };
	tochararr(pack_num, num);
	//Set packet nuumber to first four bytes
	for (int i = 0; i < 4; i++) {
		buffer_tx[i] = num[i];
	}
}

void add_crc_value_to_buffer(char buffer_tx[BUFFERS_LEN], unsigned long crc_value) {
	unsigned char num[4] = { 0 };
	tochararr(crc_value, num);
	//Set crc to last four bytes
	for (int i = 0; i < 4; i++) {
		buffer_tx[i + BUFFERS_LEN_WITHOUT_CRC] = num[i];
	}
}

void add_data_len_to_buffer(char buffer_tx[BUFFERS_LEN], int readcnt) {
	unsigned char num[4] = { 0 };
	tochararr(readcnt, num);
	//Set data_len to second four bytes
	for (int i = 0; i < 4; i++) {
		buffer_tx[i + 4] = num[i];
	}
}

bool check_acknowledge(int bytes_received, char buffer_rx[BUFFERS_LEN]) {
	bool res = false;
	char buffer_ack_rec[BUFFERS_FILE_LEN];
	char buffer_ack[BUFFERS_FILE_LEN];
	memset(buffer_ack, 0, BUFFERS_FILE_LEN);
	memcpy(buffer_ack, "ack", 3);
	// Check for receive errors and timeout
	if (bytes_received < 0) {
		// Handle receive error
		perror("recvfrom failed");
		res = false;
	}
	else if (bytes_received == 0) {
		// Handle timeout
		printf("Timeout occurred, resending packet...\n");
		res = false;
	}
	else {
		// Receive successful
		printf("Received %d bytes\n", bytes_received);
		memcpy(buffer_ack_rec, buffer_rx + 4, BUFFERS_FILE_LEN);
		if (strcmp(buffer_ack_rec, buffer_ack) == 0) {
			printf("Data sent successfully and acknowledged by receiver.\n");
			res = true;
		}
	}
	return res;
}

void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//**********************************************************************
int main()
{
	SOCKET socketS;

	InitWinsock();

	struct sockaddr_in local;
	struct sockaddr_in from;

	int fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;


	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	
	DWORD read_timeout = 5000;
	setsockopt(socketS, SOL_SOCKET, SO_RCVTIMEO, (char*)&read_timeout, sizeof read_timeout);

	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("Binding error!\n");
		getchar(); //wait for press Enter
		return 1;
	}
	//**********************************************************************
	char buffer_rx[BUFFERS_LEN];
	char buffer_tx[BUFFERS_LEN];
	uint8_t hash_buffer[HASH_LEN];

#ifdef SENDER

	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	unsigned long crc_value;
	unsigned int pack_num = 1;
	FILE* fd;

	printf("Sending packet.\n");

	//Send file name to receiver
	memset(buffer_tx, 0, BUFFERS_LEN);
	const char* filename = "test.jpg";
	add_pack_num_to_buffer(buffer_tx, pack_num);
	//Set file
	memcpy(buffer_tx + 4, filename, strlen(filename));
	// Get CRC
	crc_value = get_crc(buffer_tx);
	add_crc_value_to_buffer(buffer_tx, crc_value);

	while (1) {
		printf("Sending packet file name...\n");
		sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
		// Receive data using recvfrom() function
		int bytes_received = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
		bool res = check_acknowledge(bytes_received, buffer_rx);
		if (res == true) {
			break;
		}
	}
	pack_num++;

	fd = fopen(filename, "rb");
	if (fd == NULL) {
		perror("Can't open file");
		exit(1);
	}

	//Send hash
	memset(buffer_tx, 0, BUFFERS_LEN);
	//Get hash
	md5File(fd, hash_buffer);
	fclose(fd);
	add_pack_num_to_buffer(buffer_tx, pack_num);
	//Set hash
	memcpy(buffer_tx + 4, hash_buffer, HASH_LEN);
	// Get CRC
	crc_value = get_crc(buffer_tx);
	add_crc_value_to_buffer(buffer_tx, crc_value);
	while (1) {
		sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
		// Receive data using recvfrom() function
		int bytes_received = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
		bool res = check_acknowledge(bytes_received, buffer_rx);
		if (res == true) {
			break;
		}
	}
	pack_num++;
	fclose(fd);

	fd = fopen(filename, "rb");
	if (fd == NULL) {
		perror("Can't open file");
		exit(1);
	}

	//Sending data
	while (1) {
		printf("Sending packet data %i\n", pack_num);
		memset(buffer_tx, 0, BUFFERS_LEN);
		int readcnt = fread(buffer_tx + 8, 1, BUFFERS_FILE_DATA_LEN, fd);
		printf("Read return: %i\n", readcnt);
		if (readcnt == 0) {
			break;
		}
		add_pack_num_to_buffer(buffer_tx, pack_num);
		add_data_len_to_buffer(buffer_tx, readcnt);
		// Get CRC
		crc_value = get_crc(buffer_tx);
		add_crc_value_to_buffer(buffer_tx, crc_value);

		while (1) {
			sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
			// Receive data using recvfrom() function
			int bytes_received = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
			bool res = check_acknowledge(bytes_received, buffer_rx);
			if (res == true) {
				break;
			}
		}
		pack_num++;
	}

	fclose(fd);

	//Send end string
	int counter = 0;
	memset(buffer_tx, 0, BUFFERS_LEN);
	const char* end_string = "STOOOP";
	add_pack_num_to_buffer(buffer_tx, pack_num);
	//Set end string
	memcpy(buffer_tx + 8, end_string, strlen(end_string));
	// Get CRC
	crc_value = get_crc(buffer_tx);
	add_crc_value_to_buffer(buffer_tx, crc_value);

	while (1) {
		if (counter >= 5) {
			break;
		}
		sendto(socketS, buffer_tx, sizeof(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));
		int bytes_received = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
		bool res = check_acknowledge(bytes_received, buffer_rx);
		if (res == true) {
			break;
		}
		counter++;
	}

	closesocket(socketS);

#endif // SENDER
	return 0;
}