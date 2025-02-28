#include "chatRoom.hpp"

/*
when client joins, it should join a room.
hence a session(socket, room) will be created and this client will have a session
now, whenever it wants to start its delivery it call call start() function, where it will listen for incoming messages
when client wants to send message it can call session's deliver() message
session will call deliver() to deliver the message to the room
room will call write() function to write any message to the client's queue
It will trigger the write() for each participant except the sender itself
*/

void Room::join(ParticipantPointer participant){
    this->participants.insert(participant);
}

void Room::leave(ParticipantPointer participant){
    this->participants.erase(participant);
}

void Room::deliver(ParticipantPointer participant, Message &message){
    messageQueue.push_back(message);
    while(messageQueue.size()!=0){
        for(ParticipantPointer _participant: this->participants){
            if(participant != _participant){
                _participant->write(message);
            }
        }
    }
}

void Session::async_read() {
    boost::asio::streambuf buffer;
    auto read_handler = [&](boost::system::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
            std::string data(boost::asio::buffers_begin(buffer.data()), boost::asio::buffers_begin(buffer.data()) + bytes_transferred);
            buffer.consume(bytes_transferred);
            std::cout << "Received: " << data << std::endl;
            Message message(data);
            deliver(message);
            async_read();
        } else if (ec == boost::asio::error::eof) {
            std::cout << "Connection closed by peer" << std::endl;
            room.leave(shared_from_this());
        } else {
            std::cout << "Read error " << ec.message() << std::endl;
            room.leave(shared_from_this());
        }
    };
    boost::asio::async_read_until(clientSocket, buffer, "\n", read_handler);
}

void Session::async_write(char *messageBody, size_t messageLength){
    auto write_handler = [&](boost::system::error_code ec, std::size_t bytes_transferred){
        if(!ec){
            std::cout<<"Data is written to the socket: "<<messageBody<<std::endl;
        }else{
            std::cerr << "Write error: " << ec.message() << std::endl;
        }
    };
    boost::asio::async_write(clientSocket, boost::asio::buffer(messageBody, messageLength), write_handler);
}

void Session::start(){
    room.join(shared_from_this());
    async_read();
}

Session::Session(tcp::socket s, Room& r): clientSocket(std::move(s)), room(r){};

void Session::write(Message &message){
    messageQueue.push_back(message);
    while(messageQueue.size() != 0){
        Message message = messageQueue.front();
        messageQueue.pop_front();
        bool header_decode = message.decodeHeader();
        if(header_decode){
           char* body = message.getBody(); 
            async_write(body, message.getBodyLength());
        }else{
            std::cout<<"Message length exceeds the max length"<<std::endl;
        }
    }
}

void Session::deliver(Message& incomingMessage){
    messageQueue.push_back(incomingMessage);
    room.deliver(shared_from_this(), incomingMessage);
}
using boost::asio::ip::address_v4;

void accept_connection(boost::asio::io_context &io, char *port, Room &room, const tcp::endpoint& endpoint) {
    tcp::acceptor acceptor(io, tcp::endpoint(address_v4::any(), std::atoi(port)));
    tcp::socket socket(io);
    acceptor.async_accept([&](boost::system::error_code ec, tcp::socket socket) {
        if(!ec) {
            std::make_shared<Session>(std::move(socket), room)->start();
        }
        accept_connection(io, port, room, endpoint);
    });
}


int main(int argc, char *argv[]) {
    try {
        if(argc < 2) {
            std::cerr << "Usage: server <port> [<port> ...]\n";
            return 1;
        }
        Room room;
        boost::asio::io_context io_context;
        tcp::endpoint endpoint(tcp::v4(), atoi(argv[1]));
        accept_connection(io_context, argv[1], room, endpoint);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}