// All the includes
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <fstream>
#include <sstream>

/*
    I use 1 letter messages to communicate between the client and server, i.e. server just sends 1 letter, and client prints
    the corresponding whole message
*/

using namespace std;

// Global variables and constants
const int port = 8024;
int total_players = 0;
int all_players = 0;
int game_ids = 0;
pthread_mutex_t cnt;

// Helper function to deal with sending error with error code
int integer_receipt(int socket_file_descriptor){
    int msg = 0;
    int valid = read(socket_file_descriptor, &msg, sizeof(int));
    if(valid < 0){
        return -1;
    }
    return msg;
}

void send_error(char *msg){
    perror(msg);
    pthread_exit(NULL);
}

// Helper function to write and a read a message
// We don't want to write the verification for error everytime 
void message_int(int socket_file_descriptor, int msg){
    int valid = write(socket_file_descriptor, &msg, sizeof(int));
    if(valid < 0){
        send_error("Error writing a message\n");
    }
}

void message_word(int socket_file_descriptor, char* msg){
    int valid = write(socket_file_descriptor, msg, strlen(msg));
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

// Checks if a win has been acheived after playing the current move
int check_game_win(char game[3][3], int current_player){
    char to_search = (current_player ? 'X' : 'O');
    // Checking rows
    if(game[0][0] == to_search && game[1][0] == to_search && game[2][0] == to_search) return 1;
    if(game[0][1] == to_search && game[1][1] == to_search && game[2][1] == to_search) return 1;
    if(game[0][2] == to_search && game[1][2] == to_search && game[2][2] == to_search) return 1;

    // Checking columns
    if(game[0][0] == to_search && game[0][1] == to_search && game[0][2] == to_search) return 1;
    if(game[1][0] == to_search && game[1][1] == to_search && game[1][2] == to_search) return 1;
    if(game[2][0] == to_search && game[2][1] == to_search && game[2][2] == to_search) return 1;

    // Checking diagonals
    if(game[0][0] == to_search && game[1][1] == to_search && game[2][2] == to_search) return 1;
    if(game[0][2] == to_search && game[1][1] == to_search && game[2][0] == to_search) return 1;

    return 0;
}

// Threader creates a thread for the pair of players we want to pair with
void threader(int* sockfd);


// Running a tic-tac-toe game on this thread
void *game_instance(void* sockfd_void){

    // Clock to measure game time
    struct timespec start_time, end_time, midend_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    // Let us store all the logs for the game in the string log_temp
    ofstream fout;
    string log_temp;

    // Sockfd stores the file descriptors as the first two ints and the player IDs as the next two ints
    int* sockfd = (int*) sockfd_void;
    int pIds[2] = {sockfd[2], sockfd[3]};

    // Increasing game_ids variable and assigning a game ID to our current player
    // Using mutex lock and unlock to ensure correctness
    pthread_mutex_lock(&cnt);
    int gameID = game_ids + 1;
    game_ids++;
    pthread_mutex_unlock(&cnt);

    // Updating basic game details to the log file
    log_temp +=  string("Game details\nGame ID : ") + to_string(gameID) + string("\nPlayer 1 ID : ") + to_string(pIds[0]) + string("\nPlayer 2 ID : ") + to_string(pIds[1]); 

    // Sends game pre-start messages to the players
    message_word(sockfd[0], "x");
    message_word(sockfd[1], "y");
    sleep(0.2);

    // Sends partner IDs to the players
    message_int(sockfd[0], pIds[1]);
    message_int(sockfd[1], pIds[0]);
    sleep(0.2);

    // Sends game start messages to the players
    message_word(sockfd[0], "s");
    message_word(sockfd[1], "s");
    sleep(0.2);
    
    // Initialise Empty board
    char game[3][3];
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
            game[i][j] = ' ';
        }
    }

    // We gave 2 the X symbol, hence 2 always starts first
    int current_player = 1; // Starting player
    bool gameon = true; // Whether game is running currently
    bool disconnection = false; // Whether a disconnection was found
    int moves = 0;
    bool replaying = 0;
    double game_time;

    while(gameon && moves < 9){
        // Sending the waiting and the moving messages for both the players
        message_word(sockfd[1 - current_player], "w");
        message_word(sockfd[current_player], "m");
        sleep(0.2);

        // We ask users for two ints and then send a combination of those to the server for easy management
        // Looping till we get a valid move
        int msg = -1;
        bool first_time = true;

        // Move checking
        while(msg%3 < 0 || msg%3 > 2 || msg/3 < 0 || msg/3 > 2 || game[msg/3][msg % 3] != ' '){
            if(msg == -100){
                // Timeout error so we stop and ask for replay
                message_word(sockfd[current_player], "k");
                message_word(sockfd[1 - current_player], "c");

                // Adding all these details to the log file
                log_temp += to_string(sockfd[2+current_player]) + " ID player did not play the move on time\n";
                clock_gettime(CLOCK_MONOTONIC, &midend_time);
                game_time = ((double)(midend_time.tv_sec - start_time.tv_sec));
                log_temp += string("\nGame lasted for ") + to_string(game_time) + string(" seconds\nEnd of Game\n\n");
                fout.open("log.txt", std::ios_base::app);

                pthread_mutex_lock(&cnt);
                fout << log_temp << endl;
                pthread_mutex_unlock(&cnt);
                fout.close();

                // Going to the part which asks for replaying
                goto ask_replay_disconnect;
            }
            // Invalid move represented by 'i'
            if(!first_time) message_word(sockfd[current_player], "i");
            sleep(0.2);
            int response = integer_receipt(sockfd[current_player]);
            sleep(0.2);
            first_time = false;
            msg = response;
        }
        
        // No response from current move player, update the log file for the same
        if(msg == -1){
            // Sending the disconnection signal
            message_word(sockfd[1 - current_player], "d");
            while(integer_receipt(sockfd[1 - current_player] != 1));

            fout.open("log.txt",std::ios_base::app);
            log_temp +="Player disconnected. Game over. \n";

            // Using mutex for consistent update to the log file
            pthread_mutex_lock(&cnt);
            fout << log_temp << endl;
            pthread_mutex_unlock(&cnt);
            fout.close();
            disconnection = true;
            break;
        }

        // Updating game locally and sending the updates to the clients about the move, also managing the log file
        game[msg/3][msg % 3] = current_player ? 'X' : 'O';
        log_temp += string("\nMove Number : ") + to_string(int(moves + 1)) + string(" || Player ID : ") + to_string(pIds[current_player]) + string(" || Move : ") + to_string(msg/3) + " " + to_string(msg%3);
        message_word(sockfd[1 - current_player], "u");
        message_word(sockfd[current_player], "u");
        sleep(0.2);
        message_int(sockfd[1 - current_player], msg);
        message_int(sockfd[current_player], 9 + msg);
        sleep(0.2);   

        // Checking if there is a winner after this move
        if(check_game_win(game, current_player)){
            message_word(sockfd[current_player], "o");
            message_word(sockfd[1 - current_player], "l");
            sleep(0.2);
            log_temp += string("\nMatch ended in a win for ") + string("Winner ID: ") + to_string(pIds[current_player]);
            gameon = false;
        }

        // Move handling
        moves++;
        current_player = 1 - current_player;
    }
    // Game has been drawn
    if(gameon && !disconnection){
        message_word(sockfd[current_player], "e");
        message_word(sockfd[1 - current_player], "e"); 
        sleep(0.2);
        log_temp += "\nMatch ended in a draw\n";
    }   

    // Getting the time for end of game and thus calculating total game time
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    game_time = ((double)(end_time.tv_sec - start_time.tv_sec));

    if(!disconnection){
        // Update the log file
        log_temp += string("\nGame lasted for ") + to_string(game_time) + string(" seconds\nEnd of Game\n\n");
        fout.open("log.txt", std::ios_base::app);
        pthread_mutex_lock(&cnt);
        fout << log_temp << endl;
        pthread_mutex_unlock(&cnt);
        fout.close();
        
        ask_replay_disconnect:;
        // Ask if they want to replay
        sleep(0.2);
        message_word(sockfd[current_player], "r");
        message_word(sockfd[1 - current_player], "r");

        char ans0[10];
        char ans1[10];

        // Receive their response about whether they want to replay
        message_receipt(sockfd[0], ans0);
        message_receipt(sockfd[1], ans1);

        // If both say yes then we call threader and make new thread and make them replay each other
        if(!strncmp(ans0, "y", 1) && !strncmp(ans1, "y", 1)){
            message_word(sockfd[current_player], "q");
            message_word(sockfd[1 - current_player], "q");
            threader(sockfd);
            replaying =1;
        }else{
            message_word(sockfd[current_player], "n");
            message_word(sockfd[1-current_player], "n");
        }
    }

    // If they aren't replaying then we close these sockets
    if(!replaying) {
        close(sockfd[0]);
        close(sockfd[1]);
    
        free(sockfd);
    }

    // Update the total number of players
    pthread_mutex_lock(&cnt);
    total_players -= 2;
    pthread_mutex_unlock(&cnt);

    pthread_exit(NULL);
}


void threader(int* sockfd){
    // Creating a thread and running the game on it
    pthread_t thread;
    int thread_create = pthread_create(&thread, NULL, game_instance, (void*)sockfd);
    if (thread_create){
        printf("Thread creation failed with return code %d\n", thread_create);
        exit(-1);
    }
}

int main(int argc, char** argv){
    // Log file
    ofstream fout;
    fout.open("log.txt", std::fstream::out | std::fstream::trunc);
    fout << "All moves are written in the form of Row : Column\n" << "File is replaced every time a new server run starts\n\n";
    fout.close();
    // Creating server listening socket
    int sockfd_listener;
    struct sockaddr_in server;
    sockfd_listener = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_listener < 0){
        printf("Error creating socket\n");
        exit(-1);
    }
    server.sin_family = AF_INET; 
    server.sin_port = htons(port);     
    server.sin_addr.s_addr = INADDR_ANY; 

    // Binding server listening socket
    if (bind(sockfd_listener, (struct sockaddr*) &server, sizeof(server)) < 0){
       printf("Error binding socket\n");
       exit(-1);
    }

    pthread_mutex_init(&cnt, NULL);
    printf("Game server started. Waiting for players.\n");

    // Infinite loop of listening for clients
    while(true){
        // To limit the number of threads we create
        if(total_players < 500){
            // Creating file descriptors for the two players in a game as the first two ints
            // The next two ints are the players id's for the two players playing the game
            int *sockfd = (int*)malloc(4*sizeof(int)); 
            memset(sockfd, 0, 4*sizeof(int));


            // Finding two players
            struct sockaddr_in game_creation;

            int players_in_current_game = 0;
            while(players_in_current_game < 2) {

                // Calling the listen function with a backlog of max_players;
                listen(sockfd_listener, 500 - total_players);
                socklen_t length = sizeof(game_creation);
                memset(&game_creation, 0, length);
                

                // Accepting connections from client
                sockfd[players_in_current_game] = accept(sockfd_listener, (struct sockaddr *) &game_creation, &length);
                if(sockfd[players_in_current_game] < 0){
                    send_error("Error accepting connection\n");
                }


                // Assigning index to player (used in game and for assigning X and O)
                message_int(sockfd[players_in_current_game], all_players + 1);
                sockfd[2 + players_in_current_game] = all_players + 1;
                total_players++;
                all_players++;
                

                if(players_in_current_game == 0){
                    // Asking first player to wait for the second player
                    message_word(sockfd[0], "h");
                }

                players_in_current_game++;
            }

            threader(sockfd);
        }
    }

    // Even though we are running loop, for consistency let's close listening socket and destroy pthread
    close(sockfd_listener);
    pthread_mutex_destroy(&cnt);
    pthread_exit(NULL);
    return 0;
}
