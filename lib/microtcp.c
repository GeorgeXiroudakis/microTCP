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

#include "microtcp.h"
#include "../utils/crc32.h"

microtcp_sock_t
microtcp_socket (int domain, int type, int protocol)
{
    //creating the microTCP sock we are trying to return;
    microtcp_sock_t sock;

    //untill all the inits are successfull state is invalid
    sock.state = INVALID;

    sock.sd = socket(domain, type, protocol);
    if(sock.sd == -1){
        return sock;
    }

    /*Initializing everything else*/
    sock.init_win_size = MICROTCP_WIN_SIZE;
    sock.curr_win_size = MICROTCP_WIN_SIZE;
    sock.recvbuf = malloc(MICROTCP_RECVBUF_LEN);
    //check malloc
    if(sock.recvbuf == NULL){
        sock.sd = -2;
        return sock;
    }
    sock.buf_fill_level = 0;
    sock.cwnd = MICROTCP_INIT_CWND;
    sock.ssthresh = MICROTCP_INIT_SSTHRESH;
    sock.seq_number = 0;
    sock.ack_number = 0;
    sock.packets_send = 0;
    sock.packets_received = 0;
    sock.packets_lost = 0;
    sock.bytes_send = 0;
    sock.bytes_received = 0;
    sock.bytes_lost = 0;
    sock.isServer = 0;

    sock.state = CLOSED;
    return  sock;
}

int
check_resived_checksum(message_t message){
    uint32_t revivedChecksum = message.header.checksum;
    //zero out the cheksum in the message as that's how the sender calculated
    message.header.checksum = 0;
    if(revivedChecksum != crc32((const uint8_t*) &message, sizeof(message))){
        return -1;
    }

    return 0;
}

int
microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address,
               socklen_t address_len)
{
    //if state is invalid we cant bind
    if (socket->state == INVALID) return -1;

    if(bind(socket->sd, address, address_len) == -1){
        return -1;
    }

    socket->state = LISTEN;
    socket->isServer = 1;
    return 0;
}

int
microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address,
                  socklen_t address_len)
{
    //if state is invalid we cant connect
    if (socket->state == INVALID) return -1;

    uint32_t  my_seq_num;

    //we start the 3-way handshake
#ifdef DEBUGPRINTS
    printf("3-way handshke:\n");
#endif

    //create and initialize the header of the message
    microtcp_header_t header;
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
    socket->seq_number = ((uint32_t) rand()) % 10000;
#ifdef DEBUGPRINTS
    printf("\nCLIENT generated seq# = %ld\n\n", socket->seq_number);
#endif
    header.seq_number = socket->seq_number;
    header.ack_number = 0;
    header.control = SYN_FLAG;
    header.window = socket->curr_win_size;
    header.data_len = 0;
    header.future_use0 = 0;
    header.future_use1 = 0;
    header.future_use2 = 0;
    header.checksum = 0;
    //memset(&header.checksum, 0, sizeof(header.checksum));

    //malloc space for resFuff
    socket->recvbuf = malloc(socket->curr_win_size);

    //creating the buf of the containing the message
    message_t message;
    message.header = header;
    message.payload[0] = 0;

    message.header.checksum = crc32((const uint8_t*) &message, sizeof(message));

    //sent the initial request for connection to the server (SYN)
    if(sendto(socket->sd, &message, sizeof(message), 0, address, address_len) == -1){
        return -1;
    }
    socket->seq_number++;
#ifdef DEBUGPRINTS
    printf("sent SYN with seq# = %d\n\n", message.header.seq_number);
#endif


    void *resbuff[MICROTCP_WIN_SIZE];
    //we reseving the message initial message for the request to connect (from the client)
    if( recvfrom(socket->sd, &resbuff, socket->init_win_size, 0, address, &address_len) == -1){
        return -1;
    }

    memcpy(&message, resbuff, sizeof(message_t));
#ifdef DEBUGPRINTS
    printf("resived SYN + ACK with seq# = %d and ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif

    //check that we revived the message correctly
    if(check_resived_checksum(message))return -1;                                  //!!!!maybe state = invalide after exery return -1;

    //check the controll flags
    if( ((message.header.control & (SYN_FLAG | ACK_FLAG)) != (SYN_FLAG | ACK_FLAG)) ) return -1;

    //check the ACK
    if(message.header.ack_number != socket->seq_number) return -1;

    //save the address of the peer we are gona try to handshake will
    memcpy(&(socket->peerAdress), address, sizeof(struct sockaddr));
    socket->peerAdressLen = address_len;

    //save the seq# we got from the client
    socket->ack_number = message.header.seq_number + 1;

    //now we sent the final piece of 3 way handsake with a ACK
    message.header.control = ACK_FLAG;
    message.header.seq_number = socket->seq_number;
    message.header.ack_number = socket->ack_number;

    //get sented win soze
    socket->curr_win_size = message.header.window;

    //we zero the ckecksum and calsulate the knew one
    message.header.checksum = 0;
    message.header.checksum = crc32((const uint8_t*) &message, sizeof(message));

    //sent the ack back to the server
    if( sendto(socket->sd, &message, sizeof(message), 0, address, address_len) == -1){
        return -1;
    }
    socket->seq_number++;
#ifdef DEBUGPRINTS
    printf("sent ACK with seq# = %d and ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
    printf("END 3-way handshke:\n");
#endif

    socket->state = ESTABLISHED;

    return 0;
}

int
microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
    message_t message;
#ifdef DEBUGPRINTS
    printf("3-way handshke:\n\n");
#endif
    void *resbuff[MICROTCP_RECVBUF_LEN];
    //we reseving the message initial message for the request to connect (SYN from the client)

   if( recvfrom(socket->sd, &resbuff, socket->init_win_size, 0, address, &address_len) == -1){
        return -1;
    }

    memcpy(&message, resbuff, sizeof(message_t));
#ifdef DEBUGPRINTS
    printf("resived SYN with seq# = %d\n", message.header.seq_number);
#endif

    //check that we revived the message correctly
    if(check_resived_checksum(message))return -1;

    //check if we revived a header with only a ack in the control
    if ((message.header.control & SYN_FLAG) != SYN_FLAG)return -1;

    //save the address of the peer we are gona try to handshake will
    memcpy(&(socket->peerAdress), address, sizeof(struct sockaddr));
    socket->peerAdressLen = address_len;

    //save the nagosiated winsize
    socket->init_win_size = message.header.window;

    //now we sent the SYN + ACK to accept the connection
    message.header.control = SYN_FLAG | ACK_FLAG;
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
    socket->seq_number = ((uint32_t) rand()) % 10000;
#ifdef DEBUGPRINTS
    printf("\nSERVER generated seq# = %ld\n\n", socket->seq_number);
#endif
    socket->ack_number = message.header.seq_number + 1;
    message.header.seq_number = socket->seq_number;
    message.header.ack_number = socket->ack_number;
    //give the window size
    message.header.window = MICROTCP_RECVBUF_LEN;


    //we zero the ckecksum and calsulate the knew one
    message.header.checksum = 0;
    message.header.checksum = crc32((const uint8_t*) &message, sizeof(message));

    //sent the ack for the sonnection back to the client
    if( sendto(socket->sd, &message, sizeof(message), 0, address, address_len) == -1){
        return -1;
    }
    socket->seq_number++;
#ifdef DEBUGPRINTS
    printf("sent SYN + ACK with seq# = %d and ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif

    //we resive a ack as the final step of the 3-way handshake
    if( recvfrom(socket->sd, &resbuff, socket->curr_win_size, 0, address, &address_len) == -1){
        return -1;
    }
    memcpy(&message, resbuff, sizeof(message_t));


    //check that we revived the message correctly
    if(check_resived_checksum(message))return -1;

    //check if we revived a header with only a ack in the control
    if ((message.header.control & ACK_FLAG) != ACK_FLAG)return -1;

    //check the ACK
    if(message.header.ack_number != socket->seq_number) return -1;

    //save the seq# we got from the client
    socket->ack_number = message.header.seq_number + 1;

#ifdef DEBUGPRINTS
    printf("resived ACK with seq# = %d and ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
    printf("END 3-way handshke:\n");
#endif

    socket->state = ESTABLISHED;

    return 0;
}


int
microtcp_shutdown (microtcp_sock_t *socket, int how) {
    message_t message;
    void *resbuff[MICROTCP_RECVBUF_LEN];
    struct sockaddr resaddress;
    socklen_t resaddressLen;

    /*client side*/
    if(!socket->isServer) {

#ifdef DEBUGPRINTS
        printf("clinet stating shutdown\n\n");
#endif

        //sending FIN + ACK to server
        message.header.seq_number = socket->seq_number;
        message.header.ack_number = socket->ack_number;
        message.header.control = FIN_FLAG | ACK_FLAG;
        message.header.window = socket->curr_win_size;
        message.header.data_len = 0;
        message.header.future_use0 = 0;
        message.header.future_use1 = 0;
        message.header.future_use2 = 0;
        message.header.checksum = 0;
        message.payload[0] = 0;
        message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

        if(sendto(socket->sd, &message, sizeof(message), 0, &(socket->peerAdress), socket->peerAdressLen) == -1){
            return -1;
        }
#ifdef DEBUGPRINTS
        printf("sent FIN + ACK  with seq# = %d, ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif
        socket->seq_number++;

        //now we wait for the ACK of our FIN + ACK
        if( recvfrom(socket->sd, &resbuff, socket->curr_win_size, 0, &resaddress, &resaddressLen) == -1){
            return -1;
        }
        memcpy(&message, resbuff, sizeof(message_t));

        //check that we revived the message correctly
        if (check_resived_checksum(message))return -1;

        //check if we revived a header with only a ack in the control
        if ((message.header.control & ACK_FLAG) != ACK_FLAG)return -1;

        //check the ACK
        if (message.header.ack_number != socket->seq_number) return -1;

#ifdef DEBUGPRINTS
        printf("resiveed ACK with seq# = %d, ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif

        //save the seq# we got from the client
        socket->ack_number = message.header.seq_number + 1;

        //change the socket state
        socket->state = CLOSING_BY_HOST;
#ifdef DEBUGPRINTS
        printf("client sock CLOESED_BY_HOST\n");
#endif

        //now we wait for the FIN + ACK
        if( recvfrom(socket->sd, &resbuff, socket->curr_win_size, 0, &resaddress, &resaddressLen) == -1){
            return -1;
        }
        memcpy(&message, resbuff, sizeof(message_t));

        //check that we revived the message correctly
        if (check_resived_checksum(message))return -1;

        //check if we revived a header with only a ack in the control
        if ((message.header.control & FIN_FLAG | ACK_FLAG) != (FIN_FLAG | ACK_FLAG) )return -1;

        //check the ACK
        if (message.header.ack_number != socket->seq_number) return -1;

#ifdef DEBUGPRINTS
        printf("resiveed FIN + ACK  with seq# = %d and ack = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif

        //save the seq# we got from the client
        socket->ack_number = message.header.seq_number + 1;


        //sending ACK to server
        message.header.seq_number = socket->seq_number;
        message.header.ack_number = socket->ack_number;
        message.header.control = ACK_FLAG;
        message.header.window = socket->curr_win_size;
        message.header.data_len = 0;
        message.header.future_use0 = 0;
        message.header.future_use1 = 0;
        message.header.future_use2 = 0;
        message.header.checksum = 0;
        message.payload[0] = 0;
        message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

        if( sendto(socket->sd, &message, sizeof(message), 0, &(socket->peerAdress), socket->peerAdressLen) == -1){
            return -1;
        }
#ifdef DEBUGPRINTS
        printf("sent ACK  with seq# = %d, ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif
        socket->seq_number++;

        free(socket->recvbuf);

        socket->state = CLOSED;
#ifdef DEBUGPRINTS
        printf("client sock CLOESED\n");
#endif

        return 0;
    }





    /*server side*/

#ifdef DEBUGPRINTS
    printf("srver stating shutdown\n\n");
#endif

    //now we wait for the FIN + ACK
    if( recvfrom(socket->sd, &resbuff, socket->curr_win_size, 0, &resaddress, &resaddressLen) == -1){
        return -1;
    }
    memcpy(&message, resbuff, sizeof(message_t));

    //check that we revived the message correctly
    if (check_resived_checksum(message))return -1;

    //check if we revived a header with only a ack in the control
    if ((message.header.control & FIN_FLAG | ACK_FLAG) != (FIN_FLAG | ACK_FLAG) )return -1;

    //check the ACK
    if (message.header.ack_number != socket->seq_number) return -1;

#ifdef DEBUGPRINTS
    printf("resiveed FIN + ACK  with seq# = %d, ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif

    //save the seq# we got from the client
    socket->ack_number = message.header.seq_number + 1;

    //sending ACK to server
    message.header.seq_number = socket->seq_number;
    message.header.ack_number = socket->ack_number;
    message.header.control = ACK_FLAG;
    message.header.window = socket->curr_win_size;
    message.header.data_len = 0;
    message.header.future_use0 = 0;
    message.header.future_use1 = 0;
    message.header.future_use2 = 0;
    message.header.checksum = 0;
    message.payload[0] = 0;
    message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

    if( sendto(socket->sd, &message, sizeof(message), 0, &(socket->peerAdress), socket->peerAdressLen) == -1){
        return -1;
    }
#ifdef DEBUGPRINTS
    printf("sent ACK  with seq# = %d, ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif
    socket->seq_number++;

    socket->state = CLOSING_BY_PEER;
#ifdef DEBUGPRINTS
    printf("server sock CLOESED_BY_PEER\n");
#endif

    //sending FIN + ACK to server
    message.header.seq_number = socket->seq_number;
    message.header.ack_number = socket->ack_number;
    message.header.control = FIN_FLAG | ACK_FLAG;
    message.header.window = socket->curr_win_size;
    message.header.data_len = 0;
    message.header.future_use0 = 0;
    message.header.future_use1 = 0;
    message.header.future_use2 = 0;
    message.header.checksum = 0;
    message.payload[0] = 0;
    message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

    if( sendto(socket->sd, &message, sizeof(message), 0, &(socket->peerAdress), socket->peerAdressLen) == -1){
        return -1;
    }
#ifdef DEBUGPRINTS
    printf("sent FIN + ACK  with seq# = %d, ack# =  %d\n\n", message.header.seq_number, message.header.ack_number);
#endif
    socket->seq_number++;

    //now we wait for the ACK
    if( recvfrom(socket->sd, &resbuff, socket->curr_win_size, 0, &resaddress, &resaddressLen) == -1){
        return -1;
    }
    memcpy(&message, resbuff, sizeof(message_t));

    //check that we revived the message correctly
    if (check_resived_checksum(message))return -1;

    //check if we revived a header with only a ack in the control
    if ((message.header.control & ACK_FLAG) != (ACK_FLAG) )return -1;

    //check the ACK
    if (message.header.ack_number != socket->seq_number) return -1;

#ifdef DEBUGPRINTS
    printf("resiveed ACK  with seq# = %d, ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
#endif

    //save the seq# we got from the client
    socket->ack_number = message.header.seq_number + 1;

    free(socket->recvbuf);

    socket->state = CLOSED;

#ifdef DEBUGPRINTS
    printf("server sock CLOESED\n\n");
#endif


    return 0;
}

ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
    /* Your code here */
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
    /* Your code here */
}