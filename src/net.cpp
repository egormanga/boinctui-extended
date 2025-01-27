// =============================================================================
// This file is part of boinctui.
// http://boinctui.googlecode.com
// Copyright (C) 2012,2013 Sergey Suslov
//
// boinctui is free software; you can redistribute it and/or modify it  under
// the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// boinctui is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details
// <http://www.gnu.org/licenses/>.
// =============================================================================

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <list>
#include <algorithm>
#include "net.h"
#include "kclog.h"


void TConnect::createconnect(/*const char* shost, const char* sport*/)
{
    kLogPrintf("connecting...");
    //this->shost = strdup(shost);
    //this->sport = strdup(sport);
    struct sockaddr_in boincaddr;
    //resolving
    memset(&boincaddr,0, sizeof(boincaddr));
    boincaddr.sin_family = AF_INET;
    boincaddr.sin_port = htons(atoi(sport));
    if (inet_aton(shost,&boincaddr.sin_addr) == 0) //в shost не ip адрес
    {
	struct hostent *hostp = gethostbyname(shost); //пытаемся отресолвить
	if ( hostp == NULL )
	    kLogPrintf("host %s lookup failed\n",shost);
	else
	    memcpy(&boincaddr.sin_addr, hostp->h_addr, sizeof(boincaddr.sin_addr));
    }
    int	hsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    //connection
    if (connect(hsock, (struct sockaddr *) &boincaddr, sizeof(boincaddr)) < 0)
    {
        kLogPrintf("connect %s:%s failed!\n",shost,sport);
	this->hsock = -1;
    }
    else
    {
	this->hsock = hsock;
	kLogPrintf("OK\n");
    }
}


void TConnect::disconnect()
{
    kLogPrintf("disconnecting %s:%s ...",shost,sport);
    if (hsock != -1)
	close(hsock);
    hsock = -1;
    kLogPrintf("OK\n");
}


void TConnect::sendreq(const char* fmt, ...) //отправить запрос на сервер
{
    va_list	args;
    va_start(args, fmt);
    sendreq(fmt,args);
    va_end(args);
}


void TConnect::sendreq(const char* fmt, va_list vl) //отправить запрос на сервер
{
    //формируем строку запроса
    char req[1024];
    vsnprintf(req, sizeof(req), fmt, vl);
    //kLogPrintf("[%s]\n",req);
    //конектимся (если соединения еще нет)
    if (hsock == -1)
	createconnect(/*shost,sport*/);
    //отправляем запрос
    if (send(hsock, req, strlen(req), 0) != (int)strlen(req))
    {
	kLogPrintf("send request %s:%s error\n",shost,sport);
	disconnect();
    }
}


char* TConnect::waitresult() //получить ответ от сервера (потом нужно освобождать память извне)
{
    if (hsock == -1)
	createconnect(/*shost,sport*/);
    //чтение частями из сокета
    char* answbuf = NULL;
    int totalBytesRcvd = 0;
    int answbuflen = 0;
    char bufpart[1024]; //фрагмент
    int bytesRcvd;
    do
    {
        if ((bytesRcvd = recv(hsock, bufpart, sizeof(bufpart), 0)) <= 0 )
	{
	    kLogPrintf("recv fail %s:%s\n",shost,sport);
	    disconnect();
	    return NULL;
	}
	else
	//printf("allbytes= %d received %d bytes\n",totalBytesRcvd,bytesRcvd);
	//копируем в суммарный буфер
	answbuflen = answbuflen+bytesRcvd;
	char* tmp = (char*)realloc(answbuf,answbuflen);
	if (tmp != NULL)
	    answbuf = tmp;
	memcpy(&answbuf[totalBytesRcvd], bufpart, bytesRcvd);
        totalBytesRcvd += bytesRcvd;
        if (answbuf[totalBytesRcvd-1] == '\003')
	{
	    answbuf = (char*)realloc(answbuf,answbuflen + 1);
	    answbuf[totalBytesRcvd-1] = '\0'; //003 -> 0
	    answbuf[totalBytesRcvd] = '\0'; //терминирующий 0
	    break;
	}
    }
    while (true);
    //    printf("'%s' \nlen=%d\n", answbuf, strlen(answbuf));
    return answbuf;
}

