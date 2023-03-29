Problem 1 : Tic-Tac-Toe on Lan

Usage details:

gameserver.cpp - The Server File



How to Compile 
g++ -pthread gameserver.cpp -o gameserver

How to run
./gameserver


clientserver.cpp - The Client File (The player file)

How to Compile
g++ gameclient.cpp -o gameclient


How to run
./gameclient <your server ip>

for eg:
./gameclient 192.168.63.129
if 192.168.63.129 is my server ip

The Detailed Explanation of the code is given section by section in the actual code as comments

Problem 2 : Yet another ping program

Function takes in argument which is the IP address which we want to ping

Usage details:

To compile the problem use command
g++ yapp.cpp -o yapp

Suppose we want to ping google.com (IP address 8.8.8.8) through ubuntu, we use command
sudo ./yapp 8.8.8.8
Sudo is required

Function details:

Main function:

We get IP address from the command line and store it in char* ip.
We use inet_aton to verify hostname
We create a socket as per our requirements for sending the ICMP packet
Then we call the pinger function to send a ping

The ping packet consists of an ICMP header and also some payload to have total packet size as standard 64 bits

Then in the pinger function, we create the ping packet, setup the timeout using setsockopt.
We then send and receive the packets and calculate the response time.
