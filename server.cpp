#include <algorithm>
#include <iostream>
#include <set>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
using namespace std;
int set_nonblock(int fd) // фнкция описывающая неблокирующий режим сокета
{
	int flags; // флаги
#if defined(O_NONBLOCK) 
	if(-1 == (flags = fcntl(fd, F_GETFL, 0)))
	{
		flags = 0;
		return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
#else
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}
int main()
{
	int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Декларируем серверный сокет
	set<int> ClientSockets; // контейнер для хранения множества клиентов
	struct sockaddr_in SockAddr; // структура для задания пары IP port
	SockAddr.sin_family = AF_INET; // указываем что используем версию IPv4
	SockAddr.sin_port = htons(12345); // приводим порт 12345 к сетевому порядку байт
	SockAddr.sin_addr.s_addr = htonl(INADDR_ANY); // открываем сокет на все IP аналог 0.0.0.0
	bind(MasterSocket, (struct sockaddr*)(&SockAddr), sizeof(SockAddr)); // биндим сокет(привязка сокета к порту) 
	set_nonblock(MasterSocket); // переводим сокет в неблокирующий режим, для того что бы в select мы могли повесить accept
	listen(MasterSocket, SOMAXCONN); // включаем прием соединений, создаем очередь
	while(true) // бесконечный цикл
	{
		fd_set Set; // структура 1024 бита индекс этого массива - номер сокета
		FD_ZERO(&Set); // ставит флажки в 0
		FD_SET(MasterSocket, &Set); // добавляем в массив номер сокета
		for(auto Iter = ClientSockets.begin(); Iter != ClientSockets.end(); Iter++) // итерируем по контейнеру 
		{
			FD_SET(*Iter, &Set); // добавили все сокеты в Set
		}
		int Max = max(MasterSocket, *max_element(ClientSockets.begin(), ClientSockets.end())); // ищем максимальный дескриптор
		select(Max + 1, &Set, NULL, NULL, NULL); // select на чтение
		for(auto Iter = ClientSockets.begin(); Iter != ClientSockets.end(); Iter++) // пробегаем по массиву сокетов
		{
			if(FD_ISSET(*Iter, &Set)) // проверяем есть ли сокет клиента в нашем сете
			{
				static char Buffer[1024]; // создаем буфер
				int RecvSize = recv(*Iter, Buffer, 1024, MSG_NOSIGNAL); // читаем в буфер из сокета
				if((RecvSize == 0) && (errno != EAGAIN)) // проверка на наличие соединения и возможности чтения(EAGAIN)
				{
					shutdown(*Iter, SHUT_RDWR); // закрываем соединение
					close(*Iter); // закрываем дескриптор
					ClientSockets.erase(Iter);  // удаляем дескриптор из списка
				}
				else if(RecvSize != 0) // если есть инфа для чтения
				{
					for(auto Iter1 = ClientSockets.begin(); Iter1 != ClientSockets.end(); Iter1++) // пробегаемся по всему массиву сокетов
					{
						if(Iter != Iter1)
						{
							send(*Iter1, Buffer, RecvSize, MSG_NOSIGNAL); // отправляем на все сокеты то что приняли
						}
					}
				}
			}
		}
		if(FD_ISSET(MasterSocket, &Set)) // если select сработает на мастере
		{
			int ClientSocket = accept(MasterSocket, 0, 0); // принимаем соединение
			set_nonblock(ClientSocket); // делаем новый сокет неблокирующим
			ClientSockets.insert(ClientSocket); // добавляем сокет в список
		}
	}
	return 0;
}