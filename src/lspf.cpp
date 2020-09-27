/* Compile with ::                                                                                                                                           */
/* g++ -O0 -std=c++11 -rdynamic -Wunused-variable -ltinfo -lncurses -lpanel -lboost_thread -lboost_filesystem -lboost_system -ldl -lpthread -o lspf lspf.cpp */

/*
  Copyright (c) 2015 Daniel John Erdos

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/


#include "lspf.h"

#include <ncurses.h>
#include <dlfcn.h>
#include <sys/utsname.h>

#include <locale>
#include <boost/date_time.hpp>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/filesystem.hpp>

#include <boost/exception/diagnostic_information.hpp>

boost::condition cond_appl ;
boost::condition cond_lspf ;
boost::condition cond_batch ;

#include "utilities.h"
#include "utilities.cpp"

#include "colours.h"

#include "classes.h"
#include "classes.cpp"

#include "pVPOOL.h"
#include "pVPOOL.cpp"

#include "pWidgets.h"
#include "pWidgets.cpp"

#include "pTable.h"
#include "pTable.cpp"

#include "pPanel.h"
#include "pPanel1.cpp"
#include "pPanel2.cpp"

#include "pApplication.h"
#include "pApplication.cpp"

#include "ispexeci.cpp"

#include "pLScreen.h"
#include "pLScreen.cpp"

#undef  MOD_NAME
#define MOD_NAME lspf

#define currScrn pLScreen::currScreen
#define OIA      pLScreen::OIA

#define CTRL(c) ((c) & 037)

namespace {

map<int, string> pfKeyDefaults = { {  1, "HELP"      },
				   {  2, "SPLIT NEW" },
				   {  3, "END"       },
				   {  4, "RETURN"    },
				   {  5, "RFIND"     },
				   {  6, "RCHANGE"   },
				   {  7, "UP"        },
				   {  8, "DOWN"      },
				   {  9, "SWAP"      },
				   { 10, "LEFT"      },
				   { 11, "RIGHT"     },
				   { 12, "RETRIEVE"  },
				   { 13, "HELP"      },
				   { 14, "SPLIT NEW" },
				   { 15, "END"       },
				   { 16, "RETURN"    },
				   { 17, "RFIND"     },
				   { 18, "RCHANGE"   },
				   { 19, "UP"        },
				   { 20, "DOWN"      },
				   { 21, "SWAP"      },
				   { 22, "SWAP PREV" },
				   { 23, "SWAP NEXT" },
				   { 24, "HELP"      } } ;

pApplication* currAppl ;

poolMGR*  p_poolMGR  = new poolMGR  ;
tableMGR* p_tableMGR = new tableMGR ;
logger*   lg         = new logger   ;
logger*   lgx        = new logger   ;

fPOOL funcPOOL ;

vector<pLScreen*> screenList ;

struct appInfo
{
	string file       ;
	string module     ;
	void* dlib        ;
	void* maker_ep    ;
	void* destroyer_ep ;
	bool  mainpgm     ;
	bool  dlopened    ;
	bool  errors      ;
	bool  relPending  ;
	int   refCount    ;
} ;

map<string, appInfo> apps ;

boost::circular_buffer<string> retrieveBuffer( 99 ) ;

int    linePosn  = 0 ;
int    maxTaskid = 0 ;
uint   retPos    = 0 ;
uint   priScreen = 0 ;
uint   altScreen = 0 ;
uint   intens    = 0 ;
string ctlAction     ;
string commandStack  ;
string jumpOption    ;
string returnOption  ;
bool   pfkeyPressed  ;
bool   ctlkeyPressed ;
bool   wmPending     ;

vector<pApplication*> pApplicationBackground ;
vector<pApplication*> pApplicationTimeout    ;

errblock err    ;

string zcommand ;
string zparm    ;

string zctverb  ;
string zcttrunc ;
string zctact   ;
string zctdesc  ;

unsigned int decColour1 ;
unsigned int decColour2 ;
unsigned int decIntens  ;

const char zscreen[] = { '1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G',
			 'H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W' } ;

string gmainpgm ;

enum LSPF_STATUS
{
	LSPF_STARTING,
	LSPF_RUNNING,
	LSPF_STOPPING
} ;

enum BACK_STATUS
{
	BACK_RUNNING,
	BACK_STOPPING,
	BACK_STOPPED
} ;

enum ZCT_VERBS
{
	ZCT_ACTIONS,
	ZCT_ALIAS,
	ZCT_CANCEL,
	ZCT_EXIT,
	ZCT_NOP,
	ZCT_PASSTHRU,
	ZCT_RETF,
	ZCT_RETP,
	ZCT_RETRIEVE,
	ZCT_SCRNAME,
	ZCT_SELECT,
	ZCT_SETVERB,
	ZCT_SPLIT,
	ZCT_SWAP,
	ZCT_USERID
} ;

enum ZCMD_TYPES
{
	ZC_DISCARD,
	ZC_FIELDEXC,
	ZC_MSGID,
	ZC_NOP,
	ZC_PANELID,
	ZC_REFRESH,
	ZC_RESIZE,
	ZC_TDOWN,
	ZC_DOT_ABEND,
	ZC_DOT_AUTO,
	ZC_DOT_ENQ,
	ZC_DOT_HIDE,
	ZC_DOT_INFO,
	ZC_DOT_LOAD,
	ZC_DOT_RELOAD,
	ZC_DOT_RGB,
	ZC_DOT_SCALE,
	ZC_DOT_SHELL,
	ZC_DOT_SHOW,
	ZC_DOT_SNAP,
	ZC_DOT_STATS,
	ZC_DOT_TASKS,
	ZC_DOT_TEST
} ;

map<string, ZCT_VERBS> zverbs =
{ { "ACTIONS",  ZCT_ACTIONS  },
  { "ALIAS",    ZCT_ALIAS    },
  { "CANCEL",   ZCT_CANCEL   },
  { "EXIT",     ZCT_EXIT     },
  { "NOP",      ZCT_NOP      },
  { "PASSTHRU", ZCT_PASSTHRU },
  { "RETF",     ZCT_RETF     },
  { "RETP",     ZCT_RETP     },
  { "RETRIEVE", ZCT_RETRIEVE },
  { "SCRNAME",  ZCT_SCRNAME  },
  { "SELECT",   ZCT_SELECT   },
  { "SETVERB",  ZCT_SETVERB  },
  { "SPLIT",    ZCT_SPLIT    },
  { "SWAP",     ZCT_SWAP     },
  { "USERID",   ZCT_USERID   } } ;

map<string, ZCMD_TYPES> zcommands =
{ { "DISCARD",  ZC_DISCARD    },
  { "FIELDEXC", ZC_FIELDEXC   },
  { "MSGID",    ZC_MSGID      },
  { "NOP",      ZC_NOP        },
  { "PANELID",  ZC_PANELID    },
  { "REFRESH",  ZC_REFRESH    },
  { "RESIZE",   ZC_RESIZE     },
  { "TDOWN",    ZC_TDOWN      },
  { ".ABEND",   ZC_DOT_ABEND  },
  { ".AUTO",    ZC_DOT_AUTO   },
  { ".ENQ",     ZC_DOT_ENQ    },
  { ".HIDE",    ZC_DOT_HIDE   },
  { ".INFO",    ZC_DOT_INFO   },
  { ".LOAD",    ZC_DOT_LOAD   },
  { ".RELOAD",  ZC_DOT_RELOAD },
  { ".RGB",     ZC_DOT_RGB    },
  { ".SCALE",   ZC_DOT_SCALE  },
  { ".SHELL",   ZC_DOT_SHELL  },
  { ".SHOW",    ZC_DOT_SHOW   },
  { ".SNAP",    ZC_DOT_SNAP   },
  { ".STATS",   ZC_DOT_STATS  },
  { ".TASKS",   ZC_DOT_TASKS  },
  { ".JOBS",    ZC_DOT_TASKS  },
  { ".TEST",    ZC_DOT_TEST   }  } ;

LSPF_STATUS lspfStatus ;
BACK_STATUS backStatus ;

boost::recursive_mutex mtx ;

WINDOW* lwin   = NULL ;
PANEL*  lpanel = NULL ;

}

unsigned int pLScreen::screensTotal = 0 ;
unsigned int pLScreen::maxScreenId  = 0 ;
unsigned int pLScreen::maxrow       = 0 ;
unsigned int pLScreen::maxcol       = 0 ;
unsigned int pLScreen::maxscrn      = ZMAXSCRN ;

bool pLScreen::busy = false ;

set<uint> pLScreen::screenNums ;
map<uint,uint> pLScreen::openedByList ;
boost::posix_time::ptime pLScreen::startTime ;
boost::posix_time::ptime pLScreen::endTime   ;

WINDOW* pLScreen::OIA       = NULL ;
PANEL*  pLScreen::OIA_panel = NULL ;

pLScreen* pLScreen::currScreen = NULL ;

tableMGR* pApplication::p_tableMGR = NULL ;
poolMGR*  pApplication::p_poolMGR  = NULL ;
logger*   pApplication::lg  = NULL ;
poolMGR*  pPanel::p_poolMGR = NULL ;
poolMGR*  abc::p_poolMGR    = NULL ;
logger*   pPanel::lg        = NULL ;
logger*   tableMGR::lg      = NULL ;
logger*   poolMGR::lg       = NULL ;

bool  pApplication::ControlNonDispl = false ;
bool  pApplication::ControlDisplayLock = false ;
bool  pApplication::ControlSplitEnable = true  ;
bool  pApplication::lineInOutDone = false ;

char  field::field_paduchar = ' ' ;
bool  field::field_nulls    = false ;
uint  pPanel::panel_intens  = 0   ;
uint  field::field_intens   = 0   ;
uint  text::text_intens     = 0   ;
uint  pdc::pdc_intens       = 0   ;
uint  abc::abc_intens       = 0   ;
uint  Box::box_intens       = 0   ;

void setGlobalClassVars() ;
void initialSetup()       ;
void loadDefaultPools()   ;
void setDefaultRGB()      ;
void getDynamicClasses()  ;
bool loadDynamicClass( const string& ) ;
bool unloadDynamicClass( void* )    ;
void reloadDynamicClasses( string ) ;
void loadSystemCommandTable() ;
void loadRetrieveBuffer() ;
void saveRetrieveBuffer() ;
void cleanup()            ;
void loadCUATables()      ;
void setGlobalColours()   ;
void setRGBValues()       ;
void setDecolourisedColour() ;
void decolouriseScreen()     ;
void setColourPair( const string& ) ;
void lScreenDefaultSettings() ;
void updateDefaultVars()      ;
void ncursesUpdate( uint, uint ) ;
void createSharedPoolVars( const string& ) ;
void updateReflist()          ;
void startApplication( selobj&, bool =false ) ;
void checkStartApplication( selobj& )         ;
void startApplicationBack( selobj, pApplication*, bool =true ) ;
void terminateApplication()     ;
void terminateApplicationBack( pApplication* ) ;
void abnormalTermMessage()      ;
void processBackgroundTasks()   ;
void ResumeApplicationAndWait() ;
bool createLogicalScreen()      ;
void deleteLogicalScreen()      ;
void resolvePGM( selobj&, pApplication* ) ;
void processPGMSelect()         ;
void processZSEL()              ;
void processAction( selobj& selct, uint row, uint col, int c, bool& doSelect, bool& passthru ) ;
void processZCOMMAND( selobj& selct, uint row, uint col, bool doSelect ) ;
void issueMessage( const string& ) ;
void lineOutput()     ;
void lineOutput_end() ;
void lineInput()      ;
void updateScreenText( set<uint>&, uint, uint ) ;
uint getTextLength( uint ) ;
string getScreenText( uint ) ;
void createLinePanel() ;
void deleteLinePanel() ;
void errorScreen( int, const string& ) ;
void errorScreen( const string&, const string& ) ;
void errorScreen( const string& ) ;
void abortStartup() ;
void lspfCallbackHandler( lspfCommand& ) ;
void createpfKeyDefaults() ;
string getEnvironmentVariable( const char* ) ;
void checkSystemVariable( const string& ) ;
string pfKeyValue( int )  ;
string ControlKeyAction( char c ) ;
string listLogicalScreens() ;
int  listInterruptOptions() ;
void actionSwap( const string& ) ;
void actionTabKey( uint&, uint& ) ;
void displayPullDown( const string& = "" ) ;
void executeFieldCommand( const string&, const fieldExc&, uint ) ;
void listBackTasks() ;
void listTaskVector( vector<pApplication*>& p ) ;
void autoUpdate()    ;
bool resolveZCTEntry( string&, string& ) ;
bool isActionKey( int c ) ;
bool isEscapeKey() ;
void listRetrieveBuffer() ;
int  getScreenNameNum( const string& ) ;
void serviceCallError( errblock& ) ;
void listErrorBlock( errblock& ) ;
void displayNotifies() ;
void mainLoop() ;


int main( void )
{
	selobj selct ;

	setGlobalClassVars() ;

	lg->open()  ;
	lgx->open() ;

	err.clear() ;

	boost::thread* pThread ;
	boost::thread* bThread ;

	commandStack = "" ;
	wmPending    = false ;
	lspfStatus   = LSPF_STARTING ;
	backStatus   = BACK_STOPPED  ;
	err.settask( 1 ) ;

	screenList.push_back( new pLScreen( 0 ) ) ;

	pLScreen::OIA_startTime() ;

	llog( "I", "lspf version " LSPF_VERSION " startup in progress" << endl ) ;

	llog( "I", "Calling initialSetup" << endl ) ;
	initialSetup() ;

	llog( "I", "Calling loadDefaultPools" << endl ) ;
	loadDefaultPools() ;

	lspfStatus = LSPF_RUNNING ;

	llog( "I", "Setting default RGB colour values" << endl ) ;
	setDefaultRGB() ;

	llog( "I", "Starting background job monitor task" << endl ) ;
	bThread = new boost::thread( &processBackgroundTasks ) ;

	lScreenDefaultSettings() ;

	llog( "I", "Calling getDynamicClasses" << endl ) ;
	getDynamicClasses() ;

	llog( "I", "Loading main "+ gmainpgm +" application" << endl ) ;
	if ( not loadDynamicClass( gmainpgm ) )
	{
		llog( "S", "Main program "+ gmainpgm +" cannot be loaded or symbols resolved." << endl ) ;
		cleanup() ;
		delete bThread ;
		return 0  ;
	}

	llog( "I", "Calling loadCUATables" << endl ) ;
	loadCUATables() ;

	llog( "I", "Calling loadSystemCommandTable" << endl ) ;
	loadSystemCommandTable() ;

	loadRetrieveBuffer() ;

	updateDefaultVars() ;

	llog( "I", "Startup complete.  Starting first "+ gmainpgm +" thread" << endl ) ;
	currAppl = ((pApplication*(*)())( apps[ gmainpgm ].maker_ep))() ;

	currScrn->application_add( currAppl ) ;

	selct.def( gmainpgm ) ;

	currAppl->init_phase1( selct, ++maxTaskid, lspfCallbackHandler ) ;
	currAppl->shrdPool = 1 ;

	p_poolMGR->createProfilePool( err, "ISR" ) ;
	p_poolMGR->connect( currAppl->taskid(), "ISR", 1 ) ;
	if ( err.RC4() ) { createpfKeyDefaults() ; }

	createSharedPoolVars( "ISR" ) ;
	currAppl->init_phase2() ;

	pThread = new boost::thread( &pApplication::run, currAppl ) ;
	currAppl->pThread = pThread ;
	apps[ gmainpgm ].refCount++ ;
	apps[ gmainpgm ].mainpgm = true ;

	llog( "I", "Waiting for "+ gmainpgm +" to complete startup" << endl ) ;
	boost::mutex mutex ;
	while ( currAppl->busyAppl )
	{
		boost::mutex::scoped_lock lk( mutex ) ;
		cond_lspf.wait_for( lk, boost::chrono::milliseconds( 100 ) ) ;
		lk.unlock() ;
	}
	if ( currAppl->terminateAppl )
	{
		errorScreen( 1, "An error has occured initialising the first "+ gmainpgm +" main task." ) ;
		llog( "S", "Main program "+ gmainpgm +" failed to initialise" << endl ) ;
		currAppl->info() ;
		p_poolMGR->disconnect( currAppl->taskid() ) ;
		llog( "I", "Removing application instance of "+ currAppl->get_appname() << endl ) ;
		((void (*)(pApplication*))(apps[ currAppl->get_appname() ].destroyer_ep))( currAppl ) ;
		delete pThread ;
		cleanup() ;
		delete bThread ;
		return 0  ;
	}

	llog( "I", "First thread "+ gmainpgm +" started and initialised.  ID=" << pThread->get_id() << endl ) ;

	currScrn->set_cursor( currAppl ) ;
	pLScreen::OIA_endTime() ;

	mainLoop() ;

	saveRetrieveBuffer() ;

	llog( "I", "Stopping background job monitor task" << endl ) ;
	lspfStatus = LSPF_STOPPING ;
	backStatus = BACK_STOPPING ;
	cond_batch.notify_one()    ;
	while ( backStatus != BACK_STOPPED )
	{
		boost::this_thread::sleep_for( boost::chrono::milliseconds( 5 ) ) ;
	}

	delete bThread ;

	llog( "I", "Closing ISPS profile as last application program has terminated" << endl ) ;
	p_poolMGR->destroySystemPool( err ) ;

	p_poolMGR->statistics()  ;
	p_tableMGR->statistics() ;

	delete p_poolMGR  ;
	delete p_tableMGR ;

	llog( "I", "Closing application log" << endl ) ;
	delete lgx ;

	for ( auto it = apps.begin() ; it != apps.end() ; ++it )
	{
		if ( it->second.dlopened )
		{
			llog( "I", "dlclose of "+ it->first +" at " << it->second.dlib << endl ) ;
			unloadDynamicClass( it->second.dlib ) ;
		}
	}

	llog( "I", "lspf and LOG terminating" << endl ) ;
	delete lg ;

	return 0  ;
}


void cleanup()
{
	// Cleanup resources for early termination and inform the user, including log names.

	delete p_poolMGR  ;
	delete p_tableMGR ;
	delete currScrn   ;

	llog( "I", "Stopping background job monitor task" << endl ) ;
	lspfStatus = LSPF_STOPPING ;
	backStatus = BACK_STOPPING ;
	cond_batch.notify_one()    ;
	while ( backStatus != BACK_STOPPED )
	{
		boost::this_thread::sleep_for( boost::chrono::milliseconds( 5 ) ) ;
	}

	cout << "*********************************************************************" << endl ;
	cout << "*********************************************************************" << endl ;
	cout << "Aborting startup of lspf.  Check lspf and application logs for errors" << endl ;
	cout << "lspf log name. . . . . :"<< lg->logname() << endl ;
	cout << "Application log name . :"<< lgx->logname() << endl ;
	cout << "*********************************************************************" << endl ;
	cout << "*********************************************************************" << endl ;
	cout << endl ;

	llog( "I", "lspf and LOG terminating" << endl ) ;
	delete lgx ;
	delete lg  ;
}


void mainLoop()
{
	llog( "I", "mainLoop() entered" << endl ) ;

	int c    ;

	uint row ;
	uint col ;

	bool passthru ;
	bool doSelect ;
	bool showLock = false ;
	bool Insert   = false ;

	selobj selct ;

	MEVENT event ;

	err.clear()  ;

	pLScreen::OIA_setup() ;

	mousemask( ALL_MOUSE_EVENTS, NULL ) ;

	set_escdelay( 25 ) ;

	while ( pLScreen::screensTotal > 0 )
	{
		pLScreen::clear_status() ;
		currScrn->get_cursor( row, col ) ;

		pfkeyPressed  = false ;
		ctlkeyPressed = false ;
		ctlAction     = ""    ;

		if ( commandStack == ""                &&
		     !pApplication::ControlDisplayLock &&
		     !pApplication::ControlNonDispl    &&
		     !pApplication::lineInOutDone )
		{
			currAppl->display_setmsg() ;
			currScrn->OIA_update( priScreen, altScreen, showLock ) ;
			pLScreen::clear_busy() ;
			ncursesUpdate( row, col ) ;
			c = getch() ;
			if ( c == 13 ) { c = KEY_ENTER ; }
		}
		else
		{
			if ( pApplication::ControlDisplayLock && not pApplication::lineInOutDone )
			{
				pLScreen::clear_busy() ;
				currScrn->OIA_update( priScreen, altScreen, showLock ) ;
				currAppl->display_setmsg() ;
				ncursesUpdate( row, col ) ;
			}
			c = KEY_ENTER ;
		}

		showLock = false ;

		if ( c < 256 && isprint( c ) )
		{
			if ( currAppl->inputInhibited() ) { continue ; }
			currAppl->currPanel->field_edit( row, col, char( c ), Insert, showLock ) ;
			currScrn->set_cursor( currAppl ) ;
			continue ;
		}

		if ( c == KEY_MOUSE && getmouse( &event ) == OK )
		{
			if ( event.bstate & BUTTON1_CLICKED || event.bstate & BUTTON1_DOUBLE_CLICKED )
			{
				row = event.y ;
				col = event.x ;
				if ( currAppl->currPanel->pd_active() )
				{
					currAppl->currPanel->display_current_pd( err, row, col ) ;
				}
				currScrn->set_cursor( row, col ) ;
				if ( event.bstate & BUTTON1_DOUBLE_CLICKED )
				{
					c = KEY_ENTER ;
				}
			}
		}

		if ( c >= CTRL( 'a' ) && c <= CTRL( 'z' ) && c != CTRL( 'i' ) )
		{
			ctlAction = ControlKeyAction( c ) ;
			ctlkeyPressed = true ;
			c = KEY_ENTER ;
		}

		switch( c )
		{

			case KEY_LEFT:
				currScrn->cursor_left() ;
				if ( currAppl->currPanel->pd_active() )
				{
					col = currScrn->get_col() ;
					currAppl->currPanel->display_current_pd( err, row, col ) ;
				}
				break ;

			case KEY_RIGHT:
				currScrn->cursor_right() ;
				if ( currAppl->currPanel->pd_active() )
				{
					col = currScrn->get_col() ;
					currAppl->currPanel->display_current_pd( err, row, col ) ;
				}
				break ;

			case KEY_UP:
				currScrn->cursor_up() ;
				if ( currAppl->currPanel->pd_active() )
				{
					row = currScrn->get_row() ;
					currAppl->currPanel->display_current_pd( err, row, col ) ;
				}
				break ;

			case KEY_DOWN:
				currScrn->cursor_down() ;
				if ( currAppl->currPanel->pd_active() )
				{
					row = currScrn->get_row() ;
					currAppl->currPanel->display_current_pd( err, row, col ) ;
				}
				break ;

			case CTRL( 'i' ):   // Tab key
				actionTabKey( row, col ) ;
				break ;

			case KEY_IC:
				Insert = not Insert ;
				currScrn->set_Insert( Insert ) ;
				break ;

			case KEY_HOME:
				if ( currAppl->currPanel->pd_active() )
				{
					currAppl->currPanel->get_pd_home( row, col ) ;
					currAppl->currPanel->display_current_pd( err, row, col ) ;
				}
				else
				{
					currAppl->get_home( row, col ) ;
				}
				currScrn->set_cursor( row, col ) ;
				break ;

			case KEY_DC:
				if ( currAppl->inputInhibited() ) { break ; }
				currAppl->currPanel->field_delete_char( row, col, showLock ) ;
				break ;

			case KEY_SDC:
				if ( currAppl->inputInhibited() ) { break ; }
				currAppl->currPanel->field_erase_eof( row, col, showLock ) ;
				break ;

			case KEY_END:
				currAppl->currPanel->cursor_eof( row, col ) ;
				currScrn->set_cursor( row, col ) ;
				break ;

			case 127:
			case KEY_BACKSPACE:
				if ( currAppl->inputInhibited() ) { break ; }
				currAppl->currPanel->field_backspace( row, col, showLock ) ;
				currScrn->set_cursor( row, col ) ;
				break ;

			// All action keys follow
			case KEY_F(1):  case KEY_F(2):  case KEY_F(3):  case KEY_F(4):  case KEY_F(5):  case KEY_F(6):
			case KEY_F(7):  case KEY_F(8):  case KEY_F(9):  case KEY_F(10): case KEY_F(11): case KEY_F(12):
			case KEY_F(13): case KEY_F(14): case KEY_F(15): case KEY_F(16): case KEY_F(17): case KEY_F(18):
			case KEY_F(19): case KEY_F(20): case KEY_F(21): case KEY_F(22): case KEY_F(23): case KEY_F(24):
				pfkeyPressed = true ;

			case KEY_NPAGE:
			case KEY_PPAGE:
			case KEY_ENTER:
			case CTRL( '[' ):       // Escape key
				pLScreen::OIA_startTime() ;
				debug1( "Action key pressed.  Processing" << endl ) ;
				if ( currAppl->msgInhibited() )
				{
					currAppl->msgResponseOK() ;
					break ;
				}
				Insert = false ;
				currScrn->set_Insert( Insert ) ;
				pLScreen::show_busy() ;
				updateDefaultVars()   ;
				processAction( selct, row, col, c, doSelect, passthru ) ;
				displayNotifies() ;
				if ( passthru )
				{
					updateReflist() ;
					currAppl->set_cursor( row, col ) ;
					ResumeApplicationAndWait()       ;
					if ( currAppl->selectPanel() )
					{
						processZSEL() ;
					}
					currScrn->set_cursor( currAppl ) ;
					while ( currAppl->terminateAppl )
					{
						terminateApplication() ;
						if ( pLScreen::screensTotal == 0 ) { return ; }
						if ( currAppl->SEL && !currAppl->terminateAppl )
						{
							processPGMSelect() ;
						}
					}
				}
				else
				{
					pApplication::ControlNonDispl = false ;
					processZCOMMAND( selct, row, col, doSelect ) ;
				}
				pLScreen::OIA_endTime() ;
				break ;

			default:
				debug1( "Action key "<<c<<" ("<<keyname( c )<<") ignored" << endl ) ;
		}
		decolouriseScreen() ;
	}
}


void setGlobalClassVars()
{
	pApplication::p_tableMGR = p_tableMGR ;
	pApplication::p_poolMGR  = p_poolMGR  ;
	pApplication::lg  = lgx       ;
	pPanel::p_poolMGR = p_poolMGR ;
	abc::p_poolMGR    = p_poolMGR ;
	pPanel::lg        = lgx       ;
	tableMGR::lg      = lgx       ;
	poolMGR::lg       = lgx       ;
}


void initialSetup()
{
	err.clear() ;

	funcPOOL.define( err, "ZCTVERB",  &zctverb  ) ;
	funcPOOL.define( err, "ZCTTRUNC", &zcttrunc ) ;
	funcPOOL.define( err, "ZCTACT",   &zctact   ) ;
	funcPOOL.define( err, "ZCTDESC",  &zctdesc  ) ;
}


void ncursesUpdate( uint row, uint col )
{
	wnoutrefresh( stdscr ) ;
	wnoutrefresh( OIA ) ;
	update_panels() ;
	move( row, col ) ;
	doupdate() ;
}


void processZSEL()
{
	// Called for a selection panel (ie. SELECT PANEL(ABC) function).
	// Use what's in ZSEL to start application

	int p ;

	string cmd ;
	string opt ;

	bool addpop = false ;

	selobj selct ;

	err.clear() ;

	currAppl->save_errblock()  ;
	cmd = currAppl->get_zsel() ;
	err = currAppl->get_errblock() ;
	currAppl->restore_errblock()   ;
	if ( err.error() )
	{
		serviceCallError( err ) ;
	}

	if ( cmd == "" ) { return ; }

	if ( upper( cmd ) == "EXIT" )
	{
		currAppl->set_userAction( USR_END ) ;
		p_poolMGR->put( err, "ZVERB", "END", SHARED ) ;
		ResumeApplicationAndWait() ;
		return ;
	}

	if ( cmd.compare( 0, 5, "PANEL" ) == 0 )
	{
		opt = currAppl->get_trail() ;
		if ( opt != "" ) { cmd += " OPT(" + opt + ")" ; }
	}

	p = wordpos( "ADDPOP", cmd ) ;
	if ( p > 0 )
	{
		addpop = true ;
		idelword( cmd, p, 1 ) ;
	}

	updateDefaultVars() ;
	currAppl->currPanel->remove_pd() ;

	if ( !selct.parse( err, cmd ) )
	{
		errorScreen( "Error in selection panel "+ currAppl->get_panelid(), "ZSEL = "+ cmd ) ;
		return ;
	}

	resolvePGM( selct, currAppl ) ;

	if ( addpop )
	{
		selct.parm += " ADDPOP" ;
	}

	selct.quiet = true ;
	startApplication( selct ) ;
	if ( selct.errors )
	{
		p_poolMGR->put( err, "ZVAL1", selct.pgm ,SHARED ) ;
		p_poolMGR->put( err, "ZERRDSC", "P", SHARED ) ;
		p_poolMGR->put( err, "ZERRSRC", "ZSEL = "+ cmd ,SHARED ) ;
		errorScreen( ( selct.rsn == 998 ) ? "PSYS012W" : "PSYS013H" ) ;
	}
}


void processAction( selobj& selct, uint row, uint col, int c, bool& doSelect, bool& passthru )
{
	// Return if application is just doing line input/output
	// perform lspf high-level functions - pfkey -> command
	// application/user/site/system command table entry?
	// BUILTIN command
	// System command
	// RETRIEVE/RETF
	// Jump command entered
	// !abc run abc as a program
	// @abc run abc as a REXX procedure
	// Else pass event to application

	size_t p1 ;

	uint rw ;
	uint cl ;
	uint rtsize ;
	uint rbsize ;

	bool addRetrieve ;
	bool fromStack   = false ;
	bool nested      = true  ;

	string cmdVerb ;
	string cmdParm ;
	string cbuffer ;
	string pfcmd   ;
	string delm    ;
	string aVerb   ;
	string msg     ;
	string w1      ;
	string t       ;

	boost::circular_buffer<string>::iterator itt ;

	err.clear() ;

	pdc t_pdc ;

	doSelect  = false ;
	passthru  = true  ;
	pfcmd     = ""    ;
	zcommand  = ""    ;

	if ( pApplication::lineInOutDone ) { return ; }

	currAppl->set_userAction( USR_ENTER ) ;

	if ( c == CTRL( '[' ) )
	{
		if ( currAppl->currPanel->pd_active() )
		{
			currAppl->clear_msg() ;
			if ( currAppl->currPanel->cursor_on_pulldown( row, col ) )
			{
				currAppl->set_cursor_home() ;
				currScrn->set_cursor( currAppl ) ;
			}
			currAppl->currPanel->remove_pd() ;
		}
		else
		{
			actionSwap( "LIST" ) ;
		}
		passthru = false ;
		zcommand = "NOP" ;
		return ;
	}

	if ( c == KEY_ENTER && !ctlkeyPressed )
	{
		if ( wmPending )
		{
			currAppl->clear_msg() ;
			currScrn->save_panel_stack() ;
			currAppl->movepop( row, col ) ;
			currScrn->restore_panel_stack() ;
			wmPending = false ;
			passthru  = false ;
			zcommand  = "NOP" ;
			currScrn->set_cursor( currAppl ) ;
			currAppl->display_pd( err ) ;
			currAppl->display_id() ;
			return ;
		}
		else if ( currAppl->currPanel->on_border_line( row, col ) )
		{
			issueMessage( "PSYS015" ) ;
			zcommand  = "NOP" ;
			wmPending = true  ;
			passthru  = false ;
			return ;
		}
		else if ( currAppl->currPanel->hide_msg_window( row, col ) )
		{
			zcommand  = "NOP" ;
			passthru  = false ;
			return ;
		}
		else if ( currAppl->currPanel->jump_field( row, col, t ) )
		{
			currAppl->currPanel->cmd_setvalue( t ) ;
		}
		else if ( currAppl->currPanel->cmd_getvalue() == "" &&
			  currAppl->currPanel->on_abline( row ) )
		{
			if ( currAppl->currPanel->display_pd( err, row+2, col, msg ) )
			{
				if ( msg != "" )
				{
					issueMessage( msg ) ;
				}
				passthru = false ;
				currScrn->set_cursor( currAppl ) ;
				zcommand = "NOP" ;
				return ;
			}
			else if ( err.error() )
			{
				errorScreen( 1, "Error processing pull-down menu." ) ;
				serviceCallError( err ) ;
				currAppl->set_cursor_home() ;
				currScrn->set_cursor( currAppl ) ;
				return ;
			}
		}
		else if ( currAppl->currPanel->pd_active() )
		{
			if ( !currAppl->currPanel->cursor_on_pulldown( row, col ) )
			{
				currAppl->currPanel->remove_pd() ;
				currAppl->clear_msg() ;
				zcommand = "NOP" ;
				passthru = false ;
				return ;
			}
			t_pdc = currAppl->currPanel->retrieve_choice( err, msg ) ;
			if ( t_pdc.pdc_inact )
			{
				msg = "PSYS012T" ;
			}
			if ( msg != "" )
			{
				issueMessage( msg ) ;
				zcommand = "NOP" ;
				passthru = false ;
				return ;
			}
			else if ( t_pdc.pdc_run != "" )
			{
				currAppl->currPanel->remove_pd() ;
				currAppl->clear_msg() ;
				zcommand = t_pdc.pdc_run ;
				nested   = false ;
				if ( zcommand == "ISRROUTE" )
				{
					cmdVerb = word( t_pdc.pdc_parm, 1 )    ;
					cmdParm = subword( t_pdc.pdc_parm, 2 ) ;
					if ( cmdVerb == "SELECT" )
					{
						if ( !selct.parse( err, cmdParm ) )
						{
							llog( "E", "Error in SELECT command "+ t_pdc.pdc_parm << endl ) ;
							currAppl->setmsg( "PSYS011K" ) ;
							return ;
						}
						resolvePGM( selct, currAppl ) ;
						doSelect = true  ;
						passthru = false ;
						return ;
					}
				}
				else
				{
					zcommand += " " + t_pdc.pdc_parm ;
				}
			}
			else
			{
				currAppl->currPanel->remove_pd() ;
				currAppl->clear_msg() ;
				zcommand = "" ;
				return ;
			}
		}
	}

	if ( wmPending )
	{
		wmPending = false ;
		currAppl->clear_msg() ;
	}

	addRetrieve = true ;
	delm        = p_poolMGR->sysget( err, "ZDEL", PROFILE ) ;

	if ( t_pdc.pdc_run == "" )
	{
		currAppl->currPanel->point_and_shoot( row, col ) ;
		zcommand = strip( currAppl->currPanel->cmd_getvalue() ) ;
	}

	if ( c == KEY_ENTER  &&
	     zcommand != ""  &&
	     currAppl->currPanel->has_command_field() &&
	     p_poolMGR->sysget( err, "ZSWAPC", PROFILE ) == zcommand.substr( 0, 1 ) &&
	     p_poolMGR->sysget( err, "ZSWAP",  PROFILE ) == "Y" )
	{
		currAppl->currPanel->field_get_row_col( currAppl->currPanel->cmdfield, rw, cl ) ;
		if ( rw == row && cl < col )
		{
			passthru = false ;
			currAppl->currPanel->cmd_setvalue( "" ) ;
			actionSwap( upper( zcommand.substr( 1, col-cl-1 ) ) ) ;
			return ;
		}
	}

	if ( ctlkeyPressed )
	{
		pfcmd = ctlAction ;
	}

	if ( commandStack != "" )
	{
		if ( zcommand != "" )
		{
			if ( currAppl->currPanel->cmd_nonblank() )
			{
				passthru  = false ;
				zcommand += commandStack ;
				currAppl->currPanel->cmd_setvalue( zcommand ) ;
				commandStack = "" ;
				return ;
			}
			else
			{
				currAppl->currPanel->cmd_setvalue( zcommand ) ;
				if ( zcommand.front() != '&' && currAppl->simulate_enter() )
				{
					return ;
				}
			}
		}
		else if ( currAppl->currPanel->cmd_nonblank() )
		{
			passthru = false ;
			zcommand = "" ;
			commandStack = "" ;
			return ;
		}
		if ( not currAppl->propagateEnd )
		{
			fromStack = true ;
			zcommand  = commandStack ;
			commandStack = "" ;
		}
		addRetrieve = false ;
	}

	if ( pfkeyPressed )
	{
		if ( p_poolMGR->sysget( err, "ZKLUSE", PROFILE ) == "Y" )
		{
			currAppl->save_errblock() ;
			currAppl->reload_keylist( currAppl->currPanel ) ;
			err = currAppl->get_errblock() ;
			currAppl->restore_errblock()   ;
			if ( err.error() )
			{
				serviceCallError( err ) ;
			}
			else
			{
				pfcmd = currAppl->currPanel->get_keylist( c ) ;
			}
		}
		if ( pfcmd == "" )
		{
			pfcmd = pfKeyValue( c ) ;
		}
		t = "PF" + d2ds( c - KEY_F( 0 ), 2 ) ;
		p_poolMGR->put( err, "ZPFKEY", t, SHARED, SYSTEM ) ;
		debug1( "PF Key pressed " <<t<<" value "<< pfcmd << endl ) ;
		currAppl->currPanel->set_pfpressed( t ) ;
	}
	else
	{
		p_poolMGR->put( err, "ZPFKEY", "PF00", SHARED, SYSTEM ) ;
		currAppl->currPanel->set_pfpressed() ;
	}

	if ( addRetrieve )
	{
		rbsize = ds2d( p_poolMGR->sysget( err, "ZRBSIZE", PROFILE ) ) ;
		rtsize = ds2d( p_poolMGR->sysget( err, "ZRTSIZE", PROFILE ) ) ;
		if ( retrieveBuffer.capacity() != rbsize )
		{
			retrieveBuffer.rset_capacity( rbsize ) ;
		}
		if (  zcommand.size() >= rtsize &&
		     !findword( word( upper( zcommand ), 1 ), "RETRIEVE RETF RETP" ) &&
		     !findword( word( upper( pfcmd ), 1 ), "RETRIEVE RETF RETP" ) )
		{
			itt = find( retrieveBuffer.begin(), retrieveBuffer.end(), zcommand ) ;
			if ( itt != retrieveBuffer.end() )
			{
				retrieveBuffer.erase( itt ) ;
			}
			retrieveBuffer.push_front( zcommand ) ;
			retPos = 0 ;
		}
	}

	switch( c )
	{
		case KEY_PPAGE:
			pfcmd = "UP" ;
			p_poolMGR->put( err, "ZPFKEY", "PF25", SHARED, SYSTEM ) ;
			break ;

		case KEY_NPAGE:
			pfcmd = "DOWN" ;
			p_poolMGR->put( err, "ZPFKEY", "PF26", SHARED, SYSTEM ) ;
			break ;
	}

	if ( pfcmd != "" ) { zcommand = pfcmd + " " + zcommand ; }

	if ( zcommand.compare( 0, 2, delm+delm ) == 0 )
	{
		commandStack = zcommand.substr( 1 ) ;
		zcommand     = "" ;
		currAppl->currPanel->cmd_setvalue( "" ) ;
		return ;
	}
	else if ( zcommand.compare( 0, 1, delm ) == 0 )
	{
		zcommand.erase( 0, 1 ) ;
		currAppl->currPanel->cmd_setvalue( zcommand ) ;
	}

	p1 = zcommand.find( delm.front() ) ;
	if ( p1 != string::npos )
	{
		commandStack = zcommand.substr( p1 ) ;
		if ( commandStack == delm ) { commandStack = "" ; }
		zcommand.erase( p1 ) ;
		currAppl->currPanel->cmd_setvalue( zcommand ) ;
	}

	cmdVerb = upper( word( zcommand, 1 ) ) ;
	if ( cmdVerb == "" ) { retPos = 0 ; return ; }

	cmdParm = subword( zcommand, 2 ) ;

	if ( cmdVerb.front() == '>' )
	{
		zcommand.erase( 0, 1 ) ;
		currAppl->currPanel->cmd_setvalue( zcommand ) ;
		return ;
	}
	else if ( cmdVerb.front() == '@' )
	{
		selct.clear() ;
		currAppl->vcopy( "ZOREXPGM", selct.pgm, MOVE ) ;
		selct.parm    = zcommand.substr( 1 ) ;
		selct.newappl = ""    ;
		selct.newpool = true  ;
		selct.passlib = false ;
		selct.suspend = true  ;
		selct.fstack  = fromStack ;
		doSelect      = true  ;
		passthru      = false ;
		currAppl->currPanel->cmd_setvalue( "" ) ;
		return ;
	}
	else if ( cmdVerb.front() == '!' )
	{
		selct.clear() ;
		selct.pgm     = cmdVerb.substr( 1 ) ;
		selct.parm    = cmdParm ;
		selct.newappl = ""      ;
		selct.newpool = true    ;
		selct.passlib = false   ;
		selct.suspend = true    ;
		selct.fstack  = fromStack ;
		doSelect      = true    ;
		passthru      = false   ;
		currAppl->currPanel->cmd_setvalue( "" ) ;
		return ;
	}

	if ( zcommand.size() > 1 && zcommand.front() == '=' && ( pfcmd == "" || pfcmd == "RETURN" ) )
	{
		zcommand.erase( 0, 1 ) ;
		jumpOption = zcommand + commandStack ;
		if ( !currAppl->isprimMenu() )
		{
			currAppl->jumpEntered = true ;
			currAppl->set_userAction( USR_RETURN ) ;
			p_poolMGR->put( err, "ZVERB", "RETURN", SHARED ) ;
			currAppl->currPanel->cmd_setvalue( "" ) ;
			return ;
		}
		currAppl->currPanel->cmd_setvalue( zcommand ) ;
	}

	if ( cmdVerb == "HELP" )
	{
		if ( currAppl->show_help_member() )
		{
			zparm   = currAppl->get_help_member( row, col ) ;
			cmdParm = zparm ;
		}
		else
		{
			zcommand = "NOP" ;
			passthru = false ;
			currAppl->currPanel->showLMSG = true ;
			currAppl->currPanel->display_msg( err ) ;
			currAppl->currPanel->reset_cmd() ;
			return ;
		}
	}

	aVerb = cmdVerb ;

	if ( resolveZCTEntry( cmdVerb, cmdParm ) )
	{
		if ( currAppl->currPanel->is_cmd_inactive( cmdVerb ) )
		{
			p_poolMGR->put( err, "ZCTMVAR", left( aVerb, 8 ), SHARED ) ;
			issueMessage( "PSYS011" ) ;
			currAppl->currPanel->cmd_setvalue( "" ) ;
			zcommand = "NOP" ;
			passthru = false ;
			return ;
		}
		auto it = zverbs.find( word( zctact, 1 ) ) ;
		switch ( it->second )
		{
		case ZCT_ACTIONS:
			displayPullDown( iupper( cmdParm ) ) ;
			passthru = false ;
			zcommand = "NOP" ;
			break ;

		case ZCT_CANCEL:
			if ( currAppl->currPanel->pd_active() &&
			     currAppl->currPanel->cursor_on_pulldown( row, col ) )
			{
				currAppl->currPanel->remove_pd() ;
				currAppl->clear_msg() ;
				zcommand = "" ;
				return ;
			}
			else
			{
				currAppl->set_userAction( USR_CANCEL ) ;
				p_poolMGR->put( err, "ZVERB", "CANCEL", SHARED ) ;
				zcommand = cmdParm ;
			}
			break ;

		case ZCT_EXIT:
			currAppl->set_userAction( USR_EXIT ) ;
			p_poolMGR->put( err, "ZVERB", "EXIT", SHARED ) ;
			zcommand = cmdParm ;
			break ;

		case ZCT_NOP:
			p_poolMGR->put( err, "ZCTMVAR", left( aVerb, 8 ), SHARED ) ;
			issueMessage( "PSYS011" ) ;
			currAppl->currPanel->cmd_setvalue( "" ) ;
			passthru = false ;
			return ;

		case ZCT_PASSTHRU:
			passthru = true ;
			zcommand = strip( cmdVerb + " " + cmdParm ) ;
			break ;

		case ZCT_RETP:
			currAppl->currPanel->cmd_setvalue( "" ) ;
			listRetrieveBuffer() ;
			passthru = false ;
			zcommand = "NOP" ;
			return ;

		case ZCT_RETRIEVE:
		case ZCT_RETF:
			if ( !currAppl->currPanel->has_command_field() ) { return ; }
			if ( commandStack == "" && datatype( cmdParm, 'W' ) )
			{
				p1 = ds2d( cmdParm ) ;
				if ( p1 > 0 && p1 <= retrieveBuffer.size() ) { retPos = p1 - 1 ; }
			}
			commandStack = ""    ;
			zcommand     = "NOP" ;
			passthru     = false ;
			if ( !retrieveBuffer.empty() )
			{
				if ( it->second == ZCT_RETF )
				{
					retPos = ( retPos < 2 ) ? retrieveBuffer.size() : retPos - 1 ;
				}
				else
				{
					if ( ++retPos > retrieveBuffer.size() ) { retPos = 1 ; }
				}
				currAppl->currPanel->cmd_setvalue( retrieveBuffer[ retPos-1 ] ) ;
				currAppl->currPanel->cursor_to_cmdfield( retrieveBuffer[ retPos-1 ].size() + 1 ) ;
			}
			else
			{
				currAppl->currPanel->cmd_setvalue( "" ) ;
				currAppl->currPanel->cursor_to_cmdfield() ;
			}
			currScrn->set_cursor( currAppl ) ;
			currAppl->currPanel->remove_pd() ;
			currAppl->clear_msg() ;
			return ;

		case ZCT_SCRNAME:
			iupper( cmdParm ) ;
			w1      = word( cmdParm, 2 ) ;
			cmdParm = word( cmdParm, 1 ) ;
			if  ( cmdParm == "ON" )
			{
				p_poolMGR->put( err, "ZSCRNAM1", "ON", SHARED, SYSTEM ) ;
			}
			else if ( cmdParm == "OFF" )
			{
				p_poolMGR->put( err, "ZSCRNAM1", "OFF", SHARED, SYSTEM ) ;
			}
			else if ( isvalidName( cmdParm ) && !findword( cmdParm, "LIST PREV NEXT" ) )
			{
				p_poolMGR->put( err, currScrn->screenid(), "ZSCRNAME", cmdParm ) ;
				p_poolMGR->put( err, "ZSCRNAME", cmdParm, SHARED ) ;
				if ( w1 == "PERM" )
				{
					p_poolMGR->put( err, currScrn->screenid(), "ZSCRNAM2", w1 ) ;
				}
				else
				{
					p_poolMGR->put( err, currScrn->screenid(), "ZSCRNAM2", "" ) ;
				}
			}
			else
			{
				issueMessage( "PSYS013" ) ;
			}
			currAppl->display_id() ;
			passthru = false ;
			zcommand = "" ;
			break  ;

		case ZCT_SELECT:
			if ( !selct.parse( err, subword( zctact, 2 ) ) )
			{
				llog( "E", "Error in SELECT command "+ zctact << endl ) ;
				currAppl->setmsg( "PSYS011K" ) ;
				return ;
			}
			p1 = selct.parm.find( "&ZPARM" ) ;
			if ( p1 != string::npos )
			{
				selct.parm.replace( p1, 6, cmdParm ) ;
			}
			resolvePGM( selct, currAppl ) ;
			zcommand     = "" ;
			doSelect     = true  ;
			selct.nested = nested ;
			selct.fstack = fromStack ;
			passthru     = false ;
			break ;

		case ZCT_SETVERB:
			p_poolMGR->put( err, "ZVERB", word( zctverb, 1 ), SHARED ) ;
			if ( err.error() )
			{
				llog( "S", "VPUT for ZVERB failed" << endl ) ;
				listErrorBlock( err ) ;
			}
			zcommand = subword( zcommand, 2 ) ;
			if ( zctverb == "NRETRIEV" )
			{
				selct.clear() ;
				currAppl->vcopy( "ZRFLPGM", selct.pgm, MOVE ) ;
				selct.parm = "NR1 " + cmdParm ;
				doSelect   = true  ;
				passthru   = false ;
			}
			else if ( zctverb == "RETURN" )
			{
				currAppl->set_userAction( USR_RETURN ) ;
				returnOption = commandStack ;
				commandStack = "" ;
			}
			else if ( zctverb == "END" )
			{
				currAppl->set_userAction( USR_END ) ;
			}
			break ;

		case ZCT_SPLIT:
			if ( not currAppl->currPanel->keep_cmd() )
			{
				currAppl->clear_msg() ;
				currAppl->currPanel->cmd_setvalue( "" ) ;
				currAppl->currPanel->cursor_to_cmdfield() ;
				currScrn->set_cursor( currAppl ) ;
			}
			selct.def( gmainpgm ) ;
			startApplication( selct, true ) ;
			passthru = false ;
			zcommand = "NOP" ;
			return ;

		case ZCT_SWAP:
			iupper( cmdParm ) ;
			actionSwap( cmdParm ) ;
			passthru = false ;
			zcommand = "NOP" ;
			return ;

		case ZCT_USERID:
			iupper( cmdParm ) ;
			if ( cmdParm == "" )
			{
				w1 = p_poolMGR->get( err, currScrn->screenid(), "ZSHUSRID" ) ;
				cmdParm = ( w1 == "Y" ) ? "OFF" : "ON" ;
			}
			if ( cmdParm == "ON" )
			{
				p_poolMGR->put( err, currScrn->screenid(), "ZSHUSRID", "Y" ) ;
			}
			else if ( cmdParm == "OFF" )
			{
				p_poolMGR->put( err, currScrn->screenid(), "ZSHUSRID", "N" ) ;
			}
			else
			{
				issueMessage( "PSYS012F" ) ;
			}
			currAppl->display_id() ;
			passthru = false ;
			zcommand = "" ;
			break ;

		}
	}

	retPos = 0 ;

	if ( zcommands.find( cmdVerb ) != zcommands.end() )
	{
		passthru = false   ;
		zcommand = cmdVerb ;
		zparm    = upper( cmdParm ) ;
	}

	if ( currAppl->currPanel->pd_active() && zctverb == "END" )
	{
		currAppl->currPanel->remove_pd() ;
		currAppl->clear_msg() ;
		zcommand = ""    ;
		passthru = false ;
	}

	currAppl->currPanel->cmd_setvalue( passthru ? zcommand : "" ) ;
	debug1( "Primary command '"+ zcommand +"'  Passthru = " << passthru << endl ) ;
}


void processZCOMMAND( selobj& selct, uint row, uint col, bool doSelect )
{
	// If the event is not being passed to the application, process ZCOMMAND or start application request

	short int r ;
	short int g ;
	short int b ;

	string w1 ;
	string w2 ;
	string field_name ;

	fieldExc fxc ;

	auto it = zcommands.find( zcommand ) ;

	if ( it == zcommands.end() )
	{
		currAppl->set_cursor_home() ;
		currScrn->set_cursor( currAppl ) ;
		if ( doSelect )
		{
			updateDefaultVars() ;
			currAppl->currPanel->remove_pd() ;
			startApplication( selct ) ;
		}
		return ;
	}

	switch ( it->second )
	{
	case ZC_DISCARD:
		currAppl->currPanel->refresh_fields( err ) ;
		break  ;

	case ZC_FIELDEXC:
		field_name = currAppl->currPanel->field_getname( row, col ) ;
		if ( field_name == "" )
		{
			issueMessage( "PSYS012K" ) ;
			return ;
		}
		fxc = currAppl->currPanel->field_getexec( field_name ) ;
		if ( fxc.fieldExc_command != "" )
		{
			executeFieldCommand( field_name, fxc, col ) ;
		}
		else
		{
			issueMessage( "PSYS012J" ) ;
		}
		return ;

	case ZC_MSGID:
		if ( zparm == "" )
		{
			w1 = p_poolMGR->get( err, currScrn->screenid(), "ZMSGID" ) ;
			p_poolMGR->put( err, "ZMSGID", w1, SHARED, SYSTEM ) ;
			issueMessage( "PSYS012L" ) ;
		}
		else if ( zparm == "ON" )
		{
			p_poolMGR->put( err, currScrn->screenid(), "ZSHMSGID", "Y" ) ;
		}
		else if ( zparm == "OFF" )
		{
			p_poolMGR->put( err, currScrn->screenid(), "ZSHMSGID", "N" ) ;
		}
		else
		{
			issueMessage( "PSYS012M" ) ;
		}
		break  ;

	case ZC_NOP:
		return ;

	case ZC_PANELID:
		if ( zparm == "" )
		{
			w1 = p_poolMGR->get( err, currScrn->screenid(), "ZSHPANID" ) ;
			zparm = ( w1 == "Y" ) ? "OFF" : "ON" ;
		}
		if ( zparm == "ON" )
		{
			p_poolMGR->put( err, currScrn->screenid(), "ZSHPANID", "Y" ) ;
		}
		else if ( zparm == "OFF" )
		{
			p_poolMGR->put( err, currScrn->screenid(), "ZSHPANID", "N" ) ;
		}
		else
		{
			issueMessage( "PSYS014" ) ;
		}
		currAppl->display_id() ;
		break  ;

	case ZC_REFRESH:
		currScrn->refresh_panel_stack() ;
		pLScreen::OIA_refresh() ;
		redrawwin( stdscr ) ;
		break  ;

	case ZC_RESIZE:
		currAppl->toggle_fscreen() ;
		break  ;

	case ZC_TDOWN:
		currAppl->currPanel->field_tab_down( row, col ) ;
		currScrn->set_cursor( row, col ) ;
		return ;

	case ZC_DOT_ABEND:
		currAppl->set_forced_abend() ;
		ResumeApplicationAndWait()   ;
		if ( currAppl->abnormalEnd )
		{
			terminateApplication() ;
			if ( pLScreen::screensTotal == 0 ) { return ; }
		}
		return ;

	case ZC_DOT_AUTO:
		currAppl->set_cursor( row, col ) ;
		autoUpdate() ;
		return ;

	case ZC_DOT_ENQ:
		currAppl->show_enqueues() ;
		break ;

	case ZC_DOT_HIDE:
		if ( zparm == "NULLS" )
		{
			p_poolMGR->sysput( err, "ZNULLS", "NO", SHARED ) ;
			field::field_nulls = false ;
			currAppl->currPanel->redraw_fields( err ) ;
		}
		else
		{
			issueMessage( "PSYS012R" ) ;
		}
		break  ;

	case ZC_DOT_INFO:
		currAppl->info() ;
		break  ;

	case ZC_DOT_LOAD:
		for ( auto ita = apps.begin() ; ita != apps.end() ; ++ita )
		{
			if ( !ita->second.errors && !ita->second.dlopened )
			{
				loadDynamicClass( ita->first ) ;
			}
		}
		break  ;

	case ZC_DOT_RELOAD:
		reloadDynamicClasses( zparm ) ;
		break  ;

	case ZC_DOT_RGB:

		llog( "I", ".RGB" <<endl ) ;
		llog( "-", "**************************************" <<endl ) ;
		llog( "-", "Listing current RGB values:" <<endl) ;
		for ( int i = 1 ; i < COLORS ; ++i )
		{
			color_content( i, &r, &g, &b ) ;
			llog( "-", "   Colour "<< i << " R: " << d2ds( r, 4 ) <<
						       " G: " << d2ds( g, 4 ) <<
						       " B: " << d2ds( b, 4 ) <<endl ) ;
		}
		llog( "-", "**************************************" <<endl ) ;
		break  ;

	case ZC_DOT_SCALE:
		if ( zparm == "" ) { zparm = "ON" ; }
		if ( findword( zparm, "ON OFF" ) )
		{
			p_poolMGR->sysput( err, "ZSCALE", zparm, SHARED ) ;
		}
		break  ;

	case ZC_DOT_SHELL:
		w1 = p_poolMGR->sysget( err, "ZSHELL", SHARED ) ;
		if ( w1 == "" )
		{
			issueMessage( "PSYS013E" ) ;
			break ;
		}
		def_prog_mode()   ;
		endwin()          ;
		system( w1.c_str() ) ;
		reset_prog_mode() ;
		refresh()         ;
		break  ;

	case ZC_DOT_SHOW:
		if ( zparm == "NULLS" )
		{
			p_poolMGR->sysput( err, "ZNULLS", "YES", SHARED ) ;
			field::field_nulls = true ;
			currAppl->currPanel->redraw_fields( err ) ;
		}
		else
		{
			issueMessage( "PSYS013A" ) ;
		}
		break  ;

	case ZC_DOT_STATS:
		p_poolMGR->statistics() ;
		p_tableMGR->statistics() ;
		break  ;

	case ZC_DOT_SNAP:
		p_poolMGR->snap() ;
		break  ;

	case ZC_DOT_TASKS:
		listBackTasks() ;
		break  ;

	case ZC_DOT_TEST:
		currAppl->setTestMode() ;
		llog( "W", "Application is now running in test mode" << endl ) ;
		break  ;
	}

	currAppl->set_cursor_home() ;
	currScrn->set_cursor( currAppl ) ;
}


bool resolveZCTEntry( string& cmdVerb, string& cmdParm )
{
	// Search for command tables in ZTLIB.
	// System commands should be in ZUPROF but user/site command tables might not be.

	// Do not try to load the application command table as this will have been loaded
	// during SELECT processing if it exists.  This is always the first in the list of command tables.

	// ALIAS parameters in the command table take precedence over command line parameters.

	int i ;

	size_t j ;

	bool found = false ;

	string ztlib   ;
	string cmdtabl ;

	vector<string>cmdtabls ;
	set<string>processed   ;

	errblock err1 ;

	zctverb  = "" ;
	zcttrunc = "" ;
	zctact   = "" ;
	zctdesc  = "" ;

	cmdtabl = ( currAppl->get_applid() == "ISP" ) ? "N/A" : currAppl->get_applid() ;
	cmdtabls.push_back( cmdtabl ) ;
	processed.insert( cmdtabl ) ;

	for ( i = 1 ; i < 4 ; ++i )
	{
		cmdtabl = p_poolMGR->sysget( err, "ZUCMDT" + d2ds( i ), PROFILE ) ;
		if ( cmdtabl != "" && processed.count( cmdtabl ) == 0 )
		{
			cmdtabls.push_back( cmdtabl ) ;
		}
		processed.insert( cmdtabl ) ;
	}

	if ( p_poolMGR->sysget( err, "ZSCMDTF", PROFILE ) != "Y" )
	{
		  cmdtabls.push_back( "ISP" ) ;
		  processed.insert( "ISP" ) ;
	}

	for ( i = 1 ; i < 4 ; ++i )
	{
		cmdtabl = p_poolMGR->sysget( err, "ZSCMDT" + d2ds( i ), PROFILE ) ;
		if ( cmdtabl != "" && processed.count( cmdtabl ) == 0 )
		{
			cmdtabls.push_back( cmdtabl ) ;
		}
		processed.insert( cmdtabl ) ;
	}

	if ( processed.count( "ISP" ) == 0 )
	{
		  cmdtabls.push_back( "ISP" ) ;
	}

	ztlib = p_poolMGR->sysget( err, "ZTLIB", PROFILE ) ;

	for ( i = 0 ; i < 256 ; ++i )
	{
		for ( j = 0 ; j < cmdtabls.size() ; ++j )
		{
			err1.clear() ;
			p_tableMGR->cmdsearch( err1, funcPOOL, cmdtabls[ j ], cmdVerb, ztlib, ( j > 0 ) ) ;
			if ( err1.error() )
			{
				llog( "E", "Error received searching for command "+ cmdVerb << endl ) ;
				llog( "E", "Table name : "+ cmdtabls[ j ] << endl ) ;
				llog( "E", "Path list  : "+ ztlib << endl ) ;
				listErrorBlock( err1 ) ;
				continue ;
			}
			if ( !err1.RC0() || zctact == "" ) { continue ; }
			if ( zctact.front() == '&' )
			{
				currAppl->vcopy( substr( zctact, 2 ), zctact, MOVE ) ;
				if ( zctact == "" ) { found = false ; continue ; }
			}
			found = true ;
			break ;
		}
		if ( err1.getRC() > 0 || word( zctact, 1 ) != "ALIAS" ) { break ; }
		cmdVerb = word( zctact, 2 )    ;
		if ( subword( zctact, 3 ) != "" )
		{
			cmdParm = subword( zctact, 3 ) ;
		}
		zcommand = cmdVerb + " " + cmdParm ;
	}

	if ( i > 255 )
	{
		llog( "E", "ALIAS dept cannot be greater than 256.  Terminating search" << endl ) ;
		found = false ;
	}

	return found ;
}


void processPGMSelect()
{
	// Called when an application program has invoked the SELECT service (also VIEW, EDIT, BROWSE)

	selobj selct = currAppl->get_select_cmd() ;

	resolvePGM( selct, currAppl ) ;

	updateDefaultVars() ;

	if ( selct.backgrd )
	{
		startApplicationBack( selct, currAppl ) ;
		ResumeApplicationAndWait() ;
	}
	else
	{
		selct.quiet = true ;
		startApplication( selct ) ;
		checkStartApplication( selct ) ;
	}
}


void resolvePGM( selobj& selct, pApplication* p )
{
	// Add application to run for SELECT PANEL() and SELECT CMD() services

	switch ( selct.pgmtype )
	{
	case PGM_REXX:
		p->vcopy( "ZOREXPGM", selct.pgm, MOVE ) ;
		break ;

	case PGM_PANEL:
		p->vcopy( "ZPANLPGM", selct.pgm, MOVE ) ;
		break ;

	case PGM_SHELL:
		p->vcopy( "ZSHELPGM", selct.pgm, MOVE ) ;
		break ;

	default:
		if ( selct.pgm.size() > 0 && selct.pgm.front() == '&' )
		{
			p->vcopy( substr( selct.pgm, 2 ), selct.pgm, MOVE ) ;
		}
	}
}


void startApplication( selobj& selct, bool nScreen )
{
	// Start an application using the passed SELECT object, on a new logical screen if specified.

	// If the program is ISPSTRT, start the application in the PARM field on a new logical screen
	// or start GMAINPGM.  Force NEWPOOL option regardless of what is coded in the command.
	// PARM can be a command table entry, a PGM()/CMD()/PANEL() statement or an option for GMAINPGM.

	// Select errors.  RSN=998 Module not found
	//                 RSN=996 Load errors

	int spool ;
	int iopt  ;

	string t     ;
	string opt   ;
	string sname ;
	string applid ;

	bool setMessage ;

	err.clear() ;

	pApplication* oldAppl = currAppl ;

	boost::thread* pThread ;

	if ( selct.pgm == "ISPSTRT" )
	{
		currAppl->clear_msg() ;
		nScreen = ( pLScreen::screensTotal < ZMAXSCRN ) ;
		opt     = upper( selct.parm ) ;
		if ( !selct.parse( err, selct.parm ) )
		{
			selct.def( gmainpgm ) ;
			commandStack = opt + commandStack ;
		}
		else
		{
			resolvePGM( selct, currAppl ) ;
		}
		selct.newpool = true ;
	}

	if ( apps.find( selct.pgm ) == apps.end() )
	{
		selct.errors = true ;
		selct.rsn    = 998  ;
		t = "Application '"+ selct.pgm +"' not found" ;
		if ( selct.quiet )
		{
			llog( "E", t << endl ) ;
		}
		else
		{
			errorScreen( 1, t ) ;
		}
		return ;
	}

	if ( !apps[ selct.pgm ].dlopened )
	{
		if ( !loadDynamicClass( selct.pgm ) )
		{
			selct.errors  = true ;
			selct.rsn     = 996  ;
			t = "Errors loading application "+ selct.pgm ;
			if ( selct.quiet )
			{
				llog( "E", t << endl ) ;
			}
			else
			{
				errorScreen( 1, t ) ;
			}
			return ;
		}
	}

	if ( nScreen && !createLogicalScreen() ) { return ; }

	currAppl->store_scrname() ;
	spool      = currAppl->shrdPool   ;
	setMessage = currAppl->setMessage ;
	sname      = p_poolMGR->get( err, "ZSCRNAME", SHARED ) ;
	if ( setMessage )
	{
		currAppl->setMessage = false ;
	}

	llog( "I", "Starting application "+ selct.pgm +" with parameters '"+ selct.parm +"'" << endl ) ;
	currAppl = ((pApplication*(*)())( apps[ selct.pgm ].maker_ep))() ;

	currScrn->application_add( currAppl ) ;

	currAppl->init_phase1( selct, ++maxTaskid, lspfCallbackHandler ) ;

	apps[ selct.pgm ].refCount++ ;

	applid = ( selct.newappl == "" ) ? oldAppl->get_applid() : selct.newappl ;
	err.settask( currAppl->taskid() ) ;

	currAppl->propagateEnd = oldAppl->propagateEnd ;
	currAppl->jumpEntered  = oldAppl->jumpEntered  ;

	if ( selct.newpool )
	{
		if ( currScrn->application_stack_size() > 1 && selct.scrname == "" )
		{
			selct.scrname = sname ;
		}
		spool = p_poolMGR->createSharedPool() ;
	}

	p_poolMGR->createProfilePool( err, applid ) ;
	p_poolMGR->connect( currAppl->taskid(), applid, spool ) ;
	if ( err.RC4() ) { createpfKeyDefaults() ; }

	createSharedPoolVars( applid ) ;

	currAppl->shrdPool = spool ;
	currAppl->init_phase2() ;

	if ( !selct.suspend )
	{
		currAppl->set_addpop( oldAppl ) ;
	}

	if ( !nScreen && ( selct.passlib || selct.newappl == "" ) )
	{
		currAppl->set_zlibd( selct.passlib, oldAppl ) ;
	}

	if ( setMessage )
	{
		currAppl->set_msg1( oldAppl->getmsg1(), oldAppl->getmsgid1() ) ;
	}
	else if ( !nScreen && !selct.fstack )
	{
		oldAppl->clear_msg() ;
	}

	currAppl->loadCommandTable() ;
	pThread = new boost::thread( &pApplication::run, currAppl ) ;

	currAppl->pThread = pThread ;

	if ( selct.scrname != "" )
	{
		p_poolMGR->put( err, "ZSCRNAME", selct.scrname, SHARED ) ;
	}

	llog( "I", "Waiting for new application to complete startup.  ID=" << pThread->get_id() << endl ) ;

	boost::mutex mutex ;
	while ( currAppl->busyAppl )
	{
		boost::mutex::scoped_lock lk( mutex ) ;
		cond_lspf.wait_for( lk, boost::chrono::milliseconds( 100 ) ) ;
		lk.unlock() ;
		if ( currAppl->busyAppl && isEscapeKey() )
		{
			iopt = listInterruptOptions() ;
			if ( iopt == 1 )
			{
				currAppl->set_timeout_abend() ;
				break ;
			}
		}
	}

	if ( currAppl->do_refresh_lscreen() )
	{
		if ( lwin )
		{
			deleteLinePanel() ;
			ResumeApplicationAndWait() ;
		}
		currScrn->refresh_panel_stack() ;
	}

	if ( currAppl->abnormalEnd )
	{
		abnormalTermMessage()  ;
		terminateApplication() ;
		if ( pLScreen::screensTotal == 0 ) { return ; }
	}
	else
	{
		llog( "I", "New thread and application started and initialised. ID=" << pThread->get_id() << endl ) ;
		if ( currAppl->SEL )
		{
			processPGMSelect() ;
		}
		else if ( currAppl->line_output_pending() )
		{
			lineOutput() ;
		}
		else if ( currAppl->line_input_pending() )
		{
			lineInput() ;
		}
	}

	while ( currAppl->terminateAppl )
	{
		terminateApplication() ;
		if ( pLScreen::screensTotal == 0 ) { return ; }
		if ( currAppl->SEL && !currAppl->terminateAppl )
		{
			processPGMSelect() ;
		}
	}
	currScrn->set_cursor( currAppl ) ;
}


void startApplicationBack( selobj selct, pApplication* oldAppl, bool pgmselect )
{
	// Start a background application using the passed SELECT object
	// Issue notify if application not found or errors loading program

	// Background task can be started synchronously or asynchronously.
	// If synchronous, parent needs to wait for the child to terminate before resuming.

	int spool ;

	string applid ;
	string msg    ;

	errblock err1 ;

	pApplication* Appl ;

	boost::thread* pThread ;

	if ( apps.find( selct.pgm ) == apps.end() )
	{
		msg = "Application '"+ selct.pgm +"' not found" ;
		oldAppl->notify( msg ) ;
		llog( "E", msg <<endl ) ;
		return ;
	}

	if ( !apps[ selct.pgm ].dlopened )
	{
		if ( !loadDynamicClass( selct.pgm ) )
		{
			msg = "Errors loading '"+ selct.pgm ;
			oldAppl->notify( msg ) ;
			llog( "E", msg <<endl ) ;
			return ;
		}
	}

	llog( "I", "Starting background application "+ selct.pgm +" with parameters '"+ selct.parm +"'" <<endl ) ;
	Appl = ((pApplication*(*)())( apps[ selct.pgm ].maker_ep))() ;

	Appl->init_phase1( selct, ++maxTaskid, lspfCallbackHandler ) ;

	apps[ selct.pgm ].refCount++ ;

	mtx.lock() ;
	pApplicationBackground.push_back( Appl ) ;
	mtx.unlock() ;

	applid = ( selct.newappl == "" ) ? oldAppl->get_applid() : selct.newappl ;
	err1.settask( Appl->taskid() ) ;

	if ( selct.newpool )
	{
		spool = p_poolMGR->createSharedPool() ;
	}
	else
	{
		spool  = oldAppl->shrdPool ;
	}

	if ( pgmselect )
	{
		oldAppl->vreplace( "ZSBTASK", Appl->taskid() ) ;
	}

	p_poolMGR->createProfilePool( err1, applid ) ;
	p_poolMGR->connect( Appl->taskid(), applid, spool ) ;

	createSharedPoolVars( applid ) ;

	if ( oldAppl->background() )
	{
		if ( selct.sync )
		{
			oldAppl->busyAppl = false ;
			oldAppl->SEL      = false ;
			Appl->uAppl       = oldAppl ;
		}
		else
		{
			oldAppl->busyAppl = true ;
			cond_appl.notify_all()   ;
		}
	}

	Appl->shrdPool = spool ;
	Appl->init_phase2() ;

	if ( selct.passlib || selct.newappl == "" )
	{
		Appl->set_zlibd( selct.passlib, oldAppl ) ;
	}

	pThread = new boost::thread( &pApplication::run, Appl ) ;

	Appl->pThread = pThread ;

	llog( "I", "New background thread and application started and initialised. ID=" << pThread->get_id() << endl ) ;
}


void processBackgroundTasks()
{
	// This routine runs in a separate thread to check if there are any
	// background tasks waiting for action:
	//    cleanup application if ended
	//    start application if SELECT or SUBMIT done in the background program

	// Any routine this procedure calls that uses the pApplication object, must have
	// the address passed as it won't be currAppl.

	backStatus = BACK_RUNNING ;

	boost::mutex mutex ;
	while ( lspfStatus == LSPF_RUNNING )
	{
		boost::mutex::scoped_lock lk( mutex ) ;
		cond_batch.wait_for( lk, boost::chrono::milliseconds( 100 ) ) ;
		lk.unlock() ;
		mtx.lock() ;
		for ( auto it = pApplicationBackground.begin() ; it != pApplicationBackground.end() ; ++it )
		{
			while ( (*it)->terminateAppl )
			{
				terminateApplicationBack( (*it) ) ;
				it = pApplicationBackground.erase( it ) ;
				if ( it == pApplicationBackground.end() ) { break ; }
			}
			if ( it == pApplicationBackground.end() ) { break ; }
		}
		mtx.unlock() ;

		mtx.lock() ;
		vector<pApplication*> temp = pApplicationBackground ;

		for ( auto it = temp.begin() ; it != temp.end() ; ++it )
		{
			if ( (*it)->SEL && !currAppl->terminateAppl )
			{
				selobj selct = (*it)->get_select_cmd() ;
				resolvePGM( selct, (*it) ) ;
				startApplicationBack( selct, (*it) ) ;
			}
		}
		mtx.unlock() ;

		if ( backStatus == BACK_STOPPING )
		{
			llog( "I", "lspf shutting down.  Removing background applications" << endl ) ;
			for ( auto it = pApplicationBackground.begin() ; it != pApplicationBackground.end() ; ++it )
			{
				terminateApplicationBack( (*it) ) ;
			}
		}
	}

	llog( "I", "Background job monitor task stopped" << endl ) ;
	backStatus = BACK_STOPPED ;
}


void checkStartApplication( selobj& selct )
{
	if ( selct.errors )
	{
		currAppl->RC      = 20  ;
		currAppl->ZRC     = 20  ;
		currAppl->ZRSN    = selct.rsn ;
		currAppl->ZRESULT = "PSYS013J" ;
		if ( not currAppl->errorsReturn() )
		{
			currAppl->abnormalEnd = true ;
		}
		ResumeApplicationAndWait() ;
		while ( currAppl->terminateAppl && pLScreen::screensTotal > 0 )
		{
			terminateApplication() ;
		}
	}
}


void terminateApplication()
{
	int tRC  ;
	int tRSN ;

	uint row ;
	uint col ;

	string zappname ;
	string tRESULT  ;
	string tMSGID1  ;
	string fname    ;
	string delm     ;

	bool refList       ;
	bool nretError     ;
	bool propagateEnd  ;
	bool abnormalEnd   ;
	bool jumpEntered   ;
	bool setCursorHome ;
	bool setMessage    ;
	bool nested        ;

	slmsg tMSG1 ;

	err.clear() ;

	pApplication* prvAppl ;

	boost::thread* pThread ;

	llog( "I", "Application terminating "+ currAppl->get_appname() +" ID: "<< currAppl->taskid() << endl ) ;

	zappname = currAppl->get_appname() ;

	tRC     = currAppl->ZRC  ;
	tRSN    = currAppl->ZRSN ;
	tRESULT = currAppl->ZRESULT ;
	abnormalEnd = currAppl->abnormalEnd ;

	refList = ( currAppl->reffield == "#REFLIST" ) ;

	setMessage = currAppl->setMessage ;
	if ( setMessage )
	{
		tMSGID1 = currAppl->getmsgid1() ;
		tMSG1   = currAppl->getmsg1() ;
	}

	jumpEntered  = currAppl->jumpEntered ;
	propagateEnd = currAppl->propagateEnd && ( currScrn->application_stack_size() > 1 ) ;
	nested       = currAppl->is_nested() ;

	pThread = currAppl->pThread ;

	if ( currAppl->abnormalEnd )
	{
		while ( currAppl->cleanupRunning() )
		{
			boost::this_thread::sleep_for( boost::chrono::milliseconds( 5 ) ) ;
		}
		pThread->detach() ;
	}

	p_poolMGR->disconnect( currAppl->taskid() ) ;

	currScrn->application_remove_current() ;
	if ( !currScrn->application_stack_empty() && currAppl->newappl == "" )
	{
		prvAppl = currScrn->application_get_current() ;
		prvAppl->set_zlibd( false, currAppl ) ;
	}

	if ( currAppl->abnormalTimeout )
	{
		llog( "I", "Moving application instance of "+ zappname +" to timeout queue" << endl ) ;
		pApplicationTimeout.push_back( currAppl ) ;
	}
	else
	{
		llog( "I", "Removing application instance of "+ zappname << endl ) ;
		((void (*)(pApplication*))(apps[ zappname ].destroyer_ep))( currAppl ) ;
		delete pThread ;
		apps[ zappname ].refCount-- ;
	}

	if ( currScrn->application_stack_empty() )
	{
		p_poolMGR->destroyPool( currScrn->screenid() ) ;
		if ( pLScreen::screensTotal == 1 )
		{
			delete currScrn ;
			return ;
		}
		deleteLogicalScreen() ;
	}

	currAppl = currScrn->application_get_current() ;
	err.settask( currAppl->taskid() ) ;

	currAppl->display_pd( err ) ;

	p_poolMGR->put( err, "ZPANELID", currAppl->get_panelid(), SHARED, SYSTEM ) ;

	if ( apps[ zappname ].refCount == 0 && apps[ zappname ].relPending )
	{
		apps[ zappname ].relPending = false ;
		llog( "I", "Reloading module "+ zappname +" (pending reload status)" << endl ) ;
		if ( loadDynamicClass( zappname ) )
		{
			llog( "I", "Loaded "+ zappname +" (module "+ apps[ zappname ].module +") from "+ apps[ zappname ].file << endl ) ;
		}
		else
		{
			llog( "W", "Errors occured loading "+ zappname +"  Module removed" << endl ) ;
		}
	}

	nretError = false ;
	if ( refList )
	{
		if ( currAppl->nretriev_on() )
		{
			fname = currAppl->get_nretfield() ;
			if ( fname != "" )
			{
				if ( currAppl->currPanel->field_valid( fname ) )
				{
					currAppl->reffield = fname ;
					if ( p_poolMGR->sysget( err, "ZRFMOD", PROFILE ) == "BEX" )
					{
						delm = p_poolMGR->sysget( err, "ZDEL", PROFILE ) ;
						commandStack = delm + delm ;
					}
				}
				else
				{
					llog( "E", "Invalid field "+ fname +" in .NRET panel statement" << endl ) ;
					issueMessage( "PSYS011Z" ) ;
					nretError = true ;
				}
			}
			else
			{
				issueMessage( "PSYS011Y" ) ;
				nretError = true ;
			}
		}
		else
		{
			issueMessage( "PSYS011X" ) ;
			nretError = true ;
		}
	}

	setCursorHome = true ;
	if ( currAppl->reffield != "" && !nretError )
	{
		if ( tRC == 0 )
		{
			if ( currAppl->currPanel->field_get_row_col( currAppl->reffield, row, col ) )
			{
				currAppl->currPanel->field_setvalue( currAppl->reffield, tRESULT ) ;
				currAppl->currPanel->cursor_eof( row, col ) ;
				currAppl->currPanel->set_cursor( row, col ) ;
				if ( refList )
				{
					issueMessage( "PSYS011W" ) ;
				}
				setCursorHome = false ;
			}
			else
			{
				llog( "E", "Invalid field "+ currAppl->reffield +" in .NRET panel statement" << endl )   ;
				issueMessage( "PSYS011Z" ) ;
			}
		}
		else if ( tRC == 8 )
		{
			beep() ;
			setCursorHome = false ;
		}
		currAppl->reffield = "" ;
	}

	if ( currAppl->isprimMenu() && propagateEnd )
	{
		propagateEnd = false ;
		commandStack = ( jumpEntered ) ? jumpOption : returnOption ;
		jumpOption   = "" ;
		returnOption = "" ;
	}

	if ( currAppl->SEL )
	{
		if ( abnormalEnd )
		{
			propagateEnd  = false ;
			currAppl->RC  = 20 ;
			currAppl->ZRC = 20 ;
			if ( tRC == 20 && tRSN > 900 )
			{
				currAppl->ZRSN    = tRSN    ;
				currAppl->ZRESULT = tRESULT ;
			}
			else
			{
				currAppl->ZRSN    = 999 ;
				currAppl->ZRESULT = "Abended" ;
			}
			if ( !currAppl->errorsReturn() )
			{
				currAppl->abnormalEnd = true ;
			}
		}
		else
		{
			if ( propagateEnd )
			{
				currAppl->jumpEntered = jumpEntered ;
				currAppl->RC = 4 ;
			}
			else
			{
				currAppl->RC = 0 ;
			}
			currAppl->ZRC     = tRC     ;
			currAppl->ZRSN    = tRSN    ;
			currAppl->ZRESULT = tRESULT ;
		}
		if ( setMessage )
		{
			currAppl->set_msg1( tMSG1, tMSGID1 ) ;
		}
		currAppl->propagateEnd = propagateEnd ;
		ResumeApplicationAndWait() ;
		while ( currAppl->terminateAppl )
		{
			terminateApplication() ;
			if ( pLScreen::screensTotal == 0 ) { return ; }
		}
	}
	else
	{
		currAppl->propagateEnd = false ;
		currAppl->set_userAction( USR_ENTER ) ;
		if ( propagateEnd && ( not nested || jumpEntered ) )
		{
			currAppl->jumpEntered  = jumpEntered ;
			currAppl->propagateEnd = true ;
			ResumeApplicationAndWait() ;
			while ( currAppl->terminateAppl )
			{
				terminateApplication() ;
				if ( pLScreen::screensTotal == 0 ) { return ; }
			}
		}
		else if ( propagateEnd && not jumpEntered )
		{
			commandStack = returnOption ;
			returnOption = "" ;
		}
		if ( setMessage )
		{
			currAppl->set_msg1( tMSG1, tMSGID1 ) ;
		}
		if ( setCursorHome )
		{
			currAppl->set_cursor_home() ;
		}
		if ( not currAppl->propagateEnd && pApplication::lineInOutDone )
		{
			deleteLinePanel() ;
			pApplication::lineInOutDone = false ;
			doupdate() ;
		}
		currAppl->redraw_panel( err ) ;
	}

	llog( "I", "Application terminatation of "+ zappname +" completed.  Current application is "+ currAppl->get_appname() << endl ) ;
	currAppl->restore_Zvars( currScrn->screenid() ) ;
	currAppl->display_id() ;
	currScrn->set_cursor( currAppl ) ;
}


void terminateApplicationBack( pApplication* p )
{
	// If uAppl is set, the terminated application was started synchronously so the
	// parent needs to resume processing.  Also pass back execution results.

	int tRC  ;
	int tRSN ;

	string tRESULT ;

	bool abnormalEnd ;

	boost::thread* pThread ;
	pApplication*  uAppl   ;

	llog( "I", "Application terminating "+ p->get_appname() +" ID: "<< p->taskid() << endl ) ;
	llog( "I", "Removing background application instance of "+
		p->get_appname() << " taskid: " << p->taskid() << endl ) ;

	pThread = p->pThread ;
	uAppl   = p->uAppl   ;
	tRC     = p->ZRC  ;
	tRSN    = p->ZRSN ;
	tRESULT = p->ZRESULT ;
	abnormalEnd = p->abnormalEnd ;

	p_poolMGR->disconnect( p->taskid() ) ;
	apps[ p->get_appname() ].refCount-- ;

	((void (*)(pApplication*))(apps[ p->get_appname() ].destroyer_ep))( p ) ;

	delete pThread ;

	if ( uAppl )
	{
		if ( abnormalEnd )
		{
			uAppl->RC  = 20 ;
			uAppl->ZRC = 20 ;
			if ( tRC == 20 && tRSN > 900 )
			{
				uAppl->ZRSN    = tRSN    ;
				uAppl->ZRESULT = tRESULT ;
			}
			else
			{
				uAppl->ZRSN    = 999 ;
				uAppl->ZRESULT = "Abended" ;
			}
			if ( !uAppl->errorsReturn() )
			{
				uAppl->abnormalEnd = true ;
			}
		}
		else
		{
			uAppl->RC      = 0 ;
			uAppl->ZRC     = tRC     ;
			uAppl->ZRSN    = tRSN    ;
			uAppl->ZRESULT = tRESULT ;
		}
		uAppl->busyAppl = true ;
		cond_appl.notify_all() ;
	}
}


bool createLogicalScreen()
{
	err.clear() ;

	if ( not pApplication::ControlSplitEnable )
	{
		p_poolMGR->put( err, "ZCTMVAR", left( zcommand, 8 ), SHARED ) ;
		issueMessage( "PSYS011" ) ;
		return false ;
	}

	if ( pLScreen::screensTotal == ZMAXSCRN )
	{
		issueMessage( "PSYS011D" ) ;
		return false ;
	}

	currScrn->save_panel_stack()  ;
	updateDefaultVars()           ;
	altScreen = priScreen         ;
	priScreen = screenList.size() ;

	screenList.push_back( new pLScreen( currScrn->screenid() ) ) ;
	pLScreen::OIA_startTime() ;

	lScreenDefaultSettings() ;
	return true ;
}


void deleteLogicalScreen()
{
	// Delete a logical screen and set the active screen to the one that opened it (or its predecessor if
	// that too has been closed)

	int openedBy = currScrn->get_openedBy() ;

	delete currScrn ;

	screenList.erase( screenList.begin() + priScreen ) ;

	if ( altScreen > priScreen ) { --altScreen ; }

	priScreen = pLScreen::get_priScreen( openedBy ) ;

	if ( priScreen == altScreen )
	{
		altScreen = ( priScreen == 0 ) ? pLScreen::screensTotal - 1 : 0 ;

	}

	currScrn = screenList[ priScreen ] ;
	currScrn->restore_panel_stack()    ;
	pLScreen::OIA_startTime() ;
}


void ResumeApplicationAndWait()
{
	int iopt ;

	uint row ;
	uint col ;

	displayNotifies() ;

	if ( currAppl->applicationEnded ) { return ; }

	if ( currAppl->simulate_end() )
	{
		currAppl->set_userAction( USR_END ) ;
		p_poolMGR->put( err, "ZVERB", "END", SHARED ) ;
	}

	currAppl->busyAppl = true ;
	cond_appl.notify_all()    ;
	boost::mutex mutex ;
	while ( currAppl->busyAppl )
	{
		boost::mutex::scoped_lock lk( mutex ) ;
		cond_lspf.wait_for( lk, boost::chrono::milliseconds( 100 ) ) ;
		lk.unlock() ;
		if ( currAppl->busyAppl && isEscapeKey() )
		{
			iopt = listInterruptOptions() ;
			if ( iopt == 1 )
			{
				currAppl->set_timeout_abend() ;
				break ;
			}
			pLScreen::show_busy() ;
			currScrn->set_cursor( currAppl ) ;
			currScrn->get_cursor( row, col ) ;
			move( row, col ) ;
		}
	}

	p_poolMGR->put( err, "ZVERB", "", SHARED ) ;

	if ( currAppl->reloadCUATables ) { loadCUATables() ; }
	if ( currAppl->do_refresh_lscreen() )
	{
		if ( lwin )
		{
			deleteLinePanel() ;
			ResumeApplicationAndWait() ;
		}
		currScrn->refresh_panel_stack() ;
	}

	if ( currAppl->abnormalEnd )
	{
		abnormalTermMessage() ;
	}
	else if ( currAppl->SEL )
	{
		processPGMSelect() ;
	}
	else if ( currAppl->line_output_pending() )
	{
		lineOutput() ;
	}
	else if ( currAppl->line_input_pending() )
	{
		lineInput() ;
	}
}


void loadCUATables()
{
	// Set according to the ZC variables in ISPS profile

	setColourPair( "AB" )   ;
	setColourPair( "ABSL" ) ;
	setColourPair( "ABU" )  ;

	setColourPair( "AMT" )  ;
	setColourPair( "AWF" )  ;

	setColourPair( "CT"  )  ;
	setColourPair( "CEF" )  ;
	setColourPair( "CH"  )  ;

	setColourPair( "DT" )   ;
	setColourPair( "ET" )   ;
	setColourPair( "EE" )   ;
	setColourPair( "FP" )   ;
	setColourPair( "FK")    ;

	setColourPair( "IWF" )  ;

	setColourPair( "IMT" )  ;
	setColourPair( "LEF" )  ;
	setColourPair( "LID" )  ;
	setColourPair( "LI" )   ;

	setColourPair( "NEF" )  ;
	setColourPair( "NT" )   ;

	setColourPair( "PI" )   ;
	setColourPair( "PIN" )  ;
	setColourPair( "PT" )   ;

	setColourPair( "PS")    ;
	setColourPair( "PAC" )  ;
	setColourPair( "PUC" )  ;

	setColourPair( "RP" )   ;

	setColourPair( "SI" )   ;
	setColourPair( "SAC" )  ;
	setColourPair( "SUC")   ;

	setColourPair( "VOI" )  ;
	setColourPair( "WMT" )  ;
	setColourPair( "WT" )   ;
	setColourPair( "WASL" ) ;

	setDecolourisedColour() ;

	setGlobalColours() ;
	setRGBValues() ;
}


void setColourPair( const string& name )
{
	string t ;

	err.clear() ;

	pair<map<attType, unsigned int>::iterator, bool> result ;

	result = cuaAttr.insert( pair<attType, unsigned int>( cuaAttrName[ name ], WHITE ) ) ;

	map<attType, unsigned int>::iterator it = result.first ;

	t = p_poolMGR->sysget( err, "ZC"+ name, PROFILE ) ;
	if ( !err.RC0() )
	{
		llog( "E", "Variable ZC"+ name +" not found in ISPS profile" << endl ) ;
		llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
		listErrorBlock( err ) ;
		abortStartup() ;
	}

	if ( t.size() != 3 )
	{
		llog( "E", "Variable ZC"+ name +" invalid value of "+ t +".  Must be length of three "<< endl ) ;
		llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
		listErrorBlock( err ) ;
		abortStartup() ;
	}

	switch  ( t[ 0 ] )
	{
		case 'R': it->second = RED     ; break ;
		case 'G': it->second = GREEN   ; break ;
		case 'Y': it->second = YELLOW  ; break ;
		case 'B': it->second = BLUE    ; break ;
		case 'M': it->second = MAGENTA ; break ;
		case 'T': it->second = TURQ    ; break ;
		case 'W': it->second = WHITE   ; break ;

		default :  llog( "E", "Variable ZC"+ name +" has invalid value[0] "+ t << endl ) ;
			   llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
			   abortStartup() ;
	}

	switch  ( t[ 1 ] )
	{
		case 'L':  it->second = it->second | A_NORMAL ; break ;
		case 'H':  it->second = it->second | A_BOLD   ; break ;

		default :  llog( "E", "Variable ZC"+ name +" has invalid value[1] "+ t << endl ) ;
			   llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
			   abortStartup() ;
	}

	switch  ( t[ 2 ] )
	{
		case 'N':  break ;
		case 'B':  it->second = it->second | A_BLINK     ; break ;
		case 'R':  it->second = it->second | A_REVERSE   ; break ;
		case 'U':  it->second = it->second | A_UNDERLINE ; break ;

		default :  llog( "E", "Variable ZC"+ name +" has invalid value[2] "+ t << endl ) ;
			   llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
			   abortStartup() ;
	}
}


void setGlobalColours()
{
	string t ;

	map<char, unsigned int> gcolours = { { 'R', COLOR_RED,     } ,
					     { 'G', COLOR_GREEN,   } ,
					     { 'Y', COLOR_YELLOW,  } ,
					     { 'B', COLOR_BLUE,    } ,
					     { 'M', COLOR_MAGENTA, } ,
					     { 'T', COLOR_CYAN,    } ,
					     { 'W', COLOR_WHITE    } } ;

	err.clear() ;

	t = p_poolMGR->sysget( err, "ZGCLR", PROFILE ) ;
	init_pair( 1, gcolours[ t.front() ], COLOR_BLACK ) ;

	t = p_poolMGR->sysget( err, "ZGCLG", PROFILE ) ;
	init_pair( 2, gcolours[ t.front() ], COLOR_BLACK ) ;

	t = p_poolMGR->sysget( err, "ZGCLY", PROFILE ) ;
	init_pair( 3, gcolours[ t.front() ], COLOR_BLACK ) ;

	t = p_poolMGR->sysget( err, "ZGCLB", PROFILE ) ;
	init_pair( 4, gcolours[ t.front() ], COLOR_BLACK ) ;

	t = p_poolMGR->sysget( err, "ZGCLM", PROFILE ) ;
	init_pair( 5, gcolours[ t.front() ], COLOR_BLACK ) ;

	t = p_poolMGR->sysget( err, "ZGCLT", PROFILE ) ;
	init_pair( 6, gcolours[ t.front() ], COLOR_BLACK ) ;

	t = p_poolMGR->sysget( err, "ZGCLW", PROFILE ) ;
	init_pair( 7, gcolours[ t.front() ], COLOR_BLACK ) ;
}


void setDefaultRGB()
{
	short int r ;
	short int g ;
	short int b ;

	string t ;
	string n_rgb = "ZNRGB" ;
	string u_rgb = "ZURGB" ;

	for ( int i = 1 ; i < COLORS ; ++i )
	{
		color_content( i, &r, &g, &b ) ;
		t = d2ds( r, 4 ) + d2ds( g, 4 ) + d2ds( b, 4 ) ;
		p_poolMGR->sysput( err, n_rgb + colourValue[ i ], t, SHARED ) ;
	}

	for ( uint i = 1 ; i < 8 ; ++i )
	{
		t = p_poolMGR->sysget( err, u_rgb + colourValue[ i ], PROFILE ) ;
		if ( t.size() != 12 )
		{
			t = p_poolMGR->sysget( err, n_rgb + colourValue[ i ], SHARED ) ;
			p_poolMGR->sysput( err, u_rgb + colourValue[ i ], t, PROFILE ) ;
			continue ;
		}
		r = ds2d( t.substr( 0, 4 ) ) ;
		g = ds2d( t.substr( 4, 4 ) ) ;
		b = ds2d( t.substr( 8, 4 ) ) ;
		init_color( i, r, g, b ) ;
	}
}


void setRGBValues()
{
	short int r ;
	short int g ;
	short int b ;

	string t ;
	string u_rgb = "ZURGB" ;

	for ( uint i = 1 ; i < 8 ; ++i )
	{
		t = p_poolMGR->sysget( err, u_rgb+colourValue[ i ], PROFILE ) ;
		if ( t.size() != 12 )
		{
			continue ;
		}
		r = ds2d( t.substr( 0, 4 ) ) ;
		g = ds2d( t.substr( 4, 4 ) ) ;
		b = ds2d( t.substr( 8, 4 ) ) ;
		init_color( i, r, g, b ) ;
	}

	currScrn->refresh_panel_stack() ;
	pLScreen::OIA_refresh() ;
	redrawwin( stdscr ) ;
	wnoutrefresh( stdscr ) ;
}


void setDecolourisedColour()
{
	string t ;

	err.clear() ;

	t = p_poolMGR->sysget( err, "ZDECLRA", PROFILE ) ;
	if ( !err.RC0() )
	{
		llog( "E", "Variable ZDECLRA not found in ISPS profile" << endl ) ;
		llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
		listErrorBlock( err ) ;
		abortStartup() ;
	}

	if ( t.size() != 2 )
	{
		llog( "E", "Variable ZDCLRA invalid value of "+ t +".  Must be length of two "<< endl ) ;
		llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
		listErrorBlock( err ) ;
		abortStartup() ;
	}

	switch  ( t[ 0 ] )
	{
		case 'R': decColour1 = 1       ;
			  decColour2 = RED     ;
			  break ;

		case 'G': decColour1 = 2       ;
			  decColour2 = GREEN   ;
			  break ;

		case 'Y': decColour1 = 3       ;
			  decColour2 = YELLOW  ;
			  break ;

		case 'B': decColour1 = 4       ;
			  decColour2 = BLUE    ;
			  break ;

		case 'M': decColour1 = 5       ;
			  decColour2 = MAGENTA ;
			  break ;

		case 'T': decColour1 = 6       ;
			  decColour2 = TURQ    ;
			  break ;

		case 'W': decColour1 = 7       ;
			  decColour2 = WHITE   ;
			  break ;

		default :  llog( "E", "Variable ZDECLRA has invalid value[0] "+ t << endl ) ;
			   llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
			   abortStartup() ;
	}

	switch  ( t[ 1 ] )
	{
		case 'L':  decIntens = A_NORMAL ; break ;
		case 'H':  decIntens = A_BOLD   ; break ;

		default :  llog( "E", "Variable ZDECLRA has invalid value[1] "+ t << endl ) ;
			   llog( "S", "Rerun setup program to re-initialise ISPS profile" << endl ) ;
			   abortStartup() ;
	}

}


void decolouriseScreen()
{
	if ( p_poolMGR->sysget( err, "ZDECLR", PROFILE ) == "Y" )
	{
		currScrn->decolourise_inactive( decColour1, decColour2, decIntens ) ;
	}
	else
	{
		currScrn->set_frame_inactive( intens ) ;
	}
}


void loadDefaultPools()
{
	// Default vars go in @DEFPROF (RO) for PROFILE and @DEFSHAR (UP) for SHARE
	// These have the SYSTEM attibute set on the variable

	string log    ;
	string zuprof ;
	string home   ;
	string shell  ;
	string logname ;

	err.clear() ;

	struct utsname buf ;

	if ( uname( &buf ) != 0 )
	{
		llog( "S", "System call uname has returned an error"<< endl ) ;
		abortStartup() ;
	}

	home = getEnvironmentVariable( "HOME" ) ;
	if ( home == "" )
	{
		llog( "S", "HOME variable is required and must be set"<< endl ) ;
		abortStartup() ;
	}
	zuprof = home + ZUPROF ;

	logname = getEnvironmentVariable( "LOGNAME" ) ;
	if ( logname == "" )
	{
		llog( "S", "LOGNAME variable is required and must be set"<< endl ) ;
		abortStartup() ;
	}

	shell = getEnvironmentVariable( "SHELL" ) ;

	p_poolMGR->setProfilePath( err, zuprof ) ;

	p_poolMGR->sysput( err, "ZSCREEND", d2ds( pLScreen::maxrow ), SHARED ) ;
	p_poolMGR->sysput( err, "ZSCRMAXD", d2ds( pLScreen::maxrow ), SHARED ) ;
	p_poolMGR->sysput( err, "ZSCREENW", d2ds( pLScreen::maxcol ), SHARED ) ;
	p_poolMGR->sysput( err, "ZSCRMAXW", d2ds( pLScreen::maxcol ), SHARED ) ;
	p_poolMGR->sysput( err, "ZUSER", logname, SHARED ) ;
	p_poolMGR->sysput( err, "ZHOME", home, SHARED )    ;
	p_poolMGR->sysput( err, "ZSHELL", shell, SHARED )  ;

	p_poolMGR->createProfilePool( err, "ISPS" ) ;
	if ( !err.RC0() )
	{
		llog( "S", "Loading of system profile ISPSPROF failed.  RC="<< err.getRC() << endl ) ;
		llog( "S", "Aborting startup.  Check profile pool path" << endl ) ;
		listErrorBlock( err ) ;
		abortStartup() ;
	}
	llog( "I", "Loaded system profile ISPSPROF" << endl ) ;

	log = p_poolMGR->sysget( err, "ZSLOG", PROFILE ) ;

	lg->set( log ) ;
	llog( "I", "Starting logger on " << log << endl ) ;

	log = p_poolMGR->sysget( err, "ZALOG", PROFILE ) ;
	lgx->set( log ) ;
	llog( "I", "Starting application logger on " << log << endl ) ;

	p_poolMGR->createSharedPool() ;

	p_poolMGR->sysput( err, "Z", "", SHARED )                  ;
	p_poolMGR->sysput( err, "ZSCRNAM1", "OFF", SHARED )        ;
	p_poolMGR->sysput( err, "ZSYSNAME", buf.sysname, SHARED )  ;
	p_poolMGR->sysput( err, "ZNODNAME", buf.nodename, SHARED ) ;
	p_poolMGR->sysput( err, "ZOSREL", buf.release, SHARED )    ;
	p_poolMGR->sysput( err, "ZOSVER", buf.version, SHARED )    ;
	p_poolMGR->sysput( err, "ZMACHINE", buf.machine, SHARED )  ;
	p_poolMGR->sysput( err, "ZDATEF",  "DD/MM/YY", SHARED )    ;
	p_poolMGR->sysput( err, "ZDATEFD", "DD/MM/YY", SHARED )    ;
	p_poolMGR->sysput( err, "ZSCALE", "OFF", SHARED )          ;
	p_poolMGR->sysput( err, "ZSPLIT", "NO", SHARED )           ;
	p_poolMGR->sysput( err, "ZNULLS", "NO", SHARED )           ;
	p_poolMGR->sysput( err, "ZCMPDATE", __DATE__, SHARED ) ;
	p_poolMGR->sysput( err, "ZCMPTIME", __TIME__, SHARED ) ;
	p_poolMGR->sysput( err, "ZENVIR", "lspf " LSPF_VERSION, SHARED ) ;
	p_poolMGR->sysput( err, "ZMXTABSZ", d2ds( MXTAB_SZ ), SHARED ) ;

	p_poolMGR->setPOOLsReadOnly() ;
	gmainpgm = p_poolMGR->sysget( err, "ZMAINPGM", PROFILE ) ;

	checkSystemVariable( "ZMLIB" ) ;
	checkSystemVariable( "ZPLIB" ) ;
	checkSystemVariable( "ZTLIB" ) ;
}


string getEnvironmentVariable( const char* var )
{
	char* t = getenv( var ) ;

	if ( t == NULL )
	{
		llog( "I", "Environment variable "+ string( var ) +" has not been set"<< endl ) ;
		return "" ;
	}
	return t ;
}


void checkSystemVariable( const string& var )
{
	if ( p_poolMGR->sysget( err, var, PROFILE ) == "" )
	{
		llog( "S", var + " has not been set.  Aborting startup."<< endl ) ;
		abortStartup() ;
	}
}


void loadSystemCommandTable()
{
	// Terminate if ISPCMDS not found in ZUPROF

	string zuprof ;

	err.clear() ;

	zuprof  = getenv( "HOME" ) ;
	zuprof += ZUPROF ;
	p_tableMGR->loadTable( err, "ISPCMDS", NOWRITE, zuprof, SHARE ) ;
	if ( !err.RC0() )
	{
		llog( "S", "Loading of system command table ISPCMDS failed" <<endl ) ;
		llog( "S", "RC="<< err.getRC() <<"  Aborting startup" <<endl ) ;
		llog( "S", "Check path "+ zuprof << endl ) ;
		listErrorBlock( err ) ;
		abortStartup() ;
	}
	llog( "I", "Loaded system command table ISPCMDS" << endl ) ;
}


void loadRetrieveBuffer()
{
	uint rbsize ;

	string ifile  ;
	string inLine ;

	if ( p_poolMGR->sysget( err, "ZSRETP", PROFILE ) == "N" ) { return ; }

	ifile  = getenv( "HOME" ) ;
	ifile += ZUPROF      ;
	ifile += "/RETPLIST" ;

	std::ifstream fin( ifile.c_str() ) ;
	if ( !fin.is_open() ) { return ; }

	rbsize = ds2d( p_poolMGR->sysget( err, "ZRBSIZE", PROFILE ) ) ;
	if ( retrieveBuffer.capacity() != rbsize )
	{
		retrieveBuffer.rset_capacity( rbsize ) ;
	}

	llog( "I", "Reloading retrieve buffer" << endl ) ;

	while ( getline( fin, inLine ) )
	{
		retrieveBuffer.push_back( inLine ) ;
		if ( retrieveBuffer.size() == retrieveBuffer.capacity() ) { break ; }
	}
	fin.close() ;
}


void saveRetrieveBuffer()
{
	string ofile ;

	if ( retrieveBuffer.empty() || p_poolMGR->sysget( err, "ZSRETP", PROFILE ) == "N" ) { return ; }

	ofile  = getenv( "HOME" ) ;
	ofile += ZUPROF      ;
	ofile += "/RETPLIST" ;

	std::ofstream fout( ofile.c_str() ) ;

	if ( !fout.is_open() ) { return ; }

	llog( "I", "Saving retrieve buffer" << endl ) ;

	for ( size_t i = 0 ; i < retrieveBuffer.size() ; ++i )
	{
		fout << retrieveBuffer[ i ] << endl ;
	}
	fout.close() ;
}


string pfKeyValue( int c )
{
	// Return the value of a pfkey stored in the profile pool.  If it does not exist, VPUT a null value.

	int keyn ;

	string key ;
	string val ;

	err.clear() ;

	keyn = c - KEY_F( 0 ) ;
	key  = "ZPF" + d2ds( keyn, 2 ) ;
	val  = p_poolMGR->get( err, key, PROFILE ) ;
	if ( err.RC8() )
	{
		p_poolMGR->put( err, key, "", PROFILE ) ;
	}

	return val ;
}


void createpfKeyDefaults()
{
	err.clear() ;

	for ( int i = 1 ; i < 25 ; ++i )
	{
		p_poolMGR->put( err, "ZPF" + d2ds( i, 2 ), pfKeyDefaults[ i ], PROFILE ) ;
		if ( !err.RC0() )
		{
			llog( "E", "Error creating default key for task "<<err.taskid<<endl);
			listErrorBlock( err ) ;
		}
	}
}


string ControlKeyAction( char c )
{
	// Translate the control-key to a command (stored in ZCTRLx system profile variables)

	err.clear() ;

	string s = keyname( c ) ;
	string k = "ZCTRL"      ;

	k.push_back( s[ 1 ] ) ;

	return p_poolMGR->sysget( err, k, PROFILE ) ;
}


void lScreenDefaultSettings()
{
	// Set the default message setting for this logical screen
	// ZDEFM = Y show message id on messages
	//         N don't show message id on messages

	p_poolMGR->put( err, currScrn->screenid(), "ZSHMSGID", p_poolMGR->sysget( err, "ZDEFM", PROFILE ) ) ;
}


void updateDefaultVars()
{
	err.clear() ;

	gmainpgm = p_poolMGR->sysget( err, "ZMAINPGM", PROFILE ) ;
	p_poolMGR->sysput( err, "ZSPLIT", pLScreen::screensTotal > 1 ? "YES" : "NO", SHARED ) ;

	field::field_paduchar = p_poolMGR->sysget( err, "ZPADC", PROFILE ).front() ;

	field::field_nulls    = ( p_poolMGR->sysget( err, "ZNULLS", SHARED ) == "YES" ) ;

	intens                = ( p_poolMGR->sysget( err, "ZHIGH", PROFILE ) == "Y" ) ? A_BOLD : A_NORMAL ;
	field::field_intens   = intens ;
	pPanel::panel_intens  = intens ;
	pdc::pdc_intens       = intens ;
	abc::abc_intens       = intens ;
	Box::box_intens       = intens ;
	text::text_intens     = intens ;
}


void createSharedPoolVars( const string& applid )
{
	err.clear() ;

	p_poolMGR->put( err, "ZSCREEN", string( 1, zscreen[ priScreen ] ), SHARED, SYSTEM ) ;
	p_poolMGR->put( err, "ZSCRNUM", d2ds( currScrn->screenid() ), SHARED, SYSTEM ) ;
	p_poolMGR->put( err, "ZAPPLID", applid, SHARED, SYSTEM ) ;
	p_poolMGR->put( err, "ZPFKEY", "PF00", SHARED, SYSTEM ) ;

}


void updateReflist()
{
	// Check if .NRET is ON and has a valid field name.  If so, add file to the reflist using
	// application ZRFLPGM, parmameters PLA plus the field entry value.  Run task in the background.

	// Don't update REFLIST if the application has done a CONTROL REFLIST NOUPDATE (flag ControlRefUpdate=false)
	// or ISPS PROFILE variable ZRFURL is not set to YES

	string fname = currAppl->get_nretfield() ;

	selobj selct ;

	err.clear() ;

	if ( fname == "" ||
	     p_poolMGR->get( err, currScrn->screenid(), "ZREFUPDT" ) != "Y" ||
	     p_poolMGR->sysget( err, "ZRFURL", PROFILE ) != "YES" )
	{
		return ;
	}

	if ( currAppl->currPanel->field_valid( fname ) )
	{
		selct.clear() ;
		selct.pgm     = p_poolMGR->sysget( err, "ZRFLPGM", PROFILE ) ;
		selct.parm    = "PLA " + currAppl->currPanel->field_getrawvalue( fname ) ;
		selct.newappl = ""    ;
		selct.newpool = false ;
		selct.passlib = false ;
		selct.backgrd = true  ;
		selct.sync    = false ;
		startApplicationBack( selct, currAppl, false ) ;
	}
	else
	{
		llog( "E", "Invalid field "+ fname +" in .NRET panel statement" << endl ) ;
		issueMessage( "PSYS011Z" ) ;
	}
}


void lineOutput()
{
	// Write line output to the display.  Split line if longer than screen width.

	size_t i ;

	string t ;

	if ( not lwin )
	{
		createLinePanel() ;
		beep() ;
	}

	pLScreen::clear_busy() ;
	pLScreen::show_busy() ;
	wattrset( lwin, RED | A_BOLD ) ;
	t = currAppl->outBuffer ;

	do
	{
		if ( linePosn == int( pLScreen::maxrow-1 ) )
		{
			lineOutput_end() ;
		}
		if ( t.size() > pLScreen::maxcol )
		{
			i = t.find_last_of( ' ', pLScreen::maxcol ) ;
			i = ( i == string::npos ) ? pLScreen::maxcol : i + 1 ;
			mvwaddstr( lwin, linePosn++, 0, t.substr( 0, i ).c_str() ) ;
			t.erase( 0, i ) ;
		}
		else
		{
			mvwaddstr( lwin, linePosn++, 0, t.c_str() ) ;
			wnoutrefresh( lwin ) ;
			t = "" ;
		}
	} while ( t.size() > 0 ) ;

	pLScreen::OIA_endTime() ;
	currScrn->set_cursor( linePosn, 3 ) ;
	currScrn->OIA_update( priScreen, altScreen ) ;

	wnoutrefresh( lwin ) ;
	wnoutrefresh( OIA ) ;
	top_panel( lpanel ) ;
	wmove( lwin, linePosn, 0 ) ;
	update_panels() ;
	doupdate() ;
}


void lineOutput_end()
{
	uint row = 0 ;
	uint col = 0 ;

	set<uint>rows ;

	wattrset( lwin, RED | A_BOLD ) ;
	mvwaddstr( lwin, linePosn, 0, "***" ) ;

	pLScreen::OIA_endTime() ;
	currScrn->OIA_update( priScreen, altScreen ) ;

	pLScreen::show_enter() ;

	currScrn->set_cursor( linePosn, 3 ) ;
	top_panel( lpanel ) ;

	updateScreenText( rows, row, col ) ;
	wattrset( lwin, RED | A_BOLD ) ;

	linePosn = 0 ;
	werase( lwin ) ;

	pLScreen::clear_status() ;
	pLScreen::OIA_startTime() ;

	if ( currAppl->notify_pending() )
	{
		while ( currAppl->notify() )
		{
			lineOutput() ;
		}
		lineOutput_end() ;
	}
}


void displayNotifies()
{
	bool fscreen = !lwin ;

	if ( currAppl->notify_pending() )
	{
		while ( currAppl->notify() )
		{
			lineOutput() ;
		}
		if ( fscreen )
		{
			deleteLinePanel() ;
			pApplication::lineInOutDone = false ;
			currScrn->set_cursor( currAppl ) ;
		}
	}
}


void lineInput()
{
	uint row = linePosn ;
	uint col = 0 ;

	set<uint>rows ;

	if ( not lwin )
	{
		createLinePanel() ;
		row = 0 ;
		col = 0 ;
	}
	else if ( linePosn == int( pLScreen::maxrow-1 ) )
	{
		lineOutput_end() ;
		row = 0 ;
		col = 0 ;
	}
	++linePosn ;

	currScrn->set_cursor( row, col ) ;
	pLScreen::clear_status() ;

	updateScreenText( rows, row, col ) ;

	currAppl->inBuffer = "" ;
	for ( auto it = rows.begin() ; it != rows.end() ; ++it )
	{
		currAppl->inBuffer += getScreenText( *it ) ;
	}
}


void updateScreenText( set<uint>& rows, uint row, uint col )
{
	// Update the screen text in raw mode.

	int c ;

	bool actionKey = false ;
	bool Insert    = false ;

	wattrset( lwin, GREEN | A_BOLD ) ;

	while ( not actionKey )
	{
		currScrn->get_cursor( row, col ) ;
		currScrn->OIA_update( priScreen, altScreen ) ;
		wnoutrefresh( OIA ) ;
		wmove( lwin, row, col ) ;
		update_panels() ;
		doupdate() ;
		c = getch() ;
		if ( c < 256 && isprint( c ) )
		{
			rows.insert( row ) ;
			Insert ? winsch( lwin, c ) : waddch( lwin, c ) ;
			wnoutrefresh( lwin ) ;
			currScrn->cursor_right() ;
			continue ;
		}
		switch( c )
		{

			case KEY_LEFT:
				currScrn->cursor_left() ;
				break ;

			case KEY_RIGHT:
				currScrn->cursor_right() ;
				break ;

			case KEY_UP:
				currScrn->cursor_up() ;
				break ;

			case KEY_DOWN:
				currScrn->cursor_down() ;
				break ;

			case KEY_IC:
				Insert = not Insert ;
				currScrn->set_Insert( Insert ) ;
				break ;

			case KEY_DC:
				mvwdelch( lwin, row, col ) ;
				rows.insert( row ) ;
				break ;

			case KEY_END:
				col = min( ( pLScreen::maxcol - 1 ), getTextLength( row ) + 1 ) ;
				currScrn->set_cursor( row, col ) ;
				break ;

			case 127:
			case KEY_BACKSPACE:
				if ( col > 0 )
				{
					mvwdelch( lwin, row, --col ) ;
					currScrn->set_cursor( row, col ) ;
					rows.insert( row ) ;
				}
				break ;

			case KEY_HOME:
				currScrn->set_cursor( 0, 0 ) ;
				break ;

			default:
				actionKey = isActionKey( c ) ;
				break ;
		}
	}

	currScrn->set_Insert( false ) ;
}


string getScreenText( uint row )
{
	// Retrieve a row of text on the screen excluding null characters.
	// Nulls are determined by the fact they have no associated attributes.

	chtype inc1 ;
	chtype inc2 ;

	string t ;
	t.reserve( pLScreen::maxcol ) ;

	for ( uint j = 0 ; j < pLScreen::maxcol ; ++j )
	{
		inc1 = mvwinch( lwin, row, j ) ;
		inc2 = inc1 & A_CHARTEXT ;
		if ( inc1 != inc2 )
		{
			t.push_back( char( inc2 ) ) ;
		}
	}

	return t ;
}


uint getTextLength( uint row )
{
	// Retrieve the row text length.

	int l = pLScreen::maxcol - 1 ;

	for ( ; l >= 0 ; --l )
	{
		if ( char( mvwinch( lwin, row, l ) & A_CHARTEXT ) != ' ' ) { break ; }
	}

	return l ;
}


string listLogicalScreens()
{
	// Mainline lspf cannot create application panels but let's make this as similar as possible

	int o ;
	int c ;

	uint i ;
	uint m ;
	string ln  ;
	string t   ;
	string act ;
	string w2  ;

	err.clear() ;

	WINDOW* swwin   ;
	PANEL*  swpanel ;

	pApplication* appl ;

	vector<pLScreen*>::iterator its ;
	vector<string>::iterator it ;
	vector<string> lslist ;

	swwin   = newwin( screenList.size() + 6, 80, 1, 1 ) ;
	swpanel = new_panel( swwin )  ;
	wattrset( swwin, cuaAttr[ AWF ] | intens ) ;
	box( swwin, 0, 0 ) ;
	mvwaddstr( swwin, 0, 34, " Task List " ) ;

	wattrset( swwin, cuaAttr[ PT ] | intens ) ;
	mvwaddstr( swwin, 1, 28, "Active Logical Sessions" ) ;

	wattrset( swwin, cuaAttr[ PIN ] | intens ) ;
	mvwaddstr( swwin, 3, 2, "ID  Name      Application  Applid  Panel Title/Description" ) ;

	pLScreen::show_wait() ;

	m = 0 ;
	for ( i = 0, its = screenList.begin() ; its != screenList.end() ; ++its, ++i )
	{
		appl = (*its)->application_get_current() ;
		ln   = d2ds( (*its)->get_screenNum() )         ;
		if      ( i == priScreen ) { ln += "*" ; m = i ; }
		else if ( i == altScreen ) { ln += "-"         ; }
		else                       { ln += " "         ; }
		t = appl->get_current_panelDesc() ;
		if ( t.size() > 42 )
		{
			t.replace( 20, t.size()-39, "..." ) ;
		}
		ln = left( ln, 4 ) +
		     left( appl->get_current_screenName(), 10 ) +
		     left( appl->get_appname(), 13 ) +
		     left( appl->get_applid(), 8  )  +
		     left( t, 42 ) ;
		lslist.push_back( ln ) ;
	}

	o = m         ;
	curs_set( 0 ) ;
	while ( true )
	{
		for ( i = 0, it = lslist.begin() ; it != lslist.end() ; ++it, ++i )
		{
			wattrset( swwin, cuaAttr[ i == m ? PT : VOI ] | intens ) ;
			mvwaddstr( swwin, i+4, 2, it->c_str() ) ;
		}
		update_panels() ;
		doupdate()  ;
		c = getch() ;
		if ( c >= CTRL( 'a' ) && c <= CTRL( 'z' ) )
		{
			act = ControlKeyAction( c ) ;
			iupper( act ) ;
			if ( word( act, 1 ) == "SWAP" )
			{
				w2 = word( act, 2 ) ;
				if ( w2 == "LISTN" ) { c = KEY_DOWN ; }
				else if ( w2 == "LISTP" ) { c = KEY_UP ; }
			}
		}
		if ( c == KEY_ENTER || c == 13 ) { break ; }
		if ( c == KEY_UP )
		{
			( m == 0 ) ? m = lslist.size() - 1 : --m ;
		}
		else if ( c == KEY_DOWN || c == CTRL( 'i' ) )
		{
			( m == lslist.size()-1 ) ? m = 0 : ++m ;
		}
		else if ( isActionKey( c ) )
		{
			m = o ;
			break ;
		}
	}

	del_panel( swpanel ) ;
	delwin( swwin ) ;

	curs_set( 1 ) ;
	pLScreen::OIA_startTime() ;

	return d2ds( m+1 ) ;
}


int listInterruptOptions()
{
	// Mainline lspf cannot create application panels but let's make this as similar as possible

	int c ;

	uint i ;
	uint m ;

	string appl ;

	err.clear() ;

	WINDOW* swwin   ;
	PANEL*  swpanel ;

	vector<string>::iterator it ;
	vector<string> lslist ;

	appl = "Application: " + currAppl->get_appname() ;

	lslist.push_back( "1. Continue running application (press ESC again to interrupt)" ) ;
	lslist.push_back( "2. Abend application" ) ;

	swwin   = newwin( lslist.size() + 7, 66, 1, 1 ) ;
	swpanel = new_panel( swwin )  ;
	wattrset( swwin, cuaAttr[ AWF ] | intens ) ;
	box( swwin, 0, 0 ) ;

	wattrset( swwin, cuaAttr[ PT ] | intens ) ;
	mvwaddstr( swwin, 1, 21, "Program Interrupt Options" ) ;
	wattrset( swwin, cuaAttr[ PIN ] | intens ) ;
	mvwaddstr( swwin, 3, 2, appl.c_str() ) ;

	pLScreen::show_wait() ;

	m = 0 ;
	curs_set( 0 ) ;
	while ( true )
	{
		for ( i = 0, it = lslist.begin() ; it != lslist.end() ; ++it, ++i )
		{
			wattrset( swwin, cuaAttr[ i == m ? PT : VOI ] | intens ) ;
			mvwaddstr( swwin, i+5, 2, it->c_str() ) ;
		}
		update_panels() ;
		doupdate()  ;
		c = getch() ;
		if ( c == KEY_ENTER || c == 13 ) { break ; }
		if ( c == KEY_UP )
		{
			m == 0 ? m = lslist.size() - 1 : --m ;
		}
		else if ( c == KEY_DOWN || c == CTRL( 'i' ) )
		{
			m == lslist.size()-1 ? m = 0 : ++m ;
		}
		else if ( isActionKey( c ) )
		{
			m = 0 ;
			break ;
		}
	}

	del_panel( swpanel ) ;
	delwin( swwin ) ;
	curs_set( 1 ) ;

	update_panels() ;
	doupdate() ;

	pLScreen::OIA_startTime() ;

	return m ;
}


void listRetrieveBuffer()
{
	// Mainline lspf cannot create application panels but let's make this as similar as possible

	int c ;

	uint i ;
	uint m ;
	uint mx  ;

	string t  ;
	string ln ;

	WINDOW* rbwin   ;
	PANEL*  rbpanel ;

	vector<string> lslist ;
	vector<string>::iterator it ;

	err.clear() ;
	if ( retrieveBuffer.empty() )
	{
		issueMessage( "PSYS012B" ) ;
		return ;
	}

	mx = (retrieveBuffer.size() > pLScreen::maxrow-6) ? pLScreen::maxrow-6 : retrieveBuffer.size() ;

	rbwin   = newwin( mx+5, 60, 1, 1 ) ;
	rbpanel = new_panel( rbwin )  ;
	wattrset( rbwin, cuaAttr[ AWF ] | intens ) ;
	box( rbwin, 0, 0 ) ;
	mvwaddstr( rbwin, 0, 25, " Retrieve " ) ;

	wattrset( rbwin, cuaAttr[ PT ] | intens ) ;
	mvwaddstr( rbwin, 1, 17, "lspf Command Retrieve Panel" ) ;

	pLScreen::show_wait() ;
	for ( i = 0 ; i < mx ; ++i )
	{
		t = retrieveBuffer[ i ] ;
		if ( t.size() > 52 )
		{
			t.replace( 20, t.size()-49, "..." ) ;
		}
		ln = left(d2ds( i+1 )+".", 4 ) + t ;
		lslist.push_back( ln ) ;
	}

	curs_set( 0 ) ;
	m = 0 ;
	while ( true )
	{
		for ( i = 0, it = lslist.begin() ; it != lslist.end() ; ++it, ++i )
		{
			wattrset( rbwin, cuaAttr[ i == m ? PT : VOI ] | intens ) ;
			mvwaddstr( rbwin, i+3, 3, it->c_str() ) ;
		}
		update_panels() ;
		doupdate()  ;
		c = getch() ;
		if ( c == KEY_ENTER || c == 13 )
		{
			currAppl->currPanel->cmd_setvalue( retrieveBuffer[ m ] ) ;
			currAppl->currPanel->cursor_to_cmdfield( retrieveBuffer[ m ].size()+1 ) ;
			break ;
		}
		else if ( c == KEY_UP )
		{
			m == 0 ? m = lslist.size() - 1 : --m ;
		}
		else if ( c == KEY_DOWN || c == CTRL( 'i' ) )
		{
			m == lslist.size() - 1 ? m = 0 : ++m ;
		}
		else if ( isActionKey( c ) )
		{
			currAppl->currPanel->cursor_to_cmdfield() ;
			break ;
		}
	}

	del_panel( rbpanel ) ;
	delwin( rbwin ) ;
	curs_set( 1 )   ;

	currScrn->set_cursor( currAppl ) ;
	pLScreen::OIA_startTime() ;
}


void listBackTasks()
{
	// List background tasks and tasks that have been moved to the timeout queue

	llog( "I", ".TASKS" <<endl ) ;
	llog( "-", "****************************************************" <<endl ) ;
	llog( "-", "Listing background tasks:" << endl ) ;
	llog( "-", "         Number of tasks. . . . "<< pApplicationBackground.size()<< endl ) ;
	llog( "-", " "<< endl ) ;

	mtx.lock() ;
	listTaskVector( pApplicationBackground ) ;
	mtx.unlock() ;

	llog( "-", " "<< endl ) ;
	llog( "-", "Listing timed-out tasks:" << endl ) ;
	llog( "-", "         Number of tasks. . . . "<< pApplicationTimeout.size()<< endl ) ;
	llog( "-", " "<< endl ) ;

	listTaskVector( pApplicationTimeout ) ;

	llog( "-", "****************************************************" <<endl ) ;
}


void listTaskVector( vector<pApplication*>& p )
{
	if ( p.size() == 0 ) { return ; }

	llog( "-", "Program     Id       Status               Parameters" << endl);
	llog( "-", "--------    -----    -----------------    ----------" << endl);

	for ( auto it = p.begin() ; it != p.end() ; ++it )
	{
		llog( "-", "" << setw(12) << std::left << (*it)->get_appname() <<
				 setw(9)  << std::left << d2ds( (*it)->taskid(), 5 ) <<
				 setw(21) << std::left << (*it)->get_status() <<
				 (*it)->get_zparm() <<endl ) ;
	}
}


void autoUpdate()
{
	// Resume application every 1s and wait.
	// Check every 50ms to see if ESC(27) has been pressed - read all characters from the buffer.

	int c ;

	bool end_auto = false ;

	pLScreen::show_auto() ;
	curs_set( 0 ) ;

	while ( !end_auto )
	{
		ResumeApplicationAndWait() ;
		decolouriseScreen() ;
		pLScreen::OIA_endTime() ;
		currScrn->OIA_update( priScreen, altScreen ) ;
		wnoutrefresh( stdscr ) ;
		wnoutrefresh( OIA ) ;
		update_panels() ;
		doupdate() ;
		nodelay( stdscr, true ) ;
		pLScreen::OIA_startTime() ;
		for ( int i = 0 ; i < 20 && !end_auto ; ++i )
		{
			boost::this_thread::sleep_for( boost::chrono::milliseconds( 50 ) ) ;
			do
			{
				c = getch() ;
				if ( c == CTRL( '[' ) ) { end_auto = true ; }
			} while ( c != -1 ) ;
		}
	}

	curs_set( 1 ) ;
	pLScreen::clear_status() ;
	nodelay( stdscr, false ) ;

	currScrn->set_cursor( currAppl ) ;
}


int getScreenNameNum( const string& s )
{
	// Return the screen number of screen name 's'.  If not found, return 0.
	// If a full match is not found, try to match an abbreviation.

	int i ;
	int j ;

	vector<pLScreen*>::iterator its ;
	pApplication* appl ;

	for ( i = 1, j = 0, its = screenList.begin() ; its != screenList.end() ; ++its, ++i )
	{
		appl = (*its)->application_get_current() ;
		if ( appl->get_current_screenName() == s )
		{
			return i ;
		}
		else if ( j == 0 && abbrev( appl->get_current_screenName(), s ) )
		{
			j = i ;
		}
	}

	return j ;
}


void lspfCallbackHandler( lspfCommand& lc )
{
	// Issue commands from applications using lspfCallback() function
	// Replies go into the reply vector

	string w1 ;
	string w2 ;

	vector<pLScreen*>::iterator its    ;
	map<string, appInfo>::iterator ita ;
	pApplication* appl ;

	lc.reply.clear() ;

	w1 = word( lc.Command, 1 ) ;
	w2 = word( lc.Command, 2 ) ;

	if ( lc.Command == "SWAP LIST" )
	{
		for ( its = screenList.begin() ; its != screenList.end() ; ++its )
		{
			appl = (*its)->application_get_current()       ;
			lc.reply.push_back( d2ds( (*its)->get_screenNum() ) ) ;
			lc.reply.push_back( appl->get_appname()   )    ;
		}
		lc.RC = 0 ;
	}
	else if ( lc.Command == "BATCH KEYS" )
	{
		mtx.lock() ;
		for ( auto it = pApplicationBackground.begin() ; it != pApplicationBackground.end() ; ++it )
		{
			lc.reply.push_back( (*it)->get_jobkey() ) ;
		}
		mtx.unlock() ;
		lc.RC = 0 ;
	}
	else if ( lc.Command == "MODULE STATUS" )
	{
		for ( ita = apps.begin() ; ita != apps.end() ; ++ita )
		{
			lc.reply.push_back( ita->first ) ;
			lc.reply.push_back( ita->second.module ) ;
			lc.reply.push_back( ita->second.file   ) ;
			if ( ita->second.mainpgm )
			{
				lc.reply.push_back( "R/Not Reloadable" ) ;
			}
			else if ( ita->second.relPending )
			{
				lc.reply.push_back( "Reload Pending" ) ;
			}
			else if ( ita->second.errors )
			{
				lc.reply.push_back( "Errors" ) ;
			}
			else if ( ita->second.refCount > 0 )
			{
				lc.reply.push_back( "Running" ) ;
			}
			else if ( ita->second.dlopened )
			{
				lc.reply.push_back( "Loaded" ) ;
			}
			else
			{
				lc.reply.push_back( "Not Loaded" ) ;
			}
		}
		lc.RC = 0 ;
	}
	else if ( w1 == "MODREL" )
	{
		reloadDynamicClasses( w2 ) ;
		lc.RC = 0 ;
	}
	else
	{
		lc.RC = 20 ;
	}
}


void abortStartup()
{
	delete screenList[ 0 ] ;

	cout << "*********************************************************************" << endl ;
	cout << "*********************************************************************" << endl ;
	cout << "Aborting startup of lspf.  Check lspf and application logs for errors" << endl ;
	cout << "lspf log name. . . . . :"<< lg->logname() << endl ;
	cout << "Application log name . :"<< lgx->logname() << endl ;
	cout << "*********************************************************************" << endl ;
	cout << "*********************************************************************" << endl ;
	cout << endl ;

	delete lg  ;
	delete lgx ;

	abort() ;
}


void abnormalTermMessage()
{
	if ( currAppl->abnormalTimeout )
	{
		errorScreen( 2, "Application has been terminated at user request" ) ;
	}
	else if ( currAppl->abnormalEndForced )
	{
		errorScreen( 2, "A forced termination of the subtask has occured" ) ;
	}
	else if ( not currAppl->abnormalNoMsg )
	{
		errorScreen( 2, "An error has occured during application execution" ) ;
	}
}


void errorScreen( int etype, const string& msg )
{
	string t ;

	llog( "E", msg << endl ) ;

	if ( currAppl->errPanelissued || currAppl->abnormalNoMsg  ) { return ; }

	createLinePanel() ;

	beep() ;
	wattrset( lwin, RED | A_BOLD ) ;
	mvwaddstr( lwin, linePosn++, 0, msg.c_str() ) ;
	mvwaddstr( lwin, linePosn++, 0, "See lspf and application logs for possible further details of the error" ) ;

	if ( etype == 2 )
	{
		t = "Failing application is " + currAppl->get_appname() + ", taskid=" + d2ds( currAppl->taskid() ) ;
		llog( "E", t << endl ) ;
		mvwaddstr( lwin, linePosn++, 0, "Depending on the error, application may still be running in the background.  Recommend restarting lspf." ) ;
		mvwaddstr( lwin, linePosn++, 0, t.c_str() ) ;
	}

	deleteLinePanel() ;

	currScrn->set_cursor( currAppl ) ;
}


void errorScreen( const string& title, const string& src )
{
	// Show dialogue error screen PSYSER2 with the contents of the errblock.

	// This is done by starting application PPSP01A with a parm of PSYSER2 and
	// passing the error parameters via selobj.options.

	// Make sure err_struct is the same in application PPSP01A.

	selobj selct ;

	struct err_struct
	{
		string title ;
		string src   ;
		errblock err ;
	} errs ;

	errs.err   = err   ;
	errs.title = title ;
	errs.src   = src   ;

	selct.pgm  = "PPSP01A" ;
	selct.parm = "PSYSER2" ;
	selct.newappl = ""    ;
	selct.newpool = false ;
	selct.passlib = false ;
	selct.suspend = true  ;
	selct.options = (void*)&errs ;

	startApplication( selct ) ;
}


void errorScreen( const string& msg )
{
	// Show dialogue error screen PSYSER3 with message 'msg'

	// This is done by starting application PPSP01A with a parm of PSYSER3 plus the message id.
	// ZVAL1-ZVAL3 need to be put into the SHARED pool depending on the message issued.

	selobj selct ;

	selct.pgm  = "PPSP01A" ;
	selct.parm = "PSYSER3 "+ msg ;
	selct.newappl = ""    ;
	selct.newpool = false ;
	selct.passlib = false ;
	selct.suspend = true  ;

	startApplication( selct ) ;
}


void issueMessage( const string& msg )
{
	err.clear() ;

	currAppl->save_errblock() ;
	currAppl->set_msg( msg ) ;
	err = currAppl->get_errblock() ;
	currAppl->restore_errblock()   ;
	if ( !err.RC0() )
	{
		errorScreen( 1, "Syntax error in message "+ msg +", message file or message not found" ) ;
		listErrorBlock( err ) ;
	}
}


void createLinePanel()
{
	if ( not lwin )
	{
		lwin   = newwin( pLScreen::maxrow, pLScreen::maxcol, 0, 0 ) ;
		lpanel = new_panel( lwin ) ;
		top_panel( lpanel ) ;
		update_panels() ;
		linePosn = 0 ;
	}
}


void deleteLinePanel()
{
	if ( lwin )
	{
		lineOutput_end() ;
		del_panel( lpanel ) ;
		delwin( lwin ) ;
		lwin = NULL ;
	}
}


void serviceCallError( errblock& err )
{
	llog( "E", "A Serive Call error has occured"<< endl ) ;
	listErrorBlock( err ) ;
}


void listErrorBlock( errblock& err )
{
	llog( "E", "Error msg  : "<< err.msg1 << endl )  ;
	llog( "E", "Error id   : "<< err.msgid << endl ) ;
	llog( "E", "Error RC   : "<< err.getRC() << endl ) ;
	llog( "E", "Error ZVAL1: "<< err.val1 << endl )  ;
	llog( "E", "Error ZVAL2: "<< err.val2 << endl )  ;
	llog( "E", "Error ZVAL3: "<< err.val3 << endl )  ;
	llog( "E", "Source     : "<< err.getsrc() << endl ) ;
}


bool isActionKey( int c )
{
	return ( ( c >= KEY_F(1)    && c <= KEY_F(24) )   ||
		 ( c >= CTRL( 'a' ) && c <= CTRL( 'z' ) ) ||
		   c == CTRL( '[' ) ||
		   c == KEY_NPAGE   ||
		   c == KEY_PPAGE   ||
		   c == KEY_ENTER ) ;
}


bool isEscapeKey()
{
	int c ;

	bool esc_pressed = false ;

	nodelay( stdscr, true ) ;
	do
	{
		c = getch() ;
		if ( c == CTRL( '[' ) ) { esc_pressed = true ; }
	} while ( c != -1 ) ;

	nodelay( stdscr, false ) ;
	return esc_pressed ;
}


void actionSwap( const string& parm )
{
	int l ;

	uint i ;

	string w1 ;
	string w2 ;

	w1 = word( parm, 1 ) ;
	w2 = word( parm, 2 ) ;

	if ( not currAppl->currPanel->keep_cmd() )
	{
		currAppl->clear_msg() ;
		currAppl->currPanel->cmd_setvalue( "" ) ;
		currAppl->currPanel->cursor_to_cmdfield() ;
		currScrn->set_cursor( currAppl ) ;
	}

	if ( findword( w1, "LIST LISTN LISTP" ) )
	{
		w1 = listLogicalScreens() ;
		if ( w1 == d2ds( priScreen + 1 ) ) { return ; }
	}

	if ( pLScreen::screensTotal == 1 ) { return ; }

	currScrn->save_panel_stack() ;
	currAppl->store_scrname()    ;
	if ( w1 =="*" ) { w1 = d2ds( priScreen+1 ) ; }
	if ( w1 != "" && w1 != "NEXT" && w1 != "PREV" && isvalidName( w1 ) )
	{
		l = getScreenNameNum( w1 ) ;
		if ( l > 0 ) { w1 = d2ds( l ) ; }
	}
	if ( w2 != "" && isvalidName( w2 ) )
	{
		l = getScreenNameNum( w2 ) ;
		if ( l > 0 ) { w2 = d2ds( l ) ; }
	}

	if ( w1 == "NEXT" )
	{
		++priScreen ;
		priScreen = (priScreen == pLScreen::screensTotal ? 0 : priScreen) ;
		if ( altScreen == priScreen )
		{
			altScreen = (altScreen == 0 ? (pLScreen::screensTotal - 1) : (altScreen - 1) ) ;
		}
	}
	else if ( w1 == "PREV" )
	{
		if ( priScreen == 0 )
		{
			priScreen = pLScreen::screensTotal - 1 ;
		}
		else
		{
			--priScreen ;
		}
		if ( altScreen == priScreen )
		{
			altScreen = ((altScreen == pLScreen::screensTotal - 1) ? 0 : (altScreen + 1) ) ;
		}
	}
	else if ( datatype( w1, 'W' ) )
	{
		i = ds2d( w1 ) - 1 ;
		if ( i < pLScreen::screensTotal )
		{
			if ( i != priScreen )
			{
				if ( w2 == "*" || i == altScreen )
				{
					altScreen = priScreen ;
				}
				priScreen = i ;
			}
		}
		else
		{
			swap( priScreen, altScreen ) ;
		}
		if ( datatype( w2, 'W' ) && w1 != w2 )
		{
			i = ds2d( w2 ) - 1 ;
			if ( i != priScreen && i < pLScreen::screensTotal )
			{
				altScreen = i ;
			}
		}
	}
	else
	{
		swap( priScreen, altScreen ) ;
	}


	currScrn = screenList[ priScreen ] ;
	pLScreen::OIA_startTime() ;
	currAppl = currScrn->application_get_current() ;

	if ( not currAppl->currPanel->keep_cmd() )
	{
		currAppl->currPanel->cmd_setvalue( "" ) ;
		currAppl->currPanel->cursor_to_cmdfield() ;
		currScrn->set_cursor( currAppl ) ;
	}

	err.settask( currAppl->taskid() ) ;
	p_poolMGR->put( err, "ZPANELID", currAppl->get_panelid(), SHARED, SYSTEM ) ;
	currScrn->restore_panel_stack() ;
	currAppl->display_pd( err ) ;
}


void actionTabKey( uint& row, uint& col )
{
	// Tab processsing:
	//     If a pull down is active, go to next pulldown
	//     If cursor on a field that supports field execution and is not on the first char, or space
	//     before cursor, execute function
	//     Else act as a tab key to the next input field

	uint rw = 0 ;
	uint cl = 0 ;

	bool tab_next = true ;

	string field_name ;

	fieldExc fxc ;

	if ( currAppl->currPanel->pd_active() )
	{
		displayPullDown() ;
	}
	else
	{
		field_name = currAppl->currPanel->field_getname( row, col ) ;
		if ( field_name != "" )
		{
			currAppl->currPanel->field_get_row_col( field_name, rw, cl ) ;
			if ( rw == row && cl < col && currAppl->currPanel->field_nonblank( field_name, col-cl-1 ) )
			{
				fxc = currAppl->currPanel->field_getexec( field_name ) ;
				if ( fxc.fieldExc_command != "" )
				{
					executeFieldCommand( field_name, fxc, col ) ;
					tab_next = false ;
				}
			}
		}
		if ( tab_next )
		{
			currAppl->currPanel->field_tab_next( row, col ) ;
			currScrn->set_cursor( row, col ) ;
		}
	}
}


void displayPullDown( const string& mnemonic )
{
	string msg ;

	currAppl->currPanel->display_next_pd( err, mnemonic, msg ) ;
	if ( err.error() )
	{
		errorScreen( 1, "Error processing pull-down menu." ) ;
		serviceCallError( err ) ;
		currAppl->set_cursor_home() ;
	}
	else if ( msg != "" )
	{
		issueMessage( msg ) ;
	}
	currScrn->set_cursor( currAppl ) ;
}


void executeFieldCommand( const string& field_name, const fieldExc& fxc, uint col )
{
	// Run application associated with a field when tab pressed or command FIELDEXC entered

	// Cursor position is stored in shared variable ZFECSRP
	// Primary field is stored in shared variable ZFEDATA0
	// Data to be passed to the application (fieldExc_passed) are stored in shared vars ZFEDATAn

	int i  ;
	int ws ;

	uint cl ;

	string w1 ;

	selobj selct ;

	if ( !selct.parse( err, subword( fxc.fieldExc_command, 2 ) ) )
	{
		llog( "E", "Error in FIELD SELECT command "+ fxc.fieldExc_command << endl ) ;
		issueMessage( "PSYS011K" ) ;
		return ;
	}

	p_poolMGR->put( err, "ZFEDATA0", currAppl->currPanel->field_getrawvalue( field_name ), SHARED ) ;
	currAppl->reffield = field_name ;
	cl = currAppl->currPanel->field_get_col( field_name ) ;
	p_poolMGR->put( err, "ZFECSRP", d2ds( col - cl + 1 ), SHARED ) ;

	for ( ws = words( fxc.fieldExc_passed ), i = 1 ; i <= ws ; ++i )
	{
		w1 = word( fxc.fieldExc_passed, i ) ;
		p_poolMGR->put( err, "ZFEDATA" + d2ds( i ), currAppl->currPanel->field_getrawvalue( w1 ), SHARED ) ;
	}

	startApplication( selct ) ;
}


void getDynamicClasses()
{
	// Get modules of the form libABCDE.so from ZLDPATH concatination with name ABCDE and store in map apps
	// Duplicates are ignored with a warning messasge.
	// Terminate lspf if ZMAINPGM module not found as we cannot continue

	int i        ;
	int j        ;
	int pos1     ;

	string appl  ;
	string mod   ;
	string fname ;
	string paths ;
	string p     ;

	typedef vector<path> vec ;
	vec v        ;

	vec::const_iterator it ;

	err.clear() ;
	appInfo aI  ;

	const string e1( gmainpgm +" not found.  Check ZLDPATH is correct.  lspf terminating **" ) ;

	paths = p_poolMGR->sysget( err, "ZLDPATH", PROFILE ) ;
	j     = getpaths( paths ) ;
	for ( i = 1 ; i <= j ; ++i )
	{
		p = getpath( paths, i ) ;
		if ( is_directory( p ) )
		{
			llog( "I", "Searching directory "+ p +" for application classes" << endl ) ;
			copy( directory_iterator( p ), directory_iterator(), back_inserter( v ) ) ;
		}
		else
		{
			llog( "W", "Ignoring directory "+ p +"  Not found or not a directory." << endl ) ;
		}
	}

	for ( it = v.begin() ; it != v.end() ; ++it )
	{
		fname = it->string() ;
		p     = substr( fname, 1, (lastpos( "/", fname ) - 1) ) ;
		mod   = substr( fname, (lastpos( "/", fname ) + 1) )    ;
		pos1  = pos( ".so", mod ) ;
		if ( substr(mod, 1, 3 ) != "lib" || pos1 == 0 ) { continue ; }
		appl  = substr( mod, 4, (pos1 - 4) ) ;
		if ( apps.count( appl ) > 0 )
		{
			llog( "W", "Ignoring duplicate module "+ mod +" found in "+ p << endl ) ;
			continue ;
		}
		llog( "I", "Adding application "+ appl << endl ) ;
		aI.file       = fname ;
		aI.module     = mod   ;
		aI.mainpgm    = false ;
		aI.dlopened   = false ;
		aI.errors     = false ;
		aI.relPending = false ;
		aI.refCount   = 0     ;
		apps[ appl ]  = aI    ;
	}

	llog( "I", d2ds( apps.size() ) +" applications found and stored" << endl ) ;

	if ( apps.find( gmainpgm ) == apps.end() )
	{
		llog( "S", e1 << endl ) ;
		abortStartup()          ;
	}
}


void reloadDynamicClasses( string parm )
{
	// Reload modules (ALL, NEW or modname).  Ignore reload for modules currently in-use but set
	// pending flag to be checked when application terminates.

	int i        ;
	int j        ;
	int k        ;
	int pos1     ;

	string appl  ;
	string mod   ;
	string fname ;
	string paths ;
	string p     ;

	bool stored  ;

	err.clear()  ;

	typedef vector<path> vec ;
	vec v      ;
	appInfo aI ;

	vec::const_iterator it ;

	paths = p_poolMGR->sysget( err, "ZLDPATH", PROFILE ) ;
	j     = getpaths( paths ) ;
	for ( i = 1 ; i <= j ; ++i )
	{
		p = getpath( paths, i ) ;
		if ( is_directory( p ) )
		{
			llog( "I", "Searching directory "+ p +" for application classes" << endl ) ;
			copy( directory_iterator( p ), directory_iterator(), back_inserter( v ) ) ;
		}
		else
		{
			llog( "W", "Ignoring directory "+ p +"  Not found or not a directory." << endl ) ;
		}
	}
	if ( parm == "" ) { parm = "ALL" ; }

	i = 0 ;
	j = 0 ;
	k = 0 ;
	for ( it = v.begin() ; it != v.end() ; ++it )
	{
		fname = it->string() ;
		p     = substr( fname, 1, (lastpos( "/", fname ) - 1) ) ;
		mod   = substr( fname, (lastpos( "/", fname ) + 1) )    ;
		pos1  = pos( ".so", mod ) ;
		if ( substr(mod, 1, 3 ) != "lib" || pos1 == 0 ) { continue ; }
		appl  = substr( mod, 4, (pos1 - 4) ) ;
		llog( "I", "Found application "+ appl << endl ) ;
		stored = ( apps.find( appl ) != apps.end() ) ;

		if ( parm == "NEW" && stored ) { continue ; }
		if ( parm != "NEW" && parm != "ALL" && parm != appl ) { continue ; }
		if ( appl == gmainpgm ) { continue ; }
		if ( parm == appl && stored && !apps[ appl ].dlopened )
		{
			apps[ appl ].file = fname ;
			llog( "W", "Application "+ appl +" not loaded.  Ignoring action" << endl ) ;
			return ;
		}
		if ( stored )
		{
			apps[ appl ].file = fname ;
			if ( apps[ appl ].refCount > 0 )
			{
				llog( "W", "Application "+ appl +" in use.  Reload pending" << endl ) ;
				apps[ appl ].relPending = true ;
				continue ;
			}
			if ( apps[ appl ].dlopened )
			{
				if ( loadDynamicClass( appl ) )
				{
					llog( "I", "Loaded "+ appl +" (module "+ mod +") from "+ p << endl ) ;
					++i ;
				}
				else
				{
					llog( "W", "Errors occured loading "+ appl << endl ) ;
					++k ;
				}
			}
		}
		else
		{
			llog( "I", "Adding new module "+ appl << endl ) ;
			aI.file        = fname ;
			aI.module      = mod   ;
			aI.mainpgm     = false ;
			aI.dlopened    = false ;
			aI.errors      = false ;
			aI.relPending  = false ;
			aI.refCount    = 0     ;
			apps[ appl ]   = aI    ;
			++j ;
		}
		if ( parm == appl ) { break ; }
	}

	issueMessage( "PSYS012G" ) ;
	llog( "I", d2ds( i ) +" applications reloaded" << endl ) ;
	llog( "I", d2ds( j ) +" new applications stored" << endl ) ;
	llog( "I", d2ds( k ) +" errors encounted" << endl ) ;
	if ( parm != "ALL" && parm != "NEW" )
	{
		if ( (i+j) == 0 )
		{
			llog( "W", "Application "+ parm +" not reloaded/stored" << endl ) ;
			issueMessage( "PSYS012I" ) ;
		}
		else
		{
			llog( "I", "Application "+ parm +" reloaded/stored" << endl )   ;
			issueMessage( "PSYS012H" ) ;
		}
	}

	llog( "I", d2ds( apps.size() ) + " applications currently stored" << endl ) ;
}


bool loadDynamicClass( const string& appl )
{
	// Load module related to application appl and retrieve address of maker and destroy symbols
	// Perform dlclose first if there has been a previous successful dlopen, or if an error is encountered

	// Routine only called if the refCount is zero

	string mod   ;
	string fname ;

	void* dlib  ;
	void* maker ;
	void* destr ;

	const char* dlsym_err ;

	mod   = apps[ appl ].module ;
	fname = apps[ appl ].file   ;
	apps[ appl ].errors = true  ;

	if ( apps[ appl ].dlopened )
	{
		llog( "I", "Closing "+ appl << endl ) ;
		if ( !unloadDynamicClass( apps[ appl ].dlib ) )
		{
			llog( "W", "dlclose has failed for "+ appl << endl ) ;
			return false ;
		}
		apps[ appl ].dlopened = false ;
		llog( "I", "Reloading module "+ appl << endl ) ;
	}

	dlerror() ;
	dlib = dlopen( fname.c_str(), RTLD_NOW ) ;
	if ( !dlib )
	{
		llog( "E", "Error loading "+ fname << endl )  ;
		llog( "E", "Error is " << dlerror() << endl ) ;
		llog( "E", "Module "+ mod +" will be ignored" << endl ) ;
		return false ;
	}

	dlerror() ;
	maker     = dlsym( dlib, "maker" ) ;
	dlsym_err = dlerror() ;
	if ( dlsym_err )
	{
		llog( "E", "Error loading symbol maker" << endl ) ;
		llog( "E", "Error is " << dlsym_err << endl )     ;
		llog( "E", "Module "+ mod +" will be ignored" << endl ) ;
		unloadDynamicClass( apps[ appl ].dlib ) ;
		return false ;
	}

	dlerror() ;
	destr     = dlsym( dlib, "destroy" ) ;
	dlsym_err = dlerror() ;
	if ( dlsym_err )
	{
		llog( "E", "Error loading symbol destroy" << endl ) ;
		llog( "E", "Error is " << dlsym_err << endl )       ;
		llog( "E", "Module "+ mod +" will be ignored" << endl ) ;
		unloadDynamicClass( apps[ appl ].dlib ) ;
		return false ;
	}

	debug1( fname +" loaded at " << dlib << endl ) ;
	debug1( "Maker at " << maker << endl ) ;
	debug1( "Destroyer at " << destr << endl ) ;

	apps[ appl ].dlib         = dlib  ;
	apps[ appl ].maker_ep     = maker ;
	apps[ appl ].destroyer_ep = destr ;
	apps[ appl ].mainpgm      = false ;
	apps[ appl ].errors       = false ;
	apps[ appl ].dlopened     = true  ;

	return true ;
}


bool unloadDynamicClass( void* dlib )
{
	int i ;
	int rc = 0 ;

	for ( i = 0 ; i < 10 && rc == 0 ; ++i )
	{
		try
		{
			rc = dlclose( dlib ) ;
		}
		catch (...)
		{
			llog( "E", "An exception has occured during dlclose" << endl ) ;
			return false ;
		}
	}

	return ( rc != 0 ) ;
}
