// All the includes 
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <netdb.h>
using namespace std;

const int port = 8024; // Port to connect on
//for setting up select system call
fd_set set; 
int rv;

/*
    I use 1 letter messages to communicate between the client and server, i.e. server just sends 1 letter, and client prints
    the corresponding whole message
*/

// Sets the board to the beginning, called for every new game
void set_board(char game[3][3]){
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
            game[i][j] = ' ';
        }
    }
}

// Drawing the board in its current state
void draw_board(char game[3][3]){
    printf(" %c | %c | %c \n", game[0][0], game[0][1], game[0][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", game[1][0], game[1][1], game[1][2]);
    printf("-----------\n");
    printf(" %c | %c | %c \n", game[2][0], game[2][1], game[2][2]);
}

// The helper functions are the same as the server

// Helper function to write and a read a message
// We don't want to write the verify for error everytime 
int integer_receipt(int socket_file_descriptor){
    int response = 0;
    int valid = read(socket_file_descriptor, &response, sizeof(int));
    if(valid < 0){
        return -1;
    }
    return response;
}

// Helper function to write an error message
void send_error(char *response){
    perror(response);
    pthread_exit(NULL);
}

void message_int(int socket_file_descriptor, int response){
    int valid = write(socket_file_descriptor, &response, sizeof(int));
    if(valid < 0){
        send_error("Error writing a message\n");
    }
}

void message_word(int socket_file_descriptor, char* response){
    int valid = write(socket_file_descriptor, response, strlen(response));
    if(valid < 0){
        send_error("Error writing a message\n");
    }
}


void message_receipt(int socket_file_descriptor, char* response){
    memset(response, 0, sizeof(response));
    int valid = read(socket_file_descriptor, response, 100);
    if(valid < 0){
        send_error("Error receiving a message");
    }
}

// Handles the basic validity of the moves
int moveHandler(int sockfd)
{
    char move_string[10];
    int move = -1;    
    while (1) {
        // Here lies the complete functionality for the timeout using select system call
        FD_ZERO(&set); /* clear the set */
        FD_SET(0, &set); /* add our file descriptor to the set */
        struct timeval tout;
        tout.tv_sec = 15;
        tout.tv_usec = 0;
        rv = select(1, &set, NULL, NULL, &tout); 
        if(rv <= 0){
            // Timeout was achieved, hence we send garbage message response for server to know this
            message_int(sockfd, -100);
            break;
        }else{
            // Timeout was not exceeded, hence we register and send the move
            fgets(move_string, 10, stdin);
            int row = move_string[0] - '0';
            int col = move_string[2] - '0';
            move = (row - 1) * 3 + (col - 1);
            if (row <= 3 && row >= 1 && col <= 3 && col >= 1){
                message_int(sockfd, move);
                break;
            } 
            else
                printf("\nInvalid input. Try again.\nPlease enter row and column to add your mark:\n");
        }
    }
    return move;
}

int main(int argc, char *argv[]){
    // Require port for proper connection
    if(argc > 3){
        printf("Please provide IP address and/or no extra arguments\n");
        exit(-1);
    }

    // Connecting to the server
    struct sockaddr_in connection;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        printf("Socket error\n");
        exit(-1);
    }
    memset(&connection, 0, sizeof(connection));
    
    // Configuring sockaddr_in
    connection.sin_family = AF_INET;
    connection.sin_port = htons(port); 
    connection.sin_addr.s_addr = inet_addr(argv[1]);

    // Connecting to the server
    if (connect(sockfd, (struct sockaddr *) &connection, sizeof(connection)) < 0){
        printf("Can't connect to server\n");
        exit(-1);
    }

    // Reading the player ID from the server
    int id = integer_receipt(sockfd);
    char response[100]; //We will collect all reponses from server using this


    char game[3][3];
    int inGame = 1;
    int partnerID;

    replay:; // Used to replay back the game

    // First player waiting while the second player joins
    do {
        message_receipt(sockfd, response);
        if (!strncmp(response, "h", 1)){
            inGame = 0;
            printf("Connected to the game server. Your player ID is %d. Waiting for a partner to join . . .\n", id);
        }
    }while(strncmp(response, "x", 1) && strncmp(response, "y", 1));

    // Player O pre-start message
    if(!strncmp(response, "x", 1)){
        partnerID = integer_receipt(sockfd);
        printf("Your partners ID is %d. Your symbol is O.\n", partnerID);
    }

    // Player X pre-start message
    if(!strncmp(response, "y", 1)){
        partnerID = integer_receipt(sockfd);
        printf("Connected to the game server. Your player ID is %d. Your partners ID is %d. Your symbol is X\n", id, partnerID);
    }

    // Waiting for the start of the game
    do{
        message_receipt(sockfd, response);
    }while(strncmp(response, "s", 1));

    printf("Starting the game…\n");
    set_board(game);

    bool replayable = false; // Checks if the result allows for a replay
    draw_board(game);
    // Game starts, we try to read all possible responses from the server
    while(1){
        message_receipt(sockfd, response);

        // First time asking to play the move or the last move tried to play was invalid
        if(!strncmp(response, "m", 1) || !strncmp(response, "i", 1)){
            if(!strncmp(response, "i", 1)) printf("Invalid entry.\n");
            printf("Please enter row and column to add your mark: \n");
            int mv = moveHandler(sockfd);
        }else if(!strncmp(response, "u", 1)){ // Board updation
            int mv = integer_receipt(sockfd);
            if(mv < 9) game[mv/3][mv%3] = (inGame ? 'O' : 'X');
            else{
                mv -= 9;
                game[mv/3][mv%3] = (inGame ? 'X' : 'O');
            }
            draw_board(game);
        }else if(!strncmp(response, "w", 1)){ // Waiting for other player
            printf("Waiting for other players move...\n");
        }else if(!strncmp(response, "o", 1)){ // Win message
            printf("You won\n");
            replayable = true;
            break;
        }else if(!strncmp(response, "l", 1)){ // Loss message
            printf("You lost\n");
            replayable = true;
            break;
        }else if(!strncmp(response, "e", 1)){ // Draw message
            printf("You drew\n");
            replayable = true;
            break;
        }else if(!strncmp(response, "k", 1)){ // Player took too long to make a move message
            printf("You took too long to make move. Game over!\n");
            replayable = true;
            break;
        }else if(!strncmp(response, "c", 1)){ // Opponent took too long to make a move message
            printf("Your opponent took too long to make a move. Game over!\n");
            replayable = true;
            break;
        }else{ // Represents disconnection message
            printf("Sorry, your partner disconnected or system failed\n");
            break;
        }
    }

    // Checking for reply, if both say yes, take the client back to the start of the game
    if(replayable){
        message_receipt(sockfd, response);
        sleep(0.2);
        if(!strncmp(response, "r", 1)) {
            printf("Type y if you want to replay with the current partner\n");
            // Here lies the complete functionality for the timeout using select system call
            FD_ZERO(&set); /* clear the set */
            FD_SET(0, &set); /* add our file descriptor to the set */
            struct timeval tout;
            tout.tv_sec = 15;
            tout.tv_usec = 0;
            rv = select(1, &set, NULL, NULL, &tout); 
            if(rv <= 0){
                // Timeout was acheived
                message_int(sockfd, -100);
            }else{
                // Message was made within time. No issues
                char buff[20];
                fgets(buff,20,stdin);
                if(!strncmp(buff,"y",1)){
                    message_word(sockfd,"y");
                }else{
                    message_word(sockfd,"n");
                }
            }
            // Telling the game if the game will be replayed or not
            message_receipt(sockfd, response);
            if(!strncmp(response, "q", 1)){
                goto replay;
            }else{
                printf("No replay possible\n");
            }   
        }
    }

    // Closing the client socket
    close(sockfd);
    return 0;
}