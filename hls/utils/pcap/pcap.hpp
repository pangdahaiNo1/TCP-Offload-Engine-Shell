/**
 * @file pcap.hpp
 * @brief Own library inspiry in libpcap (just necessary functions).
 * @author José Fernando Zazo Rollón, josefernando.zazo@estudiante.uam.es
 * @author Mario Daniel Ruiz Noguera, mario.ruiz@uam.es
 * @date 2013-07-25
 */
#ifndef NFP_PCAP_H
#define NFP_PCAP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define PAGE_SIZE 4096

/*
Pcap file format:


++---------------++---------------+-------------++---------------+-------------++---------------+-------------++
|| Global Header || Packet Header | Packet Data || Packet Header | Packet Data
|| Packet Header | Packet Data ||
++---------------++---------------+-------------++---------------+-------------++---------------+-------------++

*/

/**
 * @brief Global pcap header.
 */
typedef struct pcap_hdr_s {
  uint32_t magic_number;  /**< magic number */
  uint16_t version_major; /**< major version number */
  uint16_t version_minor; /**< minor version number */
  uint32_t thiszone;      /**< GMT to local correction */
  uint32_t sigfigs;       /**< accuracy of timestamps */
  uint32_t snaplen;       /**< max length of captured packets, in octets */
  uint32_t network;       /**< data link type */
} pcap_hdr_t;

/**
 * @brief Pcap packet header.
 */
typedef struct pcaprec_hdr_s {
  uint32_t ts_sec;   /**< timestamp seconds */
  uint32_t ts_usec;  /**< timestamp microseconds */
  uint32_t incl_len; /**< number of octets of packet saved in file */
  uint32_t orig_len; /**< actual length of packet */
} pcaprec_hdr_t;

/**
 * @brief  Information from the  pcaprec_hdr_s transferred to the callback
 * function
 */
struct pcap_pkthdr {
  uint32_t       len; /**< Length of the packet */
  struct timeval ts;  /**< Time of arrival of the packet */
};

/**
 * @brief Function prototype of the callback function invoked by
 * PcapLoop/simple_loop.
 *
 * @param user The last argument given to the PcapLoop/simple_loop.
 * @param pkthdr The read fields from the file.
 * @param bytes The pointer to the read data from the file.
 */
typedef void (*pcap_handler)(unsigned char *user, struct pcap_pkthdr *pkthdr, unsigned char *bytes);

/**
 * @brief Open a pcap file in read only mode.
 *
 * @param path Path to the file.
 *
 * @return The FILE* associated.
 */
int PcapOpen(char *path, bool ethernet);

/**
 * @brief Close a previous file opened with PcapOpen.
 *
 * @param descriptor The return value of such functions.
 */
void PcapClose();

/**
 * @brief Function similar to the PcapLoop in libpcap. Iterates over cnt
 * packets in the pcap while invoking the callback function
 *
 * @param cnt Number of packets to process. 0 = All the file.
 * @param callback Function invoked for each packet.
 * @param user Pointer that will be passed to the callback function.
 *
 * @return The number of packets that have been processed.
 */
int PcapLoop(int cnt, pcap_handler callback, unsigned char *user);

/**
 * @brief Open a pcap file in write only mode.
 *
 * @param path Path to the file.
 *
 * @return if was create successful or not.
 */
int PcapOpenWrite(char *path, bool microseconds);

void PcapWriteData(uint8_t *data, int data_size);

/**
 * @brief Close a previous file opened with PcapOpenWrite.
 *
 * @param descriptor The return value of such functions.
 */

void PcapCloseWrite();

#endif
