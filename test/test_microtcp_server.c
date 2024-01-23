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
 * You can use this file to write a test microTCP server.
 * This file is already inserted at the build system.
 */

#include <../lib/microtcp.h>


#define LISTENING_PORT 12322

int
main(int argc, char **argv)
{
    microtcp_sock_t sock;
    struct sockaddr_in sin;
    struct sockaddr_in clients_sin;
    int acceptSock;


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

    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(LISTENING_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);


    if(microtcp_bind(&sock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) == -1){
        perror("ERROR in bind");
        exit(EXIT_FAILURE);
    }

    acceptSock =  microtcp_accept(&sock, (struct sockaddr *) &clients_sin, sizeof (clients_sin));
    if(acceptSock < 0){
        perror("error in micro_TCP_accept");
        exit(EXIT_FAILURE);
    }

#ifdef DEBUGPRINTS
    printf("\nSERVER after handshake seq# = %ld, ack# = %ld\n\n", sock.seq_number, sock.ack_number);
#endif

    //make message to send
    char *messageToSent = malloc(MICROTCP_MSS * 3 + MICROTCP_MSS/2);
    //char messageToSent[] = "In the heart of the bustling city, where skyscrapers touched the clouds and the symphony of car horns played in the background, there existed a hidden oasis. Tucked away between towering buildings and bustling streets, a quaint park emerged like a green jewel amidst the urban chaos. The park, with its winding pathways and vibrant flora, offered a serene retreat for those seeking solace from the frenetic pace of city life. Trees stood tall, their leaves whispering secrets to the wind, while the gentle murmur of a nearby fountain provided a soothing soundtrack. As the sun dipped below the horizon, casting a warm glow on the city skyline, the park transformed into a magical realm, where time seemed to slow down. The air was filled with the scent of blooming flowers, and the soft rustle of leaves created a lullaby that invited visitors to lose themselves in the enchantment of the moment. This hidden gem, though nestled in the midst of urban chaos, served as a reminder that even in the busiest of cities, pockets of tranquility could be found, offering a refuge for those in search of a peaceful escape.In the heart of the bustling city, where skyscrapers touched the clouds and the symphony of car horns played in the background, there existed a hidden oasis. Tucked away between towering buildings and bustling streets, a quaint park emerged like a green jewel amidst the urban chaos. The park, with its winding pathways and vibrant flora, offered a serene retreat for those seeking solace from the frenetic pace of city life. Trees stood tall, their leaves whispering secrets to the wind, while the gentle murmur of a nearby fountain provided a soothing soundtrack. As the sun dipped below the horizon, casting a warm glow on the city skyline, the park transformed into a magical realm, where time seemed to slow down. The air was filled with the scent of blooming flowers, and the soft rustle of leaves created a lullaby that invited visitors to lose themselves in the enchantment of the moment. This hidden gem, though nestled in the midst of urban chaos, served as a reminder that even in the busiest of cities, pockets of tranquility could be found, offering a refuge for those in search of a peaceful escape.In the heart of the bustling city, where skyscrapers touched the clouds and the symphony of car horns played in the background, there existed a hidden oasis. Tucked away between towering buildings and bustling streets, a quaint park emerged like a green jewel amidst the urban chaos. The park, with its winding pathways and vibrant flora, offered a serene retreat for those seeking solace from the frenetic pace of city life. Trees stood tall, their leaves whispering secrets to the wind, while the gentle murmur of a nearby fountain provided a soothing soundtrack. As the sun dipped below the horizon, casting a warm glow on the city skyline, the park transformed into a magical realm, where time seemed to slow down. The air was filled with the scent of blooming flowers, and the soft rustle of leaves created a lullaby that invited visitors to lose themselves in the enchantment of the moment. This hidden gem, though nestled in the midst of urban chaos, served as a reminder that even in the busiest of cities, pockets of tranquility could be found, offering a refuge for those in search of a peaceful escape.In the heart of the bustling city, where skyscrapers touched the clouds and the symphony of car horns played in the background, there existed a hidden oasis. Tucked away between towering buildings and bustling streets, a quaint park emerged like a green jewel amidst the urban chaos. The park, with its winding pathways and vibrant flora, offered a serene retreat for those seeking solace from the frenetic pace of city life. Trees stood tall, their leaves whispering secrets to the wind, while the gentle murmur of a nearby fountain provided a soothing soundtrack. As the sun dipped below the horizon, casting a warm glow on the city skyline, the park transformed into a magical realm, where time seemed to slow down. The air was filled with the scent of blooming flowers, and the soft rustle of leaves created a lullaby that invited visitors to lose themselves in the enchantment of the moment. This hidden gem, though nestled in the midst of urban chaos, served as a reminder that even in the busiest of cities, pockets of tranquility could be found, offering a refuge for those in search of a peaceful escape.In the heart of the bustling city, where skyscrapers touched the clouds and the symphony of car horns played in the background, there existed a hidden oasis. Tucked away between towering buildings and bustling streets, a quaint park emerged like a green jewel amidst the urban chaos. The park, with its winding pathways and vibrant flora, offered a serene retreat for those seeking solace from the frenetic pace of city life. Trees stood tall, their leaves whispering secrets to the wind, while the gentle murmur of a nearby fountain provided a soothing soundtrack. As the sun dipped below the horizon, casting a warm glow on the city skyline, the park transformed into a magical realm, where time seemed to slow down. The air was filled with the scent of blooming flowers, and the soft rustle of leaves created a lullaby that invited visitors to lose themselves in the enchantment of the moment. This hidden gem, though nestled in the midst of urban chaos, served as a reminder that even in the busiest of cities, pockets of tranquility could be found, offering a refuge for those in search of a peaceful escape.In the heart of the bustling city, where skyscrapers touched the clouds and the symphony of car horns played in the background, there existed a hidden oasis. Tucked away between towering buildings and bustling streets, a quaint park emerged like a green jewel amidst the urban chaos. The park, with its winding pathways and vibrant flora, offered a serene retreat for those seeking solace from the frenetic pace of city life. Trees stood tall, their leaves whispering secrets to the wind, while the gentle murmur of a nearby fountain provided a soothing soundtrack. As the sun dipped below the horizon, casting a warm glow on the city skyline, the park transformed into a magical realm, where time seemed to slow down. The air in the d";
    for (int i = 0; i <= (MICROTCP_MSS * 3 + MICROTCP_MSS / 2) - 1; i++) {
       /* if(i % MICROTCP_MSS == 0) messageToSent[i] = '!';
        else*/ messageToSent[i] = 'A' + (i % 26);
        printf("%c", messageToSent[i]);
    }
    printf("\n");

//    for (int i = 0; i <= (MICROTCP_MSS * 4 + MICROTCP_MSS / 2) - 1; i++) {
//        printf("%c", messageToSent[i]);
//    }



    if(microtcp_send(&sock, messageToSent, MICROTCP_MSS * 3 + MICROTCP_MSS/2, 0) == -1){
        perror("error with send\n");
        close(sock.sd);
        return 0;
    }



    if(microtcp_recv(&sock, &messageToSent, sizeof(messageToSent), 0 ) == -1 && sock.state == CLOSING_BY_PEER){
        if(microtcp_shutdown(&sock, 0) == -1){
            perror("error in shutdown");
        }
        return 0;
    }


// now the sutdown prosses will begin from the recv
//    if(microtcp_shutdown(&sock, 1) == -1){
//        perror("error in micoro_TCP_accept");
//        exit(EXIT_FAILURE);
//    }


    close(sock.sd);
    return 0;
}
