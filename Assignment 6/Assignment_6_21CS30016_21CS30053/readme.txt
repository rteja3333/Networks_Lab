TO run the entire program:
shell 1: make server 
shell 2: make client


To change the probablity of dropping messages :
    in simDNSServer.c : change drop_probablity value
    in sinDNSClient.c : change drop_probablity value

kept 0 as default value 

I had checked with probability values from ranging from 0.1 to 0.9 
-> in client side , it retransmits the query packet for 3 times at max
->in server side , it generates response as soon as it recieves the query 

