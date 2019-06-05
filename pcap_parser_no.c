#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
  // Parameter parsing
  if (argc != 2) {
    fprintf(stderr, "Usage: %s namefile\n", argv[0]);
    exit(1);
  }

  FILE *file;

  unsigned int ts_sec;    //timestamp seconds;
  unsigned int ts_usec;   //timestamp microseconds;
  unsigned int incl_len;  //number of bytes of packet data actually captured and saved;
  unsigned int orig_len;  //actual length of packet;
  unsigned int ts_sec_first;
  unsigned int ts_usec_first;
  int diff_sec;
  int diff_usec;
  float et_millis;

  unsigned char d_mac[6]; //destination mac;
  unsigned char s_mac[6]; //source mac;
  unsigned char ulp_bytes[2];
  unsigned int ulp;       //upper layer protocol;

  //IPV4
  unsigned char header_length_ipv4;  //header length in bytes;
  unsigned char dfs;            //differentiated services field;
  unsigned int length_ipv4;          //total length of the packet;
  unsigned char len_ipv4_bytes[2];
  unsigned int identification;  //name of the packet;
  unsigned char identification_bytes[2];
  unsigned char flags[2];       //flags;
  unsigned char ttl;            //time to live;
  unsigned char protocol;       //protocol;
  unsigned int checksum;        //checksum;
  unsigned char check_bytes[2];
  unsigned char s_addr[4];      //source address;
  unsigned char d_addr[4];      //destination address;
  unsigned char protocol_version;
  unsigned int extra_length_ipv4;

  //tcp frame
  unsigned int s_port_tcp;          //source port;
  unsigned char s_port_tcp_bytes[2];
  unsigned int d_port_tcp;          //destination port;
  unsigned char d_port_tcp_bytes[2];
  unsigned long int seq_num;        //sequence number;    
  unsigned char seq_num_bytes[4];
  unsigned long int ack_num;        //acknowledgment number;
  unsigned char ack_num_bytes[4];
  unsigned char header_length_tcp;
  unsigned char flags_tcp[2];
  unsigned int window_size;         //window size value;
  unsigned char win_size_bytes[2];
  unsigned int checksum_tcp;
  unsigned char check_tcp_bytes[2];
  unsigned int urgent_pointer;
  unsigned char urg_pointer_bytes[2];
  unsigned int payload_size;
  unsigned int payload_length;
  unsigned int extra_length_tcp;

  //udp frame
  unsigned int s_port_udp;          //source port;
  unsigned char s_port_udp_bytes[2];
  unsigned int d_port_udp;          //destination port;
  unsigned char d_port_udp_bytes[2];
  unsigned int length_udp;
  unsigned char length_udp_bytes[2];
  unsigned int checksum_udp;
  unsigned char check_udp_bytes[2];
  
  if((file = fopen(argv[1], "r")) == NULL) {
    fprintf(stderr, "Could not open file \n");
    exit(1);
  }
  fseek(file, 24, SEEK_SET);
  int packet = 1;

  while(!feof(file) && !ferror(file)) {
    if (feof(file)) {
      continue;
    }
    /*
    if (fread(&ts_sec, sizeof(unsigned int), 1, file) != 1) {
      if (packet == 1){
        fprintf(stderr, "Error 1\n");
        exit(1);
      } else {
        break;
      }
    }*/
    if (fread(&ts_sec, sizeof(unsigned int), 1, file) != 1) {
      // fprintf(stderr, "Error 1\n");
      // exit(1); 
      continue;
    }

    if (fread(&ts_usec, sizeof(unsigned int), 1, file) != 1) {
      fprintf(stderr, "Error 2\n");
      exit(1);
    }
    if (fread(&incl_len, sizeof(unsigned int), 1, file) != 1) {
      fprintf(stderr, "Error 3\n");
      exit(1);
    }
    if (fread(&orig_len, sizeof(unsigned int), 1, file) != 1) {
      fprintf(stderr, "Error 4\n");
      exit(1);
    }
    if (packet == (unsigned int) 1) {
      ts_sec_first = ts_sec;
      ts_usec_first = ts_usec;
    }
    fread(&d_mac, sizeof(unsigned char), 6, file);
   
    fread(&s_mac, sizeof(unsigned char), 6, file);
 
    fread(&ulp_bytes, sizeof(unsigned char), 2, file);
    ulp = (ulp_bytes[0] << 8) + ulp_bytes[1];
    printf("ulp: %d", ulp);
    printf("next c'è if");
  
    if (ulp != 0x0800) {
      //It is not an IPV4 packet
      //skip the packet
      printf("Entra in no IPV4");
      fseek(file, incl_len-14, SEEK_CUR);
    }

    fread(&protocol_version, sizeof(unsigned char), 1, file);
    if ((protocol_version >> 4) != 4) {
      //Not protocol version 4
      //skip the packet
     
      fseek(file, incl_len-15, SEEK_CUR);
    }
    header_length_ipv4 = (protocol_version & 15) << 2;
    
    fread(&dfs, sizeof(unsigned char), 1, file);
    
    fread(&len_ipv4_bytes , sizeof(unsigned char), 2, file);
    length_ipv4 = (len_ipv4_bytes[0] << 8) + len_ipv4_bytes[1];

    fread(&identification_bytes, sizeof(unsigned char), 2, file);
    identification = (identification_bytes[0] << 8) + identification_bytes[1];

    fread(&flags, sizeof(unsigned char), 2, file);

    fread(&ttl, sizeof(unsigned char), 1, file);

    fread(&protocol, sizeof(unsigned char), 1, file);

    fread(&check_bytes, sizeof(unsigned char), 2, file);
    checksum = (check_bytes[0] << 8) + check_bytes[1];

    fread(&d_addr, sizeof(unsigned char), 4, file);
    fread(&s_addr, sizeof(unsigned char), 4, file);

    if (header_length_ipv4 > (5 << 2)) {
      extra_length_ipv4 = header_length_ipv4 - (5<<2);
      fseek(file, extra_length_ipv4, SEEK_CUR);
    }
    if (protocol == 6) {
      // tcp frame
      payload_length = length_ipv4 - header_length_ipv4;

      fread(&s_port_tcp_bytes, sizeof(unsigned char), 2, file);
      s_port_tcp = (s_port_tcp_bytes[0] << 8) + s_port_tcp_bytes[1];

      fread(&d_port_tcp_bytes, sizeof(unsigned char), 2, file);
      d_port_tcp = (d_port_tcp_bytes[0] << 8) + d_port_tcp_bytes[1];

      fread(&seq_num_bytes, sizeof(unsigned char), 4, file);
      seq_num = (seq_num_bytes[0] << 24) + (seq_num_bytes[1] << 16) + (seq_num_bytes[2] << 8) + seq_num_bytes[3];

      fread(&ack_num_bytes, sizeof(unsigned char), 4, file);
      ack_num = (ack_num_bytes[0] << 24) + (ack_num_bytes[1] << 16) + (ack_num_bytes[2] << 8) + ack_num_bytes[3];
      
      fread(&flags_tcp, sizeof(unsigned char), 2, file);
      header_length_tcp = (flags_tcp[0] >> 4)*4;

      fread(&win_size_bytes, sizeof(unsigned char), 2, file);
      window_size = (win_size_bytes[0] << 8) + win_size_bytes[1];

      fread(&check_tcp_bytes, sizeof(unsigned char), 2, file);
      checksum_tcp = (check_tcp_bytes[0] << 8) + check_tcp_bytes[1];

      fread(&urg_pointer_bytes, sizeof(unsigned char), 2, file);
      urgent_pointer = (urg_pointer_bytes[0] << 8) + urg_pointer_bytes[1];

      if(header_length_tcp > 20) {
        extra_length_tcp = header_length_tcp - 20;
        fseek(file, extra_length_tcp, SEEK_CUR);
      }

      payload_size = payload_length - header_length_tcp;

      fseek(file, payload_size, SEEK_CUR);

    } else if (protocol == 17) {
      // udp frame
      fread(&s_port_udp_bytes, sizeof(unsigned char), 2, file);
      s_port_udp = (s_port_udp_bytes[0] << 8) + s_port_udp_bytes[1];

      fread(&d_port_udp_bytes, sizeof(unsigned char), 2, file);
      d_port_udp = (d_port_udp_bytes[0] << 8) + d_port_udp_bytes[1];

      fread(&length_udp_bytes, sizeof(unsigned char), 2, file);
      length_udp = (length_udp_bytes[0] << 8) + length_udp_bytes[1];

      fread(&check_udp_bytes, sizeof(unsigned char), 2, file);
      checksum_udp = (check_udp_bytes[0] << 8) + check_udp_bytes[1];

      fseek(file, length_udp - 8, SEEK_CUR);

    } else {
      // altro
      fseek(file, payload_length, SEEK_CUR);
    }
    
    diff_sec = ts_sec - ts_sec_first;
    diff_usec = ts_usec - ts_usec_first;
    et_millis = ((float) (diff_sec*1000000 + diff_usec)) / ((float) 1000);      printf("Packet number: %d, time: %.2f msecs, size: %d bytes \n", packet, et_millis, orig_len);
    printf("ETHERNET: \nDmac: ");
    for (int i=0; i<6; i++) {
      printf("%02x ", d_mac[i]);
    }
    printf("\n");
    printf("Smac: ");
    for (int i=0; i<6; i++) {
      printf("%02x ", s_mac[i]);
    }
    printf("\n");
    printf("ULP: %d \n", ulp);  
    
    if(ulp == 0x0800) {
      
      printf("IPv4\n");
      printf("Header length: %d\n", header_length_ipv4);
      printf("Total length: %d\n", length_ipv4);
      printf("Id: %d\n", identification);
      printf("Protocol: ");
      switch (protocol) {
        case 6:
          printf("TCP ");
          break;
        case 17:
          printf("UDP ");
          break;
        default:
          printf("Unknown ");
          break;
      }
      printf("\n");
      printf("TTL: %d\n", ttl);
      printf("CHK: %X \n", checksum);
      printf("SRC: %d.%d.%d.%d \n", s_addr[0], s_addr[1], s_addr[2], s_addr[3]);
      printf("DST: %d.%d.%d.%d\n", d_addr[0], d_addr[1], d_addr[2], d_addr[3]);
      printf("\n");
      
      if (protocol == 6) {
        //print tcp;
        printf("TCP: \n");
        printf("Header length: %d \n", header_length_tcp);
        printf("Source port: %d \n", s_port_tcp);
        printf("Destination port: %d \n", d_port_tcp);
        printf("Sequence number: %ld \n", seq_num);
        printf("Ack number: %ld \n", ack_num);
        printf("Window size: %d \n", window_size);
        printf("Checksum: %d \n", checksum_tcp);
        printf("Urgent pointer: %d \n", urgent_pointer);
        printf("Payload size: %d \n", payload_size);
        if (flags_tcp[1] & (1 << 4)) printf("ACK ");
        if (flags_tcp[1] & (1 << 3)) printf("PSH ");
        if (flags_tcp[1] & (1 << 2)) printf("RST ");
        if (flags_tcp[1] & (1 << 1)) printf("SYN ");
        if (flags_tcp[1] & (1 << 0)) printf("FIN ");
        printf("\n");
      } else if (protocol == 17) {
        printf("UDP: \n");
        printf("Header length: %d \n", 8);
        printf("Total length: %d \n", length_udp);
        printf("Source port: %d \n", s_port_udp);
        printf("Destination port: %d \n", d_port_udp);
        printf("Checksum: %d \n", checksum_udp);

      } else {
        printf("Unknown IPV4 payload \n");
      }

      printf("\n");

    }
    
    packet++;
    
  }

  fclose(file);
  
}