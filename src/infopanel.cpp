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
#include <algorithm>
#include <cmath>
#include "kclog.h"
#include "infopanel.h"
#include "net.h"
#include "resultparse.h"


Item* findDay( std::vector<Item*>& days, time_t d ) //ищем в статистике день d если нет вернет NULL
{
    std::vector<Item*>::reverse_iterator rit;
    for (rit = days.rbegin(); rit != days.rend(); rit++)
    {
	Item* day = (*rit)->findItem("day");
	if (day != NULL)
	{
	    if ( day->getdvalue() == d )
		return (*rit);
	}
    }
    return NULL;
}


std::string InfoPanel::getdayname(time_t ltime) // Name of the day ("today", "yesterday" etc)
{
    time_t now = time(NULL);
    if ( now/(3600 * 24) == ltime/(3600 * 24) )
	return "today";
    if ( now/(3600 * 24) == 1+ltime/(3600 * 24) )
	return "yesterday";
    return "";
}


void InfoPanel::refresh()
{
    if (srv == NULL)
	return;
    if (srv->statedom.empty())
    {
	needrefresh = true;
	werase(win);
	mvwprintw(win,0,0,"%s:%s\noffline",srv->gethost(),srv->getport());
	NView::refresh();
	return;
    }
    wattrset(win,getcolorpair(COLOR_WHITE,-1));
    wattron(win, A_REVERSE);
    mvwprintw(win,0,0,"       Tasks        ");
    wattroff(win, A_REVERSE);

    mvwprintw(win,1,0,"all              %3d",nalltasks);
    mvwprintw(win,2,0,"active           %3d",nactivetasks);
    mvwprintw(win,3,0,"run              %3d",nruntasks);
    mvwprintw(win,4,0,"wait             %3d",nqueuetasks);
    mvwprintw(win,5,0,"done             %3d",ndonetasks);
    mvwprintw(win,6,0,"other            %3d",nothertasks);

    wattron(win,A_REVERSE);
    mvwprintw(win,7,0,"      Storage       ");
    wattroff(win,A_REVERSE);

    mvwprintw(win,8,0, "total     %8.2fGb",dtotal/(1024*1024*1024));
    mvwprintw(win,9,0, "free      %8.2fGb",dfree/(1024*1024*1024));
    mvwprintw(win,10,0,"allowed   %8.2fGb",dallowed/(1024*1024*1024));
    mvwprintw(win,11,0,"in use    %8.2fGb",dboinc/(1024*1024*1024));

    wattron(win,A_REVERSE);
    mvwprintw(win,12,0,"     Statistics     ");
    wattroff(win,A_REVERSE);

    bool compact = true; // Compact stats output if user=host
    int line,col;
    getyx(win,line,col);
    if ( (!compact)||(std::abs(usertotal - hosttotal) > 1) )
    {
	mvwprintw(win,line++,0,"user total%10.0f\n",usertotal);
	mvwprintw(win,line++,0,"host total%10.0f\n",hosttotal);
    }
    else
	mvwprintw(win,line++,0,"total     %10.0f\n",usertotal);
    if ( (!compact)||(std::abs(useravg - hostavg) > 1) )
    {
	mvwprintw(win,line++,0,"user avg  %10.0f\n",useravg);
	mvwprintw(win,line++,0,"host avg  %10.0f\n",hostavg);
    }
    else
	mvwprintw(win,line++,0,"average   %10.0f\n",useravg);
    tm* ltime = localtime(&laststattime);
    char buf[128];
    strftime(buf, sizeof(buf),"%-e %b",ltime); //"%-e %b %-k:%M"
    mvwprintw(win,line++,0,"%-s %s\n", buf, getdayname(laststattime).c_str()); //дата/время последней статистики
    //wattrset(win,0);
    if ( (!compact)||(std::abs(lastdayuser - lastdayhost) > 1) )
    {
	if ( asciilinedraw == 1)
	{
	    wmove(win,line++,0); wprintw(win,"+->user   %10.0f\n",lastdayuser);
	    wmove(win,line++,0); wprintw(win,"+->host   %10.0f\n",lastdayhost);
	}
	else
	{
	    mvwaddch(win,line++,0,ACS_LTEE);     waddch(win,ACS_HLINE); wprintw(win,">user   %10.0f\n",lastdayuser);
	    mvwaddch(win,line++,0,ACS_LLCORNER); waddch(win,ACS_HLINE); wprintw(win,">host   %10.0f\n",lastdayhost);
	}
    }
    else
    {
	if( asciilinedraw == 1)
	{
	    wmove(win,line++,0); wprintw(win,"-->daily  %10.0f\n",lastdayhost);
	}
	else
	{
	    mvwaddch(win,line++,0,ACS_LLCORNER); waddch(win,ACS_HLINE); wprintw(win,">daily  %10.0f\n",lastdayhost);
	}
    }
    // Projects
    mvwprintw(win, line++,0,"\n");
    for (int i = 0; i < (int)projects.size(); i++) // Iterate through projects
    {
	// Calculate how many rows are needed
	int needlines = 2;
	if (!projects[i].sstatus.empty())
	    needlines++;
	if ( (!compact)||(std::abs(projects[i].user - projects[i].host) > 1) )
	    needlines += 2;
	else
	    needlines++;
	if ( (!compact)||(std::abs(projects[i].userlastday - projects[i].hostlastday) > 1) )
	    needlines += 2;
	else
	    needlines++;
	// Check how many lines are left in the window
	if ( ( getheight()-line ) < needlines )
	    break; // Do not print if there are few lines left
	// Output project details
	wattrset(win,getcolorpair(COLOR_YELLOW,-1));
	mvwprintw(win,line++,0,"%s\n",projects[i].name.c_str());
	if (!projects[i].sstatus.empty())
	{
	    wattrset(win,getcolorpair(COLOR_RED,-1));
	    mvwprintw(win,line++,0,"%s\n",projects[i].sstatus.c_str());
	}
	wattrset(win,getcolorpair(COLOR_WHITE,-1));
	if ( (!compact)||(std::abs(projects[i].user - projects[i].host) > 1) )
	{
	    mvwprintw(win,line++,0,"user total%10.0f\n",projects[i].user);
	    mvwprintw(win,line++,0,"host total%10.0f\n",projects[i].host);
	}
	else
	    mvwprintw(win,line++,0,"total     %10.0f\n",projects[i].user);
	ltime = localtime(&projects[i].laststattime);
	strftime(buf, sizeof(buf),"%-e %b",ltime); //"%-e %b %-k:%M"
	mvwprintw(win,line++,0,"%-s %s\n",buf, getdayname(projects[i].laststattime).c_str());
	if ( (!compact)||(std::abs(projects[i].userlastday - projects[i].hostlastday) > 1) )
	{
	    if (asciilinedraw == 1)
	    {
		wmove(win,line++,0); wprintw(win,"+->user   %10.0f\n",projects[i].userlastday);
		wmove(win,line++,0); wprintw(win,"+->host   %10.0f\n",projects[i].hostlastday);
	    }
	    else
	    {
		mvwaddch(win,line++,0,ACS_LTEE);     waddch(win,ACS_HLINE); wprintw(win,">user   %10.0f\n",projects[i].userlastday);
		mvwaddch(win,line++,0,ACS_LLCORNER); waddch(win,ACS_HLINE); wprintw(win,">host   %10.0f\n",projects[i].hostlastday);
	    }
	}
	else
	{
	    if (asciilinedraw == 1)
	    {
		wmove(win,line++,0); wprintw(win,"-->daily  %10.0f\n",projects[i].userlastday);
	    }
	    else
	    {
		mvwaddch(win,line++,0,ACS_LLCORNER); waddch(win,ACS_HLINE); wprintw(win,">daily  %10.0f\n",projects[i].userlastday);
	    }
	}
    }
    if ( line < getheight() )
	wclrtobot(win); // Clear the bottom of the window (if this is not the last line, otherwise erase the symbol in the lower right)
    NView::refresh();
}


void InfoPanel::updatedata()
{
    if (srv == NULL)
	return;
    // ===Process data===
    if (srv->statedom.empty())
	return;
    Item* tmpstatedom = srv->statedom.hookptr();
    Item* client_state = tmpstatedom->findItem("client_state");
    nactivetasks = 0;
    ndonetasks = 0;
    nruntasks = 0;
    nqueuetasks = 0;
    if (client_state != NULL)
    {
        std::vector<Item*> results = client_state->getItems("result");
	std::vector<Item*>::iterator it;
	nalltasks = results.size();
	for (it = results.begin(); it!=results.end(); it++) // Task list cycle
	{
	    Item* ready_to_report = (*it)->findItem("ready_to_report");
	    if (ready_to_report != NULL)
	        ndonetasks++;
	    Item* active_task = (*it)->findItem("active_task");
	    if (active_task != NULL)
	    {
		nactivetasks++;
		if (active_task->findItem("active_task_state")->getivalue() == 1)
		    nruntasks++;
	    }
	    else
	    {
		if (ready_to_report == NULL)
		    nqueuetasks++;
	    }
	} // Task list cycle
	nothertasks = nalltasks-nruntasks-ndonetasks-nqueuetasks;
	needrefresh = true;
    }
    // ===Disk Data===
    if (srv->dusagedom.empty())
    {
	srv->statedom.releaseptr(tmpstatedom);
	return;
    }
    Item* tmpdusagedom = srv->dusagedom.hookptr();
    Item* disk_usage_summary = tmpdusagedom->findItem("disk_usage_summary");
    if (disk_usage_summary != NULL)
    {
	dtotal = disk_usage_summary->findItem("d_total")->getdvalue();
	dfree = disk_usage_summary->findItem("d_free")->getdvalue();
	dboinc = disk_usage_summary->findItem("d_boinc")->getdvalue();
	dallowed = disk_usage_summary->findItem("d_allowed")->getdvalue();
        std::vector<Item*> results = disk_usage_summary->getItems("project");
	std::vector<Item*>::iterator it;
	for (it = results.begin(); it!=results.end(); it++) // Task list cycle
	{
	    dboinc = dboinc + (*it)->findItem("disk_usage")->getdvalue();
	}
    }
    // ===Statistics===
    if (srv->statisticsdom.empty())
    {
	srv->statedom.releaseptr(tmpstatedom);
	srv->dusagedom.releaseptr(tmpdusagedom);
	return;
    }
    laststattime = srv->getlaststattime();
//    time_t predstattime = laststattime - (24*60*60); //предыдущий день
    usertotal = 0;
    useravg   = 0;
    hosttotal = 0;
    hostavg   = 0;
    lastdayuser = 0;
    lastdayhost = 0;
    double usertotallastday = 0;
    double hosttotallastday = 0;
    double usertotalpredday = 0;
    double hosttotalpredday = 0;

    projects.clear();
    Item* tmpstatisticsdom = srv->statisticsdom.hookptr();
    Item* statistics = tmpstatisticsdom->findItem("statistics");
    if (statistics!=NULL)
    {
	std::vector<Item*> project_statistics = statistics->getItems("project_statistics");
	std::vector<Item*>::iterator it;
	for (it = project_statistics.begin(); it!=project_statistics.end(); it++) // Project list cycle
	{
	    ProjectStat st; //заполнить эту структуру
	    st.name = srv->findProjectName(tmpstatedom,((*it)->findItem("master_url")->getsvalue()));
	    st.sstatus = "";
	    // Status line
	    if (!srv->statedom.empty())
	    {
		Item* project = srv->findprojectbyname(st.name.c_str());
		if (project != NULL)
		{
		    st.sstatus = st.sstatus + (project->findItem("suspended_via_gui")? "[Susp.] " : "");
		    st.sstatus = st.sstatus + (project->findItem("dont_request_more_work") ? "[N.N.Tsk.] " : "");
		}
	    }
	    std::vector<Item*> daily_statistics = (*it)->getItems("daily_statistics"); //все дни проекта в этом векторе
	    std::sort(daily_statistics.begin(), daily_statistics.end(), daily_statisticsCmpAbove); // Sort by descending dates
	    if (!daily_statistics.empty())
	    {
		Item* lastday = findDay(daily_statistics, laststattime); // Last day
		Item* predday = NULL;
//		time_t d = lastday->findItem("day")->getdvalue(); // Time from statistics
		if ( lastday != NULL) // Find last day for this project
		{
		    usertotallastday = usertotallastday + lastday->findItem("user_total_credit")->getdvalue();
		    hosttotallastday = hosttotallastday + lastday->findItem("host_total_credit")->getdvalue();
		    //берем предпоследний
		    if (daily_statistics.size() > 1)
			predday = daily_statistics[1];
		}
//		else //в этом проекте за последний день ничего нет
//		{
//		    predday = daily_statistics.front(); //последний день этого проекта учитываем как предыдущий
//		}
		// Accumupate stats for the previous day (if any)
		if (predday != NULL)
		{
		    usertotalpredday = usertotalpredday + predday->findItem("user_total_credit")->getdvalue();
		    hosttotalpredday = hosttotalpredday + predday->findItem("host_total_credit")->getdvalue();
		}
		// Summary for all days and projects
		Item* frontday = daily_statistics.front(); //берем последний несмотря на его дату
		if (daily_statistics.size() > 1)
		{
		    Item* predfrontday;
		    predfrontday = daily_statistics[1];

		    st.userlastday = frontday->findItem("user_total_credit")->getdvalue()-predfrontday->findItem("user_total_credit")->getdvalue();
		    st.hostlastday = frontday->findItem("host_total_credit")->getdvalue()-predfrontday->findItem("host_total_credit")->getdvalue();
		}
		st.laststattime = frontday->findItem("day")->getdvalue();
		usertotal = usertotal + frontday->findItem("user_total_credit")->getdvalue();
		useravg = useravg + frontday->findItem("user_expavg_credit")->getdvalue();
		hosttotal = hosttotal + frontday->findItem("host_total_credit")->getdvalue();
		hostavg = hostavg + frontday->findItem("host_expavg_credit")->getdvalue();

		st.user = frontday->findItem("user_total_credit")->getdvalue();
		st.host = frontday->findItem("host_total_credit")->getdvalue();
	    }
	    else
	    {
		st.user = 0;
		st.host = 0;
		st.laststattime = 0;
		st.userlastday = 0;
		st.hostlastday = 0;
	    }
	    projects.push_back(st); //вставляем статистику в вектор проектов
	} //проекты
	lastdayuser = usertotallastday - usertotalpredday;
	lastdayhost = hosttotallastday - hosttotalpredday;
	//сортируем проекты чтобы наиболее актуальные отображались первыми
	std::sort(projects.begin(), projects.end(), ProjectStat::CmpAbove);
    }
    srv->statisticsdom.releaseptr(tmpstatisticsdom);
    srv->statedom.releaseptr(tmpstatedom);
}


void InfoPanel::eventhandle(NEvent* ev) 	//обработчик событий
{
    NView::eventhandle(ev); //предок
    if ( ev->done )
	return;
    if ( ev->type == NEvent::evTIMER )
    {
	updatedata();	//запросить данные с сервера
	refresh(); 	//перерисовать окно
    }
}
