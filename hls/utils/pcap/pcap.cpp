/**
 * @file pcap.c
 * @brief Own implementation of the libpcap (just necessary functions).
 * @author José Fernando Zazo Rollón, josefernando.zazo@estudiante.uam.es
 * @author Mario Daniel Ruiz Noguera, mario.ruiz@uam.es
 * @date 2013-07-25
 */

#include "pcap.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

static int         file_read           = 0;    /**< File descriptor of the pcap file to read*/
static FILE       *file_write          = NULL; /**< File descriptor of the pcap file to write */
static uint64_t    file_proc_size_read = 0;
static void       *memfile;
static struct stat sr;
static struct stat sw;

static bool read_ethernet = true;

struct pcap_file {
  FILE       *pcap_file;
  bool        read_only;
  bool        write_only;
  uint64_t    file_proc_size_read;
  void       *memfile;
  struct stat pcap_file_stat;
  bool        remove_pcap_ethernet_header;
  bool        insert_stream_ethernet_header;
};

int PcapOpen(char *path, bool remove_ethernet_header) {
  if (file_read) { /* Ensure just one file is opened. */
    return -1;
  }

  file_read = open(path, O_RDONLY);
  fstat(file_read, &sr);
#ifdef _WIN32
  memfile = mmap(NULL, sr.st_size, NULL, MAP_PRIVATE, file_read, 0);
#else
  memfile = mmap(NULL, sr.st_size, PROT_WRITE, MAP_PRIVATE, file_read, 0);
#endif
  if (memfile == MAP_FAILED) {
    printf("Error in mmap %d\n", memfile);
    perror("Error in mmap");
    return -1;
  }
  read_ethernet = !remove_ethernet_header;

  file_proc_size_read = 0;

  return 0;
}

void PcapClose() {
  file_read = 0;  // Releasing locking to read new file
  munmap(memfile, sr.st_size);
  close(file_read);
}

static int pcap_eof() {
  if (file_proc_size_read >= sr.st_size) {
    return 1;
  } else {
    return 0;
  }
}

/**
 * @brief Read global header of a pcap file.
 *
 * @return Always 0.
 */
static int readHeader() {
  /* Just ignore it */
  file_proc_size_read += sizeof(pcap_hdr_t);
  return 0;
}

/**
 * @brief Extract a packet in a pcap file.
 *
 * @param data Region where the data will be stored.
 * @param hdr Individual header of the packet (extract from pcap)
 * pointing to this region.
 *
 * @return A value less or equal to 0 is there was an error.
 */
static int readPacket(uint8_t **data, struct pcap_pkthdr *hdr) {
  pcaprec_hdr_t localheader;

  if (file_proc_size_read + sizeof(pcaprec_hdr_t) >= sr.st_size)
    return 0;

  localheader = *((pcaprec_hdr_t *)((uint8_t *)memfile + file_proc_size_read));
  file_proc_size_read += sizeof(pcaprec_hdr_t);

  hdr->ts.tv_usec = localheader.ts_usec;
  hdr->ts.tv_sec  = localheader.ts_sec;

  if (!read_ethernet) {  // skip the ethernet header
    hdr->len = localheader.orig_len - 14;
    *data    = (uint8_t *)memfile + file_proc_size_read + 14;
  } else {
    hdr->len = localheader.orig_len;
    *data    = (uint8_t *)memfile + file_proc_size_read;
  }

  file_proc_size_read += localheader.incl_len;

  return 1;
}

/*
PcapLoop()  processes  packets  from  a ‘‘savefile’’ until cnt packets are
processed, the end of the ‘‘save- file’’ is reached when reading from a
‘‘savefile’’, pcap_breakloop() is called, or an error occurs.  It does not
return  when live  read timeouts occur.  A value of -1 or 0 for cnt is
equivalent to infinity, so that packets are processed until another ending
condition occurs.
*/
int PcapLoop(int cnt, pcap_handler callback, unsigned char *user) {
  uint32_t           i;
  struct pcap_pkthdr hdr;
  uint8_t           *data;

  readHeader();
  if (cnt == 0) {
    for (cnt = 0; !pcap_eof(); cnt++) {
      if (readPacket(&data, &hdr) <= 0) {
        break;
      }

      callback(user, &hdr, data);
    }
  } else if (cnt > 0) {
    for (i = 0; i < cnt && !pcap_eof(); i++) {
      if (readPacket(&data, &hdr) <= 0) {
        break;
      }

      callback(user, &hdr, data);
    }
  } else {
    return -1;
  }

  return cnt;
}

void pcap_WriteHeader(bool microseconds) {
  pcap_hdr_t global_header;

  if (microseconds) {
    global_header.magic_number = 0xa1b2c3d4;
  } else {
    global_header.magic_number = 0xa1b23c4d;
  }

  global_header.version_major = 2;
  global_header.version_minor = 4;
  global_header.thiszone      = 0;
  global_header.sigfigs       = 0;
  global_header.snaplen       = 65535;
  global_header.network       = 1;

  fwrite(&global_header, sizeof(pcap_hdr_t), 1, file_write);
}

void PcapWriteData(uint8_t *data, int data_size) {
  pcaprec_hdr_t         local_header;
  static bool           first_call = true;
  static struct timeval tv;

  if (first_call) {
    gettimeofday(&tv, NULL);
    first_call = false;
  }

  local_header.ts_sec   = (uint32_t)tv.tv_sec;
  local_header.ts_usec  = 0;
  local_header.incl_len = data_size;
  local_header.orig_len = data_size;

  fwrite(&local_header, sizeof(pcaprec_hdr_t), 1, file_write);

  for (int i = 0; i < data_size; i++) {
    fwrite(&data[i], 1, 1, file_write);
  }
}

int PcapOpenWrite(const char *path, bool microseconds) {
  if (file_write != NULL) { /* Ensure just one file is opened. */
    perror("An output file is already open\n");
    return -1;
  }

  if ((file_write = fopen(path, "wb")) == NULL) {
    perror("POW: Error opening output file\n");
    return -1;
  }

  pcap_WriteHeader(microseconds);

  return 0;
}

void PcapCloseWrite() {
  munmap(memfile, sw.st_size);
  fclose(file_write);
  file_write = NULL;
}
