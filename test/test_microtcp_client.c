/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * You can use this file to write a test microTCP client.
 * This file is already inserted at the build system.
 */

#include <../lib/microtcp.h>


#define SERVER_LISTENING_PORT 12322

int
main(int argc, char **argv)
{
    microtcp_sock_t sock;
    void* resbuff;

    sock = microtcp_socket(AF_INET ,SOCK_DGRAM, 0);
    if(sock.state == INVALID){
        if(sock.sd == -1){
            perror("error in micoro_TCP_socket underline UDP");
            exit(EXIT_FAILURE);
        }else if(sock.sd == -2){
            perror("error in micoro_TCP_socket malloc for revbuff");
            exit(EXIT_FAILURE);
        }
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(SERVER_LISTENING_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);


    if(microtcp_connect(&sock, (struct sockaddr *) &sin, sizeof(sin)) == -1){
        perror("error in micoro_TCP_connect");
        exit(EXIT_FAILURE);
    }
#ifdef DEBUGPRINTS
    printf("\nCLIENT after handshake seq# = %ld, ack# = %ld\n\n", sock.seq_number, sock.ack_number);
#endif

    resbuff = malloc(MICROTCP_RECVBUF_LEN);
    ssize_t res;
    do{
        res = microtcp_recv(&sock, resbuff, MICROTCP_MSS, 0);
    } while (res > 0);
    if(res == -1){
        perror("error in recv\n");
    }


    if(microtcp_shutdown(&sock, 1) == -1){
        perror("error in micoro_TCP_accept");
        exit(EXIT_FAILURE);
    }

    close(sock.sd);
    return 0;
}
