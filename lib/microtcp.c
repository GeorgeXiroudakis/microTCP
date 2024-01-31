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

int
check_resived_checksum(message_t message){
    uint32_t revivedChecksum = message.header.checksum;

    //zero out the cheksum in the message as that's how the sender calculated
    message.header.checksum = 0;
    if(revivedChecksum != crc32((const uint8_t*) &message, sizeof(message) + message.header.data_len)){
        return -1;
    }

    return 0;
}

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
    //message.payload = NULL;

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
    socket->recvbuf = malloc(socket->init_win_size);

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
        //message.payload = NULL;
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
        //message.payload = NULL;
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

//  commented out as this first fin + ack will be resived by the resv of the server

//    //now we wait for the FIN + ACK
//    if( recvfrom(socket->sd, &resbuff, socket->curr_win_size, 0, &resaddress, &resaddressLen) == -1){
//        return -1;
//    }
//    memcpy(&message, resbuff, sizeof(message_t));
//
//    //check that we revived the message correctly
//    if (check_resived_checksum(message))return -1;
//
//    //check if we revived a header with only a ack in the control
//    if ((message.header.control & FIN_FLAG | ACK_FLAG) != (FIN_FLAG | ACK_FLAG) )return -1;
//
//    //check the ACK
//    if (message.header.ack_number != socket->seq_number) return -1;
//
//#ifdef DEBUGPRINTS
//    printf("resiveed FIN + ACK  with seq# = %d, ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
//#endif
//
//    //save the seq# we got from the client
//    socket->ack_number = message.header.seq_number + 1;

    if(socket->state == CLOSING_BY_PEER) {

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
            //message.payload = NULL;
            message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

            if (sendto(socket->sd, &message, sizeof(message), 0, &(socket->peerAdress), socket->peerAdressLen) == -1) {
                return -1;
            }
        #ifdef DEBUGPRINTS
            printf("sent ACK  with seq# = %d, ack# = %d\n\n", message.header.seq_number, message.header.ack_number);
        #endif
            socket->seq_number++;

            //socket->state = CLOSING_BY_PEER;
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
            //message.payload = NULL;
            message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

            if (sendto(socket->sd, &message, sizeof(message), 0, &(socket->peerAdress), socket->peerAdressLen) == -1) {
                return -1;
            }
        #ifdef DEBUGPRINTS
            printf("sent FIN + ACK  with seq# = %d, ack# =  %d\n\n", message.header.seq_number, message.header.ack_number);
        #endif
            socket->seq_number++;

            //now we wait for the ACK
            if (recvfrom(socket->sd, &resbuff, socket->curr_win_size, 0, &resaddress, &resaddressLen) == -1) {
                return -1;
            }
            memcpy(&message, resbuff, sizeof(message_t));

            //check that we revived the message correctly
            if (check_resived_checksum(message))return -1;

            //check if we revived a header with only a ack in the control
            if ((message.header.control & ACK_FLAG) != (ACK_FLAG))return -1;

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

    }else{
        fprintf(stderr, "clinet has not requested shutdown");
    }

    return 0;
}

ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
    size_t remaining = length;
    ssize_t data_sent = 0;
    ssize_t bytes_to_send;
    size_t chunks;
    size_t flow_ctrl_win = socket->init_win_size;
    size_t extraChunkSize;
    size_t extraPayload  = 0;
    size_t maxPayload = 0;

    message_t message;

    size_t i = 0;

    int tripleACKCounter = 0;
    int retransmission_flag = 0;
    socket->comgestion_state = slow_start;
    socket->cwnd = 1;
    socket->ssthresh = MICROTCP_WIN_SIZE;


    while(data_sent < length){
        bytes_to_send = min( remaining , flow_ctrl_win , socket->cwnd * MICROTCP_MSS);
        printf("\n\n\nthe winer is %zu\n\n\n", bytes_to_send);
        chunks = bytes_to_send / MICROTCP_MSS;
        extraChunkSize = bytes_to_send % MICROTCP_MSS;
        if(chunks == 0){
            perror("error with chunk size cant be zero");
            return -1;
        }
        maxPayload = ((bytes_to_send - extraChunkSize)/chunks) - sizeof(message.header);
        tripleACKCounter = 0;
        size_t excpectedACK[chunks + 1];

#ifdef DEBUGPRINTS
        printf("\nchungs = %zu\n", chunks);
        printf("\nmaxPayload = %zu\n", maxPayload);
        printf("\nextraChunkSize = %zu\n", extraChunkSize);
#endif

        for(i = 0; i < chunks; i++){

            retransmission_flag = 0;

#ifdef DEBUGPRINTS
            printf("Sending %zu chunk\n",i);
#endif
            message.header.seq_number = socket->seq_number;
            message.header.ack_number = socket->ack_number;
            message.header.control = 0;
            message.header.window = 0;
            message.header.data_len = maxPayload ;
            message.header.future_use0 = 0;
            message.header.future_use1 = 0;
            message.header.future_use2 = 0;
            message.header.checksum = 0;
            memcpy(message.payload, buffer + i * maxPayload ,  message.header.data_len);
            message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));
            printf("\ncheckum = %u\n", message.header.checksum);
            extraPayload += sizeof(microtcp_header_t);

            if(sendto(socket->sd, &message, (sizeof(message.header) + message.header.data_len), 0, &(socket->peerAdress), socket->peerAdressLen)  == -1){
                perror("error in sentTo in send\n");
            }
            socket->seq_number += message.header.data_len;
            excpectedACK[i] = socket->seq_number;

#ifdef DEBUGPRINTS
            for(size_t j = 0;j < message.header.data_len;j++) {
                printf("%c", ((char *) message.payload)[j]);
            }
            printf("\nChunk %zu sent\n",i);
#endif
        }


        if( bytes_to_send % MICROTCP_MSS){
            chunks++;

#ifdef DEBUGPRINTS
            printf("\nextraPayload %lu\n", extraPayload);
#endif
            message.header.seq_number = socket->seq_number;
            message.header.ack_number = socket->ack_number;
            message.header.data_len = extraChunkSize + extraPayload;
            message.header.checksum = 0;
            memcpy(message.payload, buffer + (i * maxPayload), message.header.data_len);
            message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

#ifdef DEBUGPRINTS
            printf("Sending extra %zu chunk\n",i);
            for(size_t j = 0;j < message.header.data_len;j++) {
                printf("%c", ((char *) message.payload)[j]);
            }
#endif

            if(sendto(socket->sd, &message, sizeof(message.header) + message.header.data_len, 0, &(socket->peerAdress), socket->peerAdressLen)  == -1){
                perror("error in sentTo in send\n");
            }
            socket->seq_number += message.header.data_len;
            excpectedACK[chunks - 1] = socket->seq_number;

#ifdef DEBUGPRINTS
            printf("\nChunk %zu sent\n",i);
#endif
        }

        //resive asks
        void* resivebuff[MICROTCP_RECVBUF_LEN];
        message_t ackMesege;

        for(i = 0; i < chunks; i++){
            struct timeval timeout;
            timeout. tv_sec = 0;
            timeout. tv_usec = MICROTCP_ACK_TIMEOUT_US;
            if ( setsockopt( socket->sd , SOL_SOCKET, SO_RCVTIMEO , & timeout , sizeof( struct timeval)) < 0) {
                perror(" error in setsockopt\n");
            }

            ssize_t bytesReceived = recvfrom(socket->sd, &resivebuff, sizeof (message_t), 0, &socket->peerAdress, &socket->peerAdressLen);

            if (bytesReceived < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    printf("Receive timeout occurred\n");

                    if(socket->comgestion_state == slow_start){
                        socket->ssthresh = socket->cwnd/2;
                        socket->cwnd = 1;
                        tripleACKCounter = 0;
                    }else if(socket->comgestion_state == congestion_avoidance){
#ifdef DEBUGPRINTS
                        printf("\nFrom congestion avoidance to slow start\n");
#endif
                        socket->comgestion_state = slow_start;
                        socket->ssthresh = socket->cwnd/2;
                        socket->cwnd = 1;
                        tripleACKCounter = 0;
                    }else if(socket->comgestion_state == fast_recovery){
#ifdef DEBUGPRINTS
                        printf("\nFrom fast recovery to slow start\n");
#endif
                        socket->comgestion_state = slow_start;
                        socket->ssthresh = socket->cwnd/2;
                        socket->cwnd = 1;
                        tripleACKCounter = 0;
                    }

                    retransmission_flag = 1;                                            //TODO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                } else {
                    return -1;
                }
            }

            printf("\nReceived ACK %zu\n", i);

            memcpy(&ackMesege, resivebuff, sizeof(message_t));

            if ((ackMesege.header.control & ACK_FLAG) != (ACK_FLAG) )return -1;

            flow_ctrl_win = message.header.window;


            //the clients buffer is full
            while (flow_ctrl_win == 0){
                recvfrom(socket->sd, socket->recvbuf, MICROTCP_RECVBUF_LEN, 0, &socket->peerAdress, &socket->peerAdressLen);
                memcpy(&message,socket->recvbuf,sizeof(microtcp_header_t));

                if ((ackMesege.header.control & ACK_FLAG) != (ACK_FLAG) )continue;
                flow_ctrl_win = message.header.window;
            }


            if (ackMesege.header.ack_number != excpectedACK[i]) {
                tripleACKCounter++;
                if(tripleACKCounter >= 3) {
                    printf("%s AND we got worng header header ack so retrasmit, a header ack: %u != soket seq: %zu \n", socket->isServer ? "i am server" : "i am client", ackMesege.header.ack_number, excpectedACK[i]);

                    if(socket->comgestion_state != fast_recovery){
#ifdef DEBUGPRINTS
                        printf("\nFrom slow start or congestion avoidance to fast recovery\n");
#endif
                        socket->comgestion_state = fast_recovery;
                        socket->ssthresh = socket->cwnd/2;
                        socket->cwnd = socket->ssthresh + 3 * MICROTCP_MSS;
                    }else{
                        socket->cwnd += MICROTCP_MSS;
                    }
                    retransmission_flag = 1;
                }
            }

            //after each succeefull ack add one to the cwd
            if(socket->comgestion_state == slow_start) {
                socket->cwnd += MICROTCP_MSS;
                if(socket->cwnd >= socket->ssthresh){
#ifdef  DEBUGPRINTS
                    printf("\nFrom slow start to congestion avoidance\n");
#endif

                    socket->comgestion_state = congestion_avoidance;
                }
            }else if(socket->comgestion_state == congestion_avoidance){
                socket->cwnd += MICROTCP_MSS + (MICROTCP_MSS/socket->cwnd);
                tripleACKCounter =  0;
            }else if(socket->comgestion_state == fast_recovery){
#ifdef DEBUGPRINTS
                printf("\nFrom fast recovery to congestion avoidance\n");
#endif
                socket->comgestion_state = congestion_avoidance;
                socket->cwnd = socket->ssthresh;
                tripleACKCounter = 0;
            }

            socket->ack_number++;

            if(retransmission_flag == 1){
                //We retransmitt all the chunks from the last good ack we receive
                size_t k = 0;
                for(k = i ; k < chunks; k++){

#ifdef DEBUGPRINTS
                    printf("\nWe start retransmission from chunk %zu\n",k);
#endif

                    retransmission_flag = 0;

#ifdef DEBUGPRINTS
                    printf("Sending %zu chunk\n",i);
#endif
                    message.header.seq_number = socket->seq_number;
                    message.header.ack_number = socket->ack_number;
                    message.header.control = 0;
                    message.header.window = 0;
                    message.header.data_len = maxPayload ;
                    message.header.future_use0 = 0;
                    message.header.future_use1 = 0;
                    message.header.future_use2 = 0;
                    message.header.checksum = 0;
                    memcpy(message.payload, buffer + k * maxPayload ,  message.header.data_len);
                    message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));
                    printf("\ncheckum = %u\n", message.header.checksum);
                    extraPayload += sizeof(microtcp_header_t);

                    if(sendto(socket->sd, &message, (sizeof(message.header) + message.header.data_len), 0, &(socket->peerAdress), socket->peerAdressLen)  == -1){
                        perror("error in sentTo in send\n");
                    }
                    socket->seq_number += message.header.data_len;

#ifdef DEBUGPRINTS
                    for(size_t j = 0;j < message.header.data_len;j++) {
                        printf("%c", ((char *) message.payload)[j]);
                    }
                    printf("\nChunk %zu sent\n",k);
#endif
                }

                //Extra chunk
                if( bytes_to_send % MICROTCP_MSS){

#ifdef DEBUGPRINTS
                    printf("\nextraPayload %lu\n", extraPayload);
#endif
                    message.header.seq_number = socket->seq_number;
                    message.header.ack_number = socket->ack_number;
                    message.header.data_len = extraChunkSize + extraPayload;
                    message.header.checksum = 0;
                    memcpy(message.payload, buffer + (k * maxPayload), message.header.data_len);
                    message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

#ifdef DEBUGPRINTS
                    printf("Sending extra %zu chunk\n",i);
                    for(size_t j = 0;j < message.header.data_len;j++) {
                        printf("%c", ((char *) message.payload)[j]);
                    }
#endif

                    if(sendto(socket->sd, &message, sizeof(message.header) + message.header.data_len, 0, &(socket->peerAdress), socket->peerAdressLen)  == -1){
                        perror("error in sentTo in send\n");
                    }
                    socket->seq_number += message.header.data_len;

#ifdef DEBUGPRINTS
                    printf("\nChunk %zu sent\n",i);
#endif
                }

            }
        }

        remaining -= bytes_to_send;
        data_sent += bytes_to_send;
    }


    message.header.seq_number = socket->seq_number;
    message.header.ack_number = socket->ack_number;
    message.header.control = FIN_FLAG;
    message.header.window = 0;                  //////////////////////////////////////////////////////////////////////////////////////////////
    message.header.data_len = 0;
    message.header.future_use0 = 0;
    message.header.future_use1 = 0;
    message.header.future_use2 = 0;
    message.header.checksum = 0;
    message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

    if (sendto(socket->sd, &message, sizeof(message), 0, &(socket->peerAdress), socket->peerAdressLen) == -1) {
        return -1;
    }
#ifdef DEBUGPRINTS
    printf("sent FIN with seq# = %d, ack# =  %d\n\n", message.header.seq_number, message.header.ack_number);
#endif
    socket->seq_number++;
    printf("sented the fin\n");


    return data_sent;
}

int sentACK(microtcp_sock_t *socket){
    message_t message;

    //sending duplicate ack
    message.header.seq_number = socket->seq_number;
    message.header.ack_number = socket->ack_number;
    message.header.window = socket->curr_win_size;
    message.header.control = ACK_FLAG;
    message.header.window = 0;                  //////////////////////////////////////////////////////////////////////////////////////////////
    message.header.data_len = 0;
    message.header.future_use0 = 0;
    message.header.future_use1 = 0;
    message.header.future_use2 = 0;
    message.header.checksum = 0;
    message.header.checksum = crc32((const uint8_t *) &message, sizeof(message));

    if (sendto(socket->sd, &message, sizeof(message), 0, &(socket->peerAdress), socket->peerAdressLen) == -1) {
        return -1;
    }
#ifdef DEBUGPRINTS
    printf("sent ACK with seq# = %d, ack# =  %d\n\n", message.header.seq_number, message.header.ack_number);
#endif
    socket->seq_number++;

    return 0;
}


ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
    message_t message;
    int ack = 0;
    size_t remainingSizeOfBuff = socket->init_win_size;


    struct sockaddr resaddress;
    socklen_t resaddressLen;
    ssize_t ToatalDataReseved = 0;
    size_t fillLevelOfBuffer = 0;

    //reset the flow window
    socket->curr_win_size = remainingSizeOfBuff;

    while(remainingSizeOfBuff > 0 ){

        //if we dont receive data it means that the server is done sending data so we return
        struct timeval timeout;
        timeout. tv_sec = 0;
        timeout. tv_usec = MICROTCP_ACK_TIMEOUT_US;
        if ( setsockopt( socket->sd , SOL_SOCKET, SO_RCVTIMEO , & timeout , sizeof( struct timeval)) < 0) {
            perror(" error in setsockopt\n");
        }

        //we resive data
        if (recvfrom(socket->sd, socket->recvbuf, socket->curr_win_size, 0, &resaddress, &resaddressLen) < 0) {
            printf("No more data to resive right know\n");
            return 0;
        }

        memcpy(&message.header, socket->recvbuf, sizeof(message.header));
        memcpy(&message.payload, socket->recvbuf + sizeof(message.header), message.header.data_len);


        printf("\ncheckum = %u\n", message.header.checksum);
        //check that we revived the message correctly
        if (check_resived_checksum(message)){                //TODO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                //send duplicate ack
            printf("\n\nFAIL\n\n");
                if(sentACK(socket) == -1)return 0;
            }

//        //check the seq
//        if (message.header.ack_number != socket->seq_number) {
//            printf(" i am %s AND header ack: %u != soket seq: %zu ", socket->isServer ? "i am server" : "i am client", message.header.ack_number, socket->seq_number);
//            return -1;
//        }
//
//        if (message.header.seq_number != socket->ack_number) {
//            printf("i am %s AND header seq: %u != soket ack: %zu ", socket->isServer ? "i am server" : "i am client", message.header.ack_number, socket->seq_number);
//            printf("sent duplicate ACK");
//            if(sentACK(socket) == -1)return -1;
//        }

        //check if client wants to close the connection
        if ((message.header.control & FIN_FLAG | ACK_FLAG) == (FIN_FLAG | ACK_FLAG) && socket->isServer) {
            //save the seq# we got from the client
            socket->ack_number++; //= message.header.seq_number;

            //change the socket state
            socket->state = CLOSING_BY_PEER;

#ifdef DEBUGPRINTS
            printf("resiveed FIN + ACK with seq# = %d, ack# = %d\n\n", message.header.seq_number,
                   message.header.ack_number);
#endif
            printf("returning correctly\n");
            return 0;
        }

        //check if server is done sending data
        if ((message.header.control & FIN_FLAG) == FIN_FLAG) {
            socket->ack_number++;
            return 0;
        }

        //save the seq# we got from the client
        socket->ack_number += message.header.data_len;


        //pass the data to the user
        if(message.header.data_len <= remainingSizeOfBuff){
            memcpy(buffer + fillLevelOfBuffer, message.payload, message.header.data_len);
            //adjust the total data
            ToatalDataReseved += message.header.data_len;
            //if the if was unsuccessful the wille will fail
            remainingSizeOfBuff -= message.header.data_len;
            socket->curr_win_size = remainingSizeOfBuff;

#ifdef DEBUGPRINTS
            printf("resived: ");
            for (size_t i = 0; i < message.header.data_len; i++) {
                printf("%c", (message.payload)[i]);
            }

            printf("\nsenting ACK\n");
#endif
        }else{
            remainingSizeOfBuff = 0;
            socket->curr_win_size = 0;


            //sentACK(socket);
            //sent headers with win = 0
        }

        //sent ACK
        printf("Sending  %d ack to server\n", ack);
        sentACK(socket);
        ack++;
    }

    return ToatalDataReseved;

}

