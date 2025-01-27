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

#include <string.h>
#include "nmenu.h"
#include "kclog.h"


NMenu::NMenu(NRect rect, bool horis) : NGroup(rect)
{
    mitems = NULL;
    ishoris = horis;
    posted = false;
    menu = new_menu(mitems);
    setbackground(getcolorpair(COLOR_WHITE,-1) | A_BOLD);
    setforeground(getcolorpair(COLOR_BLACK,COLOR_WHITE));
    set_menu_grey(menu, getcolorpair(COLOR_RED,-1)); //цвет разделителей
    set_menu_win(menu, win);
    postmenu();
    if (horis)
	scrollbar = NULL;
    else
    {
	scrollbar = new NScrollBar(NRect(getheight(),1, 1, getwidth()-1), 0, 0, ACS_VLINE | A_BOLD);
	insert(scrollbar);
    }
}


NMenu::~NMenu()
{
    //kLogPrintf("NMenu::~NMenu()\n");
    unpostmenu();
    if (!ishoris)
	delwin(menu_sub(menu));
    free_menu(menu);
    //освобождаем строки
    std::list<char*>::iterator it;
    for (it = itemnames.begin(); it != itemnames.end(); it++)
    {
	delete (*it);
    }
    for (it = itemcomments.begin(); it != itemcomments.end(); it++)
    {
	delete (*it);
    }
    //массив эл-тов
    if (mitems != NULL)
    {
	int i = 0;
	while (mitems[i] != NULL)
	    free_item(mitems[i++]);
	free(mitems);
    }
}


void NMenu::destroysubmenu() //закрыть субменю
{
    std::list<NView*>::iterator it;
    bool done = false;
    while (!done)
    {
	done = true;
	for (it = items.begin(); it != items.end(); it++)
	{
	    if (scrollbar==*it)
		continue; //подпорка
	    delete *it;
	    remove (*it);
	    done = false;
	    break;
	}
    }
}


void NMenu::additem(const char* name, const char* comment) //добавить эл-т в меню
{
    unpostmenu();
    ITEM** tmp = (ITEM**)realloc(mitems,(itemnames.size()+1)*sizeof(ITEM*));
    if (tmp != NULL)
        mitems = tmp;
    if (name == NULL) //финализация списка
    {
	mitems[itemnames.size()] = NULL;
	set_menu_items(menu, mitems);
	set_menu_mark(menu, " ");
	if ( !ishoris ) //для вертикальных есть рамка
	{
	    int lines = itemnames.size(); //видимых эл-тов
	    if ((lines>2)&&(lines + getbegrow() > getmaxy(stdscr) - 8))
		lines = getmaxy(stdscr)-getbegrow() - 8;
	    set_menu_format(menu, lines, 1);
	    resize(lines+2,menu->width+3); //изменяем размер под кол-во эл-тов
	    set_menu_sub(menu,derwin(win,getheight()-2,getwidth()-2,1,1));
	    if(asciilinedraw == 1)
		wborder(win, '|', '|', '-', '-', '+', '+', '+', '+');
	    else
		box(win,0,0); //рамка
	    if (scrollbar)
	    {
		scrollbar->resize(getheight()-2,1);
		scrollbar->move(1,getwidth()-1);
		scrollbar->setpos(0,itemnames.size(),top_row(menu),top_row(menu)+getheight()-2);
		scrollbar->refresh();
	    }
	}
	else //горизонтальное
	{
	    set_menu_format(menu, 1, itemnames.size());
	    set_menu_sub(menu,derwin(win,getheight(),getwidth(),0,0));
	    menu_opts_off(menu, O_ROWMAJOR);
	    menu_opts_off(menu, O_SHOWDESC);
	    set_menu_mark(menu, "  ");
	}
	//set_menu_win(menu, win);
	menu_opts_off(menu,O_SHOWMATCH);
	postmenu();
    }
    else
    {
	if (strlen(name) > 0)
	{
	    itemnames.push_back(strdup(name));
	    itemcomments.push_back(strdup(comment));
	    mitems[itemnames.size()-1] = new_item(itemnames.back(),itemcomments.back());
	}
	else
	{
	    itemnames.push_back(strdup(" "));
	    itemcomments.push_back(strdup(" "));
	    mitems[itemnames.size()-1] = new_item(itemnames.back(),itemcomments.back());
	    item_opts_off(mitems[itemnames.size()-1], O_SELECTABLE);
	}
    }
}


void NMenu::refresh()
{
    //int rows, cols;
    //menu_format(menu, &rows, &cols);
    if (getheight() == 1) //только для горизонтального меню
    {
	//закрашиваем фоном правую часть строки
	wattrset(win,bgattr);
	wmove(win,0,menu->width);
	//clrtoeol();
	int x,y;
	do
	{
	    getyx(win, y, x);
	    wprintw(win," ");
	}
	while (x < getwidth() - 1);
	//wattrset(win,0);
    }
    if (scrollbar)
    {
	scrollbar->setpos(0,itemnames.size(),top_row(menu),top_row(menu)+getheight()-2);
	scrollbar->refresh();
    }
    NGroup::refresh();
}


void NMenu::eventhandle(NEvent* ev) 	//обработчик событий
{
    if ( ev->done )
	return; //событие уже обработано кем-то ранее
    //отправляем всем подменю
    NGroup::eventhandle(ev);
    if ( ev->done )
	return; //выходим если какое-то подменю обработало событие
    //пытаемся обработать самостоятельно
    int y,x;
    menu_format(menu, &y, &x);
    NMouseEvent* mevent = (NMouseEvent*)ev;
    //колесо прокрутки мыши
    if ( ev->type == NEvent::evMOUSE )
    {
	if (isinside(mevent->row, mevent->col))
	{
	    //колесо вверх
	    #if NCURSES_MOUSE_VERSION > 1
	    if (mevent->cmdcode & BUTTON4_PRESSED) //NOT TESTED
	    #else
	    if (mevent->cmdcode & BUTTON4_PRESSED)
	    #endif
	    {
		if ( y > 1 )
		{
		    if (item_index(current_item(menu)) > 0) //элемент не первый
		    {
                //ITEM* preditem = mitems[item_index(current_item(menu)) - 1]; //предыдущий
                menu_driver(menu, REQ_SCR_UPAGE);
		    }
		    ev->done = true;
		}
	    }
	    //колесо вниз
	    #if NCURSES_MOUSE_VERSION > 1
	    if (mevent->cmdcode & BUTTON5_PRESSED) //NOT TESTED
	    #else
	    if ( mevent->cmdcode & (BUTTON2_PRESSED | REPORT_MOUSE_POSITION)) //REPORT_MOUSE_POSITION подпорка иначе теряет эвенты при быстрой прокрутке вниз
	    #endif
	    {
		if ( y > 1 ) //вертикальное
		{
		    ITEM* nextitem = mitems[item_index(current_item(menu)) + 1]; //какой следующий
		    if (nextitem != NULL) //элемент не последний
			menu_driver(menu, REQ_SCR_DPAGE);
		    ev->done = true;
		}
	    }
	}
    }
    //одиночный или двойной клик
    if (( ev->type == NEvent::evMOUSE ) && (((mevent->cmdcode & BUTTON1_CLICKED))||((mevent->cmdcode & BUTTON1_DOUBLE_CLICKED))))
    {
	if (isinside(mevent->row, mevent->col))
	{
	    int n;
	    if (y > 1) //вертикальное
		n = mevent->row - getabsbegrow() - 1 + menu->toprow;
	    else //горизонтальное
		n = mevent->col/(menu->itemlen+menu->spc_cols) - getabsbegcol();
	    if ((n >= 0)&&(n < item_count(menu)))
	    {
		if ( (item_opts(mitems[n]) & O_SELECTABLE) != 0 )
		{
		    set_current_item(menu, mitems[n]); //сделеть n-ный активным
		    destroysubmenu(); //закрыть субменю
		    if ((y == 1)||(mevent->cmdcode & BUTTON1_DOUBLE_CLICKED))
			action(); //putevent(new NEvent(NEvent::evKB, KEY_ENTER)); //открыть соотв субменю
		}
		ev->done = true;
	    }
	}
    }
    if ( ev->type == NEvent::evKB )
    {
	ev->done = true;
        switch(ev->keycode)
	{
	    case KEY_ENTER:
	    case '\n':
		action();
		break;
	    /*
	    case KEY_RIGHT:
		if ( x > 1)
		    menu_driver(menu, REQ_RIGHT_ITEM);
		else
		    ev->done = false;
		break;
	    case KEY_LEFT:
		if ( x > 1 )
		    menu_driver(menu, REQ_LEFT_ITEM);
		else
		    ev->done = false;
		break;
	    */
	    case KEY_UP:
		if ( y > 1 )
		{
		    if (item_index(current_item(menu)) > 0) //элемент не первый
		    {
			ITEM* preditem = mitems[item_index(current_item(menu)) - 1]; //предыдущий
			menu_driver(menu, REQ_UP_ITEM);
			if ( (item_opts(preditem) & O_SELECTABLE) == 0 )
			    menu_driver(menu, REQ_UP_ITEM); //чтобы пропустить разделитель
		    }
		}
		else
		    ev->done = false;
		break;
	    case KEY_DOWN:
		if ( y > 1 ) //вертикальное
		{
		    ITEM* nextitem = mitems[item_index(current_item(menu)) + 1]; //какой следующий
		    if (nextitem != NULL) //элемент не последний
		    {
			menu_driver(menu, REQ_DOWN_ITEM);
			if ( (item_opts(nextitem) & O_SELECTABLE) == 0 )
			    menu_driver(menu, REQ_DOWN_ITEM); //чтобы пропустить разделитель
		    }
		}
		else
		    ev->done = false;
		break;
	    default:
		//если событие нам неизвестно, то отказываемся от его
		//обработки (снимаем флаг done)
		ev->done = false;
	} //switch
    }
    if (ev->done) //если обработали, то нужно перерисоваться
	refresh();
}


