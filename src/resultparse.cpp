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

#include <expat.h>
#include <stdio.h>
#include <string.h>
#include <stack>
#include "resultparse.h"
#include "kclog.h"


std::stack<Item*> curitem; //на верху стека указатель на текущий заполняемый эл-т а голова это вершина дерева

void callbackStartElement(void* userdata, const char* name, const char** atts); //колбэк начала эл-та
void callbackEndElement(void* userdata, const char* name);			//коллбэк завершения эл-та
void callbackData(void *userdata, const char *content, int len); //коллбэк значения эл-та


char* stripinvalidtag(char* xml, int len) 	//ГРЯЗНЫЙ ХАК нужен чтобы до парсинга удалить кривые теги
						//в сообщениях вида <a href=.. </a> иначе будет ошибка парсинга
{
    const char* tag1 = "<body>";
    const char* tag2 = "</body>";
    //int bytesdone = 0; // Bytes viewed
    char* pos = (char*)xml;
    while (pos < xml + len)
    {
	char* x1 = strstr(pos, tag1);
        char* x2 = strstr(pos, tag2);
        if ((x1 != NULL)&&(x2 != NULL))
	{
	    for(char* p = x1 + strlen(tag1); p < x2; p++)
	    {
		if ((*p == '<')||(*p == '>')) // Remove HTML tags
		    *p = ' ';
	    }
	    pos = (x1>x2)? x1:x2; // Pick the largest
	    pos++;
	}
	else
	    break;
    }
    return xml;
}


Item* xmlparse(const char* xml, int len, std::string& errmsg) //xml строка с xml len ее размер в байтах
{
    XML_Parser parser;
    void* ret;
    parser = XML_ParserCreate(NULL/*"UTF-8"*/);
    XML_SetUserData(parser, (void*) &ret);
    XML_SetElementHandler(parser, callbackStartElement, callbackEndElement); //устанавливаем колбэки
    XML_SetCharacterDataHandler(parser, callbackData);

    Item* roottree = new Item(""); //создаем корневой эл-т дерева
    curitem.push(roottree); //и делаем его текущим
    int retcode = XML_Parse(parser, xml, len, XML_TRUE); //собственно парсинг
    if (retcode == XML_STATUS_ERROR)
    {
	    kLogPrintf("XML Error: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
        errmsg = std::string("XML error:") + std::string(XML_ErrorString(XML_GetErrorCode(parser)));
    }
    XML_ParserFree(parser);
    while (!curitem.empty())
	curitem.pop(); //очищаем стек
    return roottree; //возвращаем постороенное дерево
}


void callbackStartElement(void* userdata, const char* name, const char** atts)
{
    //kLogPrintf("\t+ %s\n",name);
    //аттрибуты тегов типа <header length=\"4\">
    //length=4 это атрибут (в boinc таких тегов нет?)
    /*
    for(int i = 0; atts[i]; i += 2)
	printf(" %s= '%s'", atts[i], atts[i + 1]);
    printf("\n");
    */

    //создаем новый эл-т дерева
    Item* pitem = new Item(name);
    //добавляем созданный эл-т в вышестоящий (если он есть)
    if (!curitem.empty())
    {
	Item* parentitem = curitem.top(); //владелец всегда на верху стека
	parentitem->addsubitem(pitem);
    }
    //делаем созданный эл-т текущим (кладем в стек)
    curitem.push(pitem);
}


void callbackEndElement(void* userdata, const char* name)
{
    //kLogPrintf("\t- %s\n",name);
    //удаляем текущий эл-т из стека (текущим становится его родитель)
    curitem.pop();
}


void callbackData(void *userdata, const char *content, int len)
{
    char *tmp = (char*)malloc(len+1);
    strncpy(tmp, content, len);
    tmp[len] = '\0';
    //data = (void *) tmp;
    //kLogPrintf("\ncallbackData()-->[%s]<-- len=%d\n",tmp,len);
    //заносим значение в текущий эл-т
    bool empty = true;
    for (uint i = 0; i < strlen(tmp); i++)
    {
	if (tmp[i] != ' ')
	{
	    empty = false;
	    break;
	}
    }
    if ( (!empty) && (strcmp(tmp,"\n") != 0) ) //пропускаем пустые строки
    {
	Item* pitem = curitem.top(); //текущий на верху стека
	pitem->appendvalue(tmp);
    }
    free(tmp);
}

