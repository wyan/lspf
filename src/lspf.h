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

#include <string>
#include <stack>
#include <fstream>
#include <panel.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <set>

// #define DEBUG1 1
// #define DEBUG2 1
#define MOD_NAME lspf

#define LSPF_VERSION "1.1.14"
#define LSPF_VERSION_MAJ 1
#define LSPF_VERSION_REV 1
#define LSPF_VERSION_MOD 14

typedef unsigned int uint ;

using namespace std ;
using namespace boost::posix_time ;

// Customisable values below.  Also included in the setup.cpp program for the ISPS profile pool.
// Recompile setup.cpp and run after changes

// ~ character represents the user's home directory and will be resolved at setup run time

// ZUPROF   - User subdirectory where the lspf user-specific profiles/tables are found (ISPS, profiles, clipboard)
//            ZUPROF will be prefixed with the user's home directory at runtime (ZUPROF in ISPSPROF contains the prefix)
// ZSYSPATH - Where the system-specific files are found.
// ZLDPATH  - Where the application classes are located (concatenation allowed)
// SLOG     - Location of the user's system log file.  Can use ~ character.
// ALOG     - Location of the user's application log file. Can use ~ character.
// MLIB     - Search path for messages. Can use ~ character.
// PLIB     - Search path for panels. Can use ~ character.
// TLIB     - Search path for tables. Can use ~ character.
// TABL     - Table ouput path. Can use the ~ character (concatination allowed).
// MUSR     - User search path for messages. Can use ~ character (concatination allowed).
// PUSR     - User search path for panels. Can use ~ character (concatination allowed).
// TUSR     - User search path for tables. Can use ~ character (concatination allowed).
// TABU     - User table ouput path. Can use the ~ character (concatination allowed).
// ZREXPATH - Location of the rexx execs (conatenation allowed).  Can use ~ character
// ZSPOOL   - Name of the directory to store command and job output.  Can use ~ character.
// ZMAINPGM - Name of the initial program to invoke.  This is treated as a SELECT PANEL()
// ZMAINPAN - Name of the initial selection panel to invoke ( ie. SELECT PANEL(ZMAINPAN) )
// ZPANLPGM - Name of the program invoked on the SELECT PANEL service
// ZEDITPGM - Name of the editor program to invoke
// ZBRPGM   - Name of the browser program to invoke
// ZVIEWPGM - Name of the viewer program to invoke (usually same as the default editor program)
// ZFLSTPGM - Name of the file list program to invoke
// ZHELPPGM - Name of the tutorial/help program to invoke
// ZOREXPGM - Name of the oorexx interpreter call program to invoke
// ZSHELPGM - Name of the shell interpreter program to invoke
// ZSHELP   - Name of the system help member (ZPLIB/help concatenation searched for this)
// ZMAXSCRN - Maximum number of split screens allowed (Greater than 8 and the screen will not be displayed in the Screen[] status area.)
// EDREC_SZ - Size of the edit recovery table
// MXTAB_SZ - Maximum number of rows allowed in an lspf table (must be less than 16,777,216)

#define ZUPROF          "/.lspf"
#define ZSYSPATH        "/home/daniel/lspf"
#define ZLDPATH         ZSYSPATH "/Apps:" ZSYSPATH "/Apps2"
#define MLIB            "~" ZUPROF "/mlib:" ZSYSPATH "/mlib"
#define PLIB            "~" ZUPROF "/plib:" ZSYSPATH "/plib"
#define TLIB            "~" ZUPROF ":~" ZUPROF "/tlib:" ZSYSPATH "/tlib"
#define TABL            "~" ZUPROF "/tlib"
#define MUSR            ""
#define PUSR            ""
#define TUSR            ""
#define TABU            ""
#define ZREXPATH        "~/rexx:" ZSYSPATH "/rexx"
#define ZSPOOL          "~" ZUPROF "/spool"
#define SLOG            "~/.lspf/lspflog"
#define ALOG            "~/.lspf/appllog"
#define ZRFLTBL         "LSRPLIST"
#define ZMAINPGM        "PMAIN0A"
#define ZMAINPAN        "PMAINP01"
#define ZPANLPGM        "PDPANLA"
#define ZEDITPGM        "PEDIT01"
#define ZBRPGM          "PBRO01A"
#define ZVIEWPGM        "PEDIT01"
#define ZFLSTPGM        "PFLST0A"
#define ZHELPPGM        "PTUTORA"
#define ZOREXPGM        "POREXX1"
#define ZSHELPGM        "PSHELL0"
#define ZSHELP          "HPSPF01"
#define ZRFLPGM         "PLRFLST1"
#define ZMAXSCRN        8
#define EDREC_SZ        8
#define MXTAB_SZ        500000
// ***************************************** End custom values **************************************

// ***************************************** CUA defaults *******************************************
#define KAB     "YLN"
#define KABSL   "BLN"
#define KABU    "WHN"
#define KAMT    "RHN"
#define KAWF    "BHN"
#define KCT     "YHN"
#define KCEF    "TLU"
#define KCH     "BHN"
#define KDT     "GLN"
#define KET     "THN"
#define KEE     "YHR"
#define KFP     "GLN"
#define KFK     "BLN"
#define KIMT    "WHN"
#define KIWF    "BLN"
#define KLEF    "TLU"
#define KLID    "GLN"
#define KLI     "WLN"
#define KNEF    "TLU"
#define KNT     "GLN"
#define KPI     "BLN"
#define KPIN    "GLN"
#define KPT     "BLN"
#define KPS     "THN"
#define KPAC    "WLN"
#define KPUC    "BLN"
#define KRP     "WHN"
#define KSI     "WHN"
#define KSAC    "WLN"
#define KSUC    "BLN"
#define KVOI    "TLN"
#define KWMT    "YHN"
#define KWT     "RHN"
#define KWASL   "BLN"

// ***************************************** End defaults *******************************************

#define BLACK           COLOR_PAIR(0)
#define RED             COLOR_PAIR(1)
#define GREEN           COLOR_PAIR(2)
#define YELLOW          COLOR_PAIR(3)
#define BLUE            COLOR_PAIR(4)
#define MAGENTA         COLOR_PAIR(5)
#define TURQ            COLOR_PAIR(6)
#define WHITE           COLOR_PAIR(7)

namespace lspf {

const bool NOCHECK = false ;

struct lspfCommand
{
	string Command       ;
	int    RC            ;
	vector<string> reply ;
} ;

enum msgSET
{
	COND,
	UNCOND
} ;

enum s_paths
{
	s_ZMLIB,
	s_ZPLIB,
	s_ZTLIB,
	s_ZTABL
} ;

enum tbWRITE
{
	WRITE,
	NOWRITE
} ;

enum tbREP
{
	REPLACE,
	NOREPLACE
} ;

enum tbDISP
{
	SHARE,
	NON_SHARE
} ;

enum enqDISP
{
	SHR,
	EXC
} ;

enum enqSCOPE
{
	LOCAL,
	GLOBAL
} ;

enum WAIT_REASON
{
	WAIT_NONE,
	WAIT_OUTPUT,
	WAIT_SELECT,
	WAIT_USER
} ;

enum dataType
{
	INTEGER,
	STRING,
	ERROR
} ;

enum vdType
{
	DEFINED,
	IMPLICIT,
	ALL
} ;

enum readCHCK
{
	ROCHECK,
	NOROCHECK
} ;

enum vcMODE
{
	LOCATE,
	MOVE
} ;

enum poolType
{
	SHARED,
	PROFILE,
	BOTH,
	ASIS
} ;

enum srCOND
{
	s_EQ,
	s_NE,
	s_LE,
	s_LT,
	s_GE,
	s_GT
} ;

enum vTYPE
{
	SYSTEM,
	USER
} ;

enum attType
{
	AB,
	ABSL,
	ABU,
	AMT,
	AWF,
	CT,
	CEF,
	CH,
	DT,
	ET,
	EE,
	FP,
	FK,
	IMT,
	INPUT,
	IWF,
	LEF,
	LID,
	LI,
	NEF,
	NONE,
	NT,
	PI,
	PIN,
	PT,
	PS,
	PAC,
	PUC,
	RP,
	SI,
	SAC,
	SUC,
	VOI,
	WMT,
	WT,
	WASL,
	CHAR,
	DATAIN,
	DATAOUT,
	OUTPUT,
	TEXT
} ;

enum USR_ACTIONS
{
	USR_END,
	USR_ENTER,
	USR_EXIT,
	USR_CANCEL,
	USR_RETURN
} ;

}


class errblock
{
	public:
		string  msgid  ;
		string  msg1   ;
		string  sline  ;
		const string* dline ;
		string  val1   ;
		string  val2   ;
		string  val3   ;
		string  udata  ;
		uint    taskid ;
		uint    ptid   ;
		uint    RC     ;
		uint    RSN    ;
		uint    maxRC  ;
		bool    debug  ;
		bool    abend  ;
		bool    sCall  ;
		bool    panel  ;
		bool    dialog ;
	errblock()
	{
		msgid  = "" ;
		msg1   = "" ;
		sline  = "" ;
		dline  = NULL ;
		val1   = "" ;
		val2   = "" ;
		val3   = "" ;
		udata  = "" ;
		taskid = 0  ;
		ptid   = 0  ;
		RC     = 0  ;
		RSN    = 0  ;
		maxRC  = 0  ;
		debug  = false ;
		abend  = false ;
		sCall  = false ;
		panel  = false ;
		dialog = false ;
	}
	void clear()
	{
		msgid  = "" ;
		msg1   = "" ;
		sline  = "" ;
		dline  = NULL ;
		val1   = "" ;
		val2   = "" ;
		val3   = "" ;
		udata  = "" ;
		RC     = 0  ;
		RSN    = 0  ;
		maxRC  = 0  ;
		debug  = false ;
		panel  = false ;
		dialog = false ;
	}
	void setRC( uint i )
	{
		RC = i ;
	}
	void setRSN( uint i )
	{
		RSN = i ;
	}
	uint getRC()
	{
		return RC ;
	}
	bool RC0()
	{
		return ( RC == 0 ) ;
	}
	bool RC4()
	{
		return ( RC == 4 ) ;
	}
	bool RC8()
	{
		return ( RC == 8 ) ;
	}
	bool RC12()
	{
		return ( RC == 12 ) ;
	}
	bool RSN0()
	{
		return ( RSN == 0 ) ;
	}
	bool RSN4()
	{
		return ( RSN == 4 ) ;
	}
	void setmaxRC( uint i )
	{
		maxRC = i ;
	}
	void setmaxRC()
	{
		maxRC = max( RC, maxRC ) ;
	}
	uint getmaxRC()
	{
		return maxRC ;
	}
	void setDebugMode()
	{
		debug = true ;
	}
	bool debugMode()
	{
		return debug ;
	}
	void setAbending()
	{
		abend = true ;
	}
	bool abending()
	{
		return abend ;
	}
	bool error()
	{
		return RC > 8 ;
	}
	void setcall( const string& s )
	{
		msg1 = s ;
	}
	void setcall( const string& s1,
		      const string& s2,
		      uint i=20 )
	{
		msg1  = s1 ;
		msgid = s2 ;
		val1  = "" ;
		val2  = "" ;
		val3  = "" ;
		RC    = i  ;
	}
	void setcall( const string& s1,
		      const string& s2,
		      const string& s3,
		      uint i=20 )
	{
		msg1  = s1 ;
		msgid = s2 ;
		val1  = s3 ;
		val2  = "" ;
		val3  = "" ;
		RC    = i  ;
	}
	void setcall( const string& s1,
		      const string& s2,
		      const string& s3,
		      const string& s4,
		      uint i=20 )
	{
		msg1  = s1 ;
		msgid = s2 ;
		val1  = s3 ;
		val2  = s4 ;
		val3  = "" ;
		RC    = i  ;
	}
	void setcall( const string& s1,
		      const string& s2,
		      const string& s3,
		      const string& s4,
		      const string& s5,
		      uint i=20 )
	{
		msg1  = s1 ;
		msgid = s2 ;
		val1  = s3 ;
		val2  = s4 ;
		val3  = s5 ;
		RC    = i  ;
	}
	void seterror()
	{
		RC    = 20 ;
	}
	void seterror( const string& s1,
		       uint i=20 )
	{
		msgid = "PSYE019D" ;
		val1  = s1 ;
		val2  = "" ;
		RC    = i  ;
		if ( panel ) { copysrc() ; }
	}
	void seterror( const string& s1,
		       const string& s2,
		       uint i=20 )
	{
		msgid = "PSYE019D" ;
		val1  = s1 ;
		val2  = s2 ;
		RC    = i  ;
		if ( panel ) { copysrc() ; }
	}
	void seterrid( const string& s,
		       uint i=20 )
	{
		msgid = s  ;
		val1  = "" ;
		val2  = "" ;
		val3  = "" ;
		RC    = i  ;
		if ( panel ) { copysrc() ; }
	}
	void seterrid( const string& s1,
		       const string& s2,
		       uint i=20 )
	{
		msgid = s1 ;
		val1  = s2 ;
		val2  = "" ;
		val3  = "" ;
		RC    = i  ;
		if ( panel ) { copysrc() ; }
	}
	void seterrid( const string& s1,
		       const string& s2,
		       const string& s3,
		       uint i=20 )
	{
		msgid = s1 ;
		val1  = s2 ;
		val2  = s3 ;
		val3  = "" ;
		RC    = i  ;
		if ( panel ) { copysrc() ; }
	}
	void seterrid( const string& s1,
		       const string& s2,
		       const string& s3,
		       const string& s4,
		       uint i=20 )
	{
		msgid = s1 ;
		val1  = s2 ;
		val2  = s3 ;
		val3  = s4 ;
		RC    = i  ;
		if ( panel ) { copysrc() ; }
	}
	void setsrc( const string& s )
	{
		sline = s ;
		dline = NULL ;
	}
	void setsrc( const string* p )
	{
		dline = p  ;
		sline = "" ;
	}
	void copysrc()
	{
		if ( dline )
		{
			sline = *dline ;
			sline.erase( 0, sline.find_first_not_of( ' ' ) ) ;
			dline = NULL ;
		}
	}
	string getsrc()
	{
		return ( dline ) ? *dline : sline ;
	}
	void setpanelsrc()
	{
		panel  = true  ;
		dialog = false ;
	}
	bool panelsrc()
	{
		return panel ;
	}
	void setdialogsrc()
	{
		panel  = false ;
		dialog = true  ;
	}
	bool dialogsrc()
	{
		return dialog ;
	}
	void clearsrc()
	{
		dline = NULL ;
		sline = ""   ;
	}
	void setval( const string& s1 )
	{
		val1 = s1 ;
	}
	void setval( const string& s1,
		     const string& s2 )
	{
		val1 = s1 ;
		val2 = s2 ;
	}
	void setval( const string& s1,
		     const string& s2,
		     const string& s3 )
	{
		val1 = s1 ;
		val2 = s2 ;
		val3 = s3 ;
	}
	void setUserData( const string& s )
	{
		udata = s ;
	}
	const string& getUserData()
	{
		return udata ;
	}
	void setServiceCall()
	{
		sCall = true ;
	}
	bool ServiceCall()
	{
		return sCall ;
	}
	void settask( uint i )
	{
		taskid = i ;
	}
} ;


#define _quotes( a ) #a
#define quotes( a ) _quotes( a )


#define llog(t, s) \
{ \
lg->lock() ; \
(*lg) << microsec_clock::local_time() << \
" " << left( quotes(MOD_NAME), 10 ) << \
" 00000 " << t << " " << s ; \
lg->unlock() ; \
}

#ifdef DEBUG1
#define debug1( s ) \
{ \
lg->lock() ; \
(*lg) << microsec_clock::local_time() << \
" " << left( quotes(MOD_NAME), 10 ) << \
" 00000 D line: "  << __LINE__  << \
" >>L1 Function: " << __FUNCTION__ << \
" -  " << s ; \
lg->unlock() ; \
}
#else
#define debug1( s )
#endif


#ifdef DEBUG2
#define debug2( s ) \
{ \
lg->lock() ; \
(*lg) << microsec_clock::local_time() << \
" " << left( quotes(MOD_NAME), 10 ) << \
" 00000 D line: "  << __LINE__  << \
" >>L2 Function: " << __FUNCTION__ << \
" -  " << s ; \
lg->unlock() ; \
}
#else
#define debug2( s )
#endif
