/*
  Copyright (c) 2015 Daniel John Erdos

  This program is free software; you can redistribute it and/or modify x
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

using namespace std ;

/* *************************************************************************************************** */
/* *VERY* simple routine to add a bit of hilighting to C++ programs and various other types of files.  */
/* Match brackets and braces and hilight mis-matches                                                   */
/* *************************************************************************************************** */

#include <boost/exception/diagnostic_information.hpp>
#include "eHilight.h"

#undef  MOD_NAME
#define MOD_NAME HILIGHT


int taskid() { return 0 ; }

void addHilight( logger* lg, hilight& h, const string& line, string& shadow )
{
	try
	{
		hiRoutine[ h.hl_language ]( h, line, shadow ) ;
	}
	catch (...)
	{
		llog( "E", "An exception has occured hilighting line "<<line<<endl) ;
		exception_ptr ptr = current_exception() ;
		llog( "E", "Exception: " <<  boost::current_exception_diagnostic_information() << endl ) ;
		llog( "E", "Hilighting disabled"<<endl) ;
		h.hl_abend = true ;
	}
}


bool addHilight( const string& lang )
{
	return hiRoutine.count( lang ) > 0 ;
}


void addCppHilight( hilight& h, const string& line, string& shadow )
{
	// Special characters (yellow) must also be in the delims list.

	uint j  ;
	uint start ;

	size_t ln ;
	size_t p1 ;

	string w ;
	const string delims( " (){}=;<>+-*/|:!%#[]&,\\\"'" ) ;
	const string specials( "+-*/=<>&,|:!;%#[]\\" ) ;

	bool oQuote = false ;
	char Quote ;

	map<string, keyw>::iterator it ;

	int  oBrac1   = h.hl_oBrac1   ;
	int  oBrac2   = h.hl_oBrac2   ;
	bool oComment = h.hl_oComment ;

	ln = line.size() ;
	if ( ln == 0 ) { shadow = "" ; return ; }

	shadow = string( ln, E_GREEN ) ;

	start  = 0 ;
	p1     = 0 ;

	for ( j = 0 ; j < ln ; ++j )
	{
		if ( !oQuote && ln > 1 && j < ln-1 )
		{
			if ( line.compare( j, 2, "/*" ) == 0 )
			{
				oComment = true ;
				shadow.replace( j, 2, 2, E_TURQ ) ;
				++j ;
				continue ;
			}
			if ( oComment && line.compare( j, 2, "*/" ) == 0 )
			{
				oComment = false ;
				shadow.replace( j, 2, 2, E_TURQ ) ;
				++j ;
				continue ;
			}
			if ( !oComment && line.compare( j, 2, "//" ) == 0 )
			{
				shadow.replace( j, ln-j, ln-j, E_TURQ ) ;
				break ;
			}
		}

		if ( oQuote && ln > 1 && j < ln-1 && ( line.compare( j, 2, "\\\\" ) == 0 ) )
		{
			++j ;
			continue ;
		}

		if ( ln > 1 && j < ln-1 && ( line.compare( j, 2, "\\'" ) == 0 || line.compare( j, 2, "\\\"" ) == 0 ) )
		{
			++j ;
			continue ;
		}

		if ( oComment )
		{
			shadow[ j ] = E_TURQ ;
			continue ;
		}

		if ( !oQuote )
		{
			if ( line[ j ] == ' '  ) { continue ; }
			if ( line[ j ] == '"' || line[ j ] == '\'' )
			{
				oQuote = true ;
				Quote  = line[ j ] ;
				start  = j ;
				continue   ;
			}
		}
		else if ( line[ j ] == Quote )
		{
			oQuote = false ;
			shadow.replace( start, j-start+1, j-start+1, E_WHITE ) ;
			continue ;
		}
		if ( oQuote )
		{
			if ( j == ln-1 )
			{
				shadow.replace( start, j-start+1, j-start+1, G_WHITE ) ;
			}
			continue ;
		}
		if ( specials.find( line[ j ] ) != string::npos )
		{
			shadow[ j ] = E_YELLOW ;
			continue ;
		}
		p1 = line.find_first_of( delims, j ) ;
		if ( p1 != j || p1 == string::npos )
		{
			if ( p1 == string::npos ) { p1 = ln ; }
			w  = line.substr( j, p1-j ) ;
			it = kw_cpp.find( w ) ;
			if ( it != kw_cpp.end() )
			{
				shadow.replace( j, it->second.kw_len, it->second.kw_len, it->second.kw_col ) ;
			}
			if ( h.hl_ifLogic )
			{
				if ( w == "if" )
				{
					++h.hl_oIf ;
				}
				else if ( w == "else" )
				{
					if ( h.hl_oIf == 0 )
					{
						shadow.replace( j, 4, 4, G_WHITE ) ;
					}
					else if ( h.hl_oIf > 0 )
					{
						--h.hl_oIf ;
					}
				}
			}
			j = p1 - 1 ;
			continue   ;
		}
		if ( h.hl_Paren )
		{
			if ( line[ j ] == '(' ) { ++oBrac1 ; shadow[ j ] = oBrac1 % 7 + 6 ; continue ; }
			if ( line[ j ] == ')' )
			{
				if ( oBrac1 == 0 ) { shadow[ j ] = G_WHITE ; }
				else               { shadow[ j ] = oBrac1 % 7 + 6 ; --oBrac1 ; }
				continue ;
			}
		}
		if ( h.hl_doLogic )
		{
			if ( line[ j ] == '{' )
			{
				++oBrac2 ;
				shadow[ j ] = oBrac2 % 7 + 6 ;
				continue ;
			}
			else if ( line[ j ] == '}' )
			{
				if ( oBrac2 == 0 )
				{
					shadow[ j ] = G_WHITE ;
				}
				else if ( oBrac2 > 0 )
				{
					shadow[ j ] = oBrac2 % 7 + 6 ;
					--oBrac2 ;
					if ( oBrac2 == 0 ) { h.hl_oIf = 0 ; }
				}
				continue ;
			}
		}
	}
	h.hl_oBrac1   = oBrac1 ;
	h.hl_oBrac2   = oBrac2 ;
	h.hl_oComment = oComment ;
}


void addASMHilight( hilight& h, const string& line, string& shadow )
{
	uint i  ;
	uint wd ;
	uint start ;
	uint j  ;
	uint p1 ;
	uint ln ;

	string w ;

	char Quote  = h.hl_Quote  ;
	bool oQuote = h.hl_oQuote ;

	ln = line.size() ;
	if ( ln == 0 ) { shadow = "" ; return ; }

	start = 0 ;
	p1    = 0 ;

	if ( line[ 0 ] == '*' )
	{
		shadow = string( ln, E_TURQ ) ;
		return ;
	}

	shadow = string( ln, E_GREEN ) ;

	j = 0 ;
	if ( h.hl_continue )
	{
		wd = 3 ;
		h.hl_continue = false ;
		if ( oQuote )
		{
			p1 = line.find_first_not_of( ' ' ) ;
			i  = ( ln < 16 ) ? ( ln - p1 ) : ( 16 - p1 ) ;
			if ( p1 < 15 )
			{
				shadow.replace( p1, i, i, G_RED ) ;
				j     = 15 ;
				start = 15 ;
			}
		}
	}
	else
	{
		if ( line[ 0 ] != ' ' ) { wd = 1 ; }
		else                    { wd = 2 ; }
		oQuote = false ;
	}

	for ( ; j < ln ; ++j )
	{
		if ( wd == 4 )
		{
			shadow.replace( j, ln-j, ln-j, E_TURQ ) ;
			break ;
		}
		if ( ln > 1 && j < ln-2 && line.compare( j, 2, "\\'" ) == 0 )
		{
			++j ;
			continue ;
		}

		if ( !oQuote && ln > 1 && j < ln-2 )
		{
			if ( line.compare( j, 2, "L\'" ) == 0 )
			{
				shadow.replace( j, 2, 2, E_WHITE ) ;
				++j ;
				continue ;
			}
		}

		if ( !oQuote )
		{
			if ( line[ j ] == ' ' )
			{
				if ( j > 0 && line[ j - 1 ] != ' ' ) { ++wd ; }
				continue ;
			}
			if ( line[ j ] == '"' || line[ j ] == '\'' )
			{
				oQuote = true ;
				Quote  = line[ j ] ;
				start  = j ;
				continue   ;
			}
		}
		else if ( line[ j ] == Quote )
		{
			oQuote = false ;
			shadow.replace( start, j-start+1, j-start+1, E_WHITE ) ;
			continue ;
		}
		if ( oQuote )
		{
			if ( j == ln-1 )
			{
				shadow.replace( start, j-start+1, j-start+1, G_WHITE ) ;
			}
			continue ;
		}
		if ( wd == 1 )
		{
			shadow[ j ] = E_TURQ ;
		}
		else if ( wd == 2 )
		{
			shadow[ j ] = E_RED ;
		}
	}
	if ( ln > 71 )
	{
		shadow.replace( 71, ln-71, ln-71, E_RED ) ;
		if ( line[ 71 ] != ' ' )
		{
			h.hl_continue = true ;
			if ( oQuote )
			{
				shadow.replace( start, 71-start, 71-start, E_WHITE ) ;
			}
		}
	}
	h.hl_oQuote = oQuote ;
	h.hl_Quote  = Quote  ;
}


void addRxxHilight( hilight& h, const string& line, string& shadow )
{
	// Special characters (yellow) must also be in the delims list.

	uint ln    ;
	uint start ;
	uint j     ;

	size_t p1 ;

	string w ;

	const string delims( " ()=;<>+-*[]\"'/&|:%\\" ) ;
	const string specials( "=<>+-*[]/&|:%\\" ) ;

	bool oQuote = false ;
	char Quote ;

	map<string, keyw>::iterator it ;

	int  oBrac1   = h.hl_oBrac1   ;
	bool oComment = h.hl_oComment ;

	ln = line.size() ;
	if ( ln == 0 ) { shadow = "" ; return ; }

	shadow = string( ln, E_GREEN ) ;

	start  = 0 ;
	p1     = 0 ;

	for ( j = 0 ; j < ln ; ++j )
	{
		if ( !oQuote && ln > 1 && j < ln-1 )
		{
			if ( line.compare( j, 2, "/*" ) == 0 )
			{
				oComment = true ;
				shadow.replace( j, 2, 2, E_TURQ ) ;
				++j ;
				continue ;
			}
			if ( oComment && line.compare( j, 2, "*/" ) == 0 )
			{
				oComment = false ;
				shadow.replace( j, 2, 2, E_TURQ ) ;
				++j ;
				continue ;
			}
			if ( line.compare( j, 2, "--" ) == 0 )
			{
				oComment = false ;
				shadow.replace( j, ln-j, ln-j, E_TURQ ) ;
				break ;
			}
		}

		if ( ln > 1 && j < ln-1 && line.compare( j, 2, "\\'" ) == 0 )
		{
			++j ;
			continue ;
		}

		if ( oComment )
		{
			shadow[ j ] = E_TURQ ;
			continue ;
		}

		if ( !oQuote )
		{
			if ( line[ j ] == ' '  ) { continue ; }
			if ( line[ j ] == '"' || line[ j ] == '\'' )
			{
				oQuote = true ;
				Quote  = line[ j ] ;
				start  = j ;
				continue   ;
			}
		}
		else if ( line[ j ] == Quote )
		{
			oQuote = false ;
			shadow.replace( start, j-start+1, j-start+1, E_WHITE ) ;
			continue ;
		}
		if ( oQuote )
		{
			if ( j == ln-1 )
			{
				shadow.replace( start, j-start+1, j-start+1, G_WHITE ) ;
			}
			continue ;
		}
		if ( specials.find( line[ j ] ) != string::npos )
		{
			shadow[ j ] = E_YELLOW ;
			continue ;
		}
		p1 = line.find_first_of( delims, j ) ;
		if ( p1 != j || p1 == string::npos )
		{
			if ( p1 == string::npos ) { p1 = ln ; }
			w     = upper( line.substr( j, p1-j ) ) ;
			it    = kw_rexx.find( w ) ;
			if ( it != kw_rexx.end() )
			{
				shadow.replace( j, it->second.kw_len, it->second.kw_len, it->second.kw_col ) ;
			}
			if ( h.hl_ifLogic )
			{
				if ( w == "IF" )
				{
					++h.hl_oIf ;
				}
				else if ( w == "ELSE" )
				{
					if ( h.hl_oIf == 0 )
					{
						shadow.replace( j, 4, 4, G_WHITE ) ;
					}
					else if ( h.hl_oIf > 0 )
					{
						--h.hl_oIf ;
					}
				}
			}
			if ( h.hl_doLogic )
			{
				if ( w == "DO" || w == "SELECT" )
				{
					++h.hl_oDo ;
				}
				else if ( w == "END" )
				{
					if ( h.hl_oDo == 0 )
					{
						shadow.replace( j, 3, 3, G_WHITE ) ;
					}
					else
					{
						--h.hl_oDo ;
					}
				}
			}
			j = p1 - 1 ;
			continue ;
		}
		if ( h.hl_Paren )
		{
			if ( line[ j ] == '(' ) { ++oBrac1 ; shadow[ j ] = oBrac1 % 7 + 6 ; continue ; }
			if ( line[ j ] == ')' )
			{
				if ( oBrac1 == 0 ) { shadow[ j ] = G_WHITE ; }
				else               { shadow[ j ] = oBrac1 % 7 + 6 ; --oBrac1 ; }
				continue ;
			}
		}
	}
	h.hl_oBrac1   = oBrac1 ;
	h.hl_oComment = oComment ;
}


void addOthHilight( hilight& h, const string& line, string& shadow )
{
	// Highlight as a pseudo-PL/1 language.
	// Special characters (yellow) must also be in the delims list.

	uint ln ;
	uint start ;
	uint j  ;

	size_t p1 ;

	string w ;

	const string delims( " +-*/=<>()&|:\"'" ) ;
	const string specials( "+-*/=<>&|:" ) ;

	bool oQuote = false ;
	char Quote ;

	map<string, keyw>::iterator it ;

	int  oBrac1   = h.hl_oBrac1   ;
	bool oComment = h.hl_oComment ;

	ln = line.size() ;
	if ( ln == 0 ) { shadow = "" ; return ; }

	shadow = string( ln, E_GREEN ) ;

	start  = 0 ;
	p1     = 0 ;

	for ( j = 0 ; j < ln ; ++j )
	{
		if ( !oQuote && ln > 1 && j < ln-1 )
		{
			if ( line.compare( j, 2, "/*" ) == 0 )
			{
				oComment = true ;
				shadow.replace( j, 2, 2, E_TURQ ) ;
				++j ;
				continue ;
			}
			if ( oComment && line.compare( j, 2, "*/" ) == 0 )
			{
				oComment = false ;
				shadow.replace( j, 2, 2, E_TURQ ) ;
				++j ;
				continue ;
			}
		}

		if ( oComment )
		{
			shadow[ j ] = E_TURQ ;
			continue ;
		}

		if ( !oQuote )
		{
			if ( line[ j ] == ' '  ) { continue ; }
			if ( line[ j ] == '"' || line[ j ] == '\'' )
			{
				oQuote = true ;
				Quote  = line[ j ] ;
				start  = j ;
				continue   ;
			}
		}
		else if ( line[ j ] == Quote )
		{
			oQuote = false ;
			shadow.replace( start, j-start+1, j-start+1, E_WHITE ) ;
			continue ;
		}
		if ( oQuote )
		{
			if ( j == ln-1 )
			{
				shadow.replace( start, j-start+1, j-start+1, G_WHITE ) ;
			}
			continue ;
		}
		if ( specials.find( line[ j ] ) != string::npos )
		{
			shadow[ j ] = E_YELLOW ;
			continue ;
		}
		p1 = line.find_first_of( delims, j ) ;
		if ( p1 != j || p1 == string::npos )
		{
			if ( p1 == string::npos ) { p1 = ln ; }
			w     = upper( line.substr( j, p1-j ) ) ;
			it    = kw_other.find( w ) ;
			if ( it != kw_other.end() )
			{
				shadow.replace( j, it->second.kw_len, it->second.kw_len, it->second.kw_col ) ;
			}
			if ( h.hl_ifLogic )
			{
				if ( w == "IF" )
				{
					++h.hl_oIf ;
				}
				else if ( w == "ELSE" )
				{
					if ( h.hl_oIf == 0 )
					{
						shadow.replace( j, 4, 4, G_WHITE ) ;
					}
					else if ( h.hl_oIf > 0 )
					{
						--h.hl_oIf ;
					}
				}
			}
			if ( h.hl_doLogic )
			{
				if ( w == "DO" )
				{
					++h.hl_oDo ;
				}
				else if ( w == "END" )
				{
					if ( h.hl_oDo == 0 )
					{
						shadow.replace( j, 3, 3, G_WHITE ) ;
					}
					else
					{
						--h.hl_oDo ;
					}
				}
			}
			j = p1 - 1 ;
			continue ;
		}
		if ( h.hl_Paren )
		{
			if ( line[ j ] == '(' ) { ++oBrac1 ; shadow[ j ] = oBrac1 % 7 + 6 ; continue ; }
			if ( line[ j ] == ')' )
			{
				if ( oBrac1 == 0 ) { shadow[ j ] = G_WHITE ; }
				else               { shadow[ j ] = oBrac1 % 7 + 6 ; --oBrac1 ; }
				continue ;
			}
		}
	}
	h.hl_oBrac1   = oBrac1 ;
	h.hl_oComment = oComment ;
}


void addDefHilight( hilight& h, const string& line, string& shadow )
{
	// Highlight in a single colour, green.

	size_t ln ;

	ln = line.size() ;
	if ( ln == 0 ) { shadow = "" ; return ; }

	shadow = string( ln, E_GREEN ) ;
}


void addPanHilight( hilight& h, const string& line, string& shadow )
{
	// Special characters (yellow) must also be in the delims list.

	uint start ;
	uint j ;

	size_t ln ;
	size_t p1 ;

	string w ;

	const string delims( " =,&.()=<>!+-*|\"'" ) ;
	const string specials( "=,&.<>!*|" ) ;

	bool oQuote = false ;
	char Quote ;

	map<string, keyw>::iterator it ;

	int  oBrac1   = h.hl_oBrac1   ;
	bool oComment = h.hl_oComment ;

	ln = line.size() ;
	if ( ln == 0 ) { shadow = "" ; return ; }

	shadow = string( ln, E_GREEN ) ;

	start  = 0 ;
	p1     = 0 ;

	for ( j = 0 ; j < ln ; ++j )
	{
		if ( !oQuote && ln > 1 && j < ln-1 )
		{
			if ( line.compare( j, 2, "/*" ) == 0 )
			{
				oComment = true ;
				shadow.replace( j, 2, 2, E_TURQ ) ;
				++j ;
				continue ;
			}
			if ( oComment && line.compare( j, 2, "*/" ) == 0 )
			{
				oComment = false ;
				shadow.replace( j, 2, 2, E_TURQ ) ;
				++j ;
				continue ;
			}
			if ( oComment && j == 0 && upper(line).compare( 0, 11, ")ENDCOMMENT" ) == 0 )
			{
				oComment = false ;
				shadow.replace( 0, 11, 11, E_TURQ ) ;
				++j ;
				continue ;
			}
			if ( line.compare( j, 2, "--" ) == 0 || line.compare( j, 1, "#" ) == 0 )
			{
				oComment = false ;
				shadow.replace( j, ln-j, ln-j, E_TURQ ) ;
				break ;
			}
		}

		if ( ln > 1 && j < ln-1 && line.compare( j, 2, "\\'" ) == 0 )
		{
			++j ;
			continue ;
		}

		if ( oComment )
		{
			shadow[ j ] = E_TURQ ;
			continue ;
		}

		if ( !oQuote )
		{
			if ( line[ j ] == ' '  ) { continue ; }
			if ( line[ j ] == '"' || line[ j ] == '\'' )
			{
				oQuote = true ;
				Quote  = line[ j ] ;
				start  = j ;
				continue   ;
			}
		}
		else if ( line[ j ] == Quote )
		{
			oQuote = false ;
			shadow.replace( start, j-start+1, j-start+1, E_WHITE ) ;
			continue ;
		}
		if ( oQuote )
		{
			if ( j == ln-1 )
			{
				shadow.replace( start, j-start+1, j-start+1, G_WHITE ) ;
			}
			continue ;
		}
		if ( j == 0 && line[ j ] == ')' )
		{
			w  = word( line, 1 ) ;
			iupper( w ) ;
			it = kw_panel.find( w ) ;
			if ( it != kw_panel.end() )
			{
				shadow.replace( 0, it->second.kw_len, it->second.kw_len, it->second.kw_col ) ;
				j = it->second.kw_len ;
			}
			if ( w == ")COMMENT" ) { oComment = true ; }
			continue ;
		}
		if ( specials.find( line[ j ] ) != string::npos )
		{
			shadow[ j ] = E_YELLOW ;
			continue ;
		}
		p1 = line.find_first_of( delims, j ) ;
		if ( p1 != j || p1 == string::npos )
		{
			if ( p1 == string::npos ) { p1 = ln ; }
			w     = upper( line.substr( j, p1-j ) ) ;
			it    = kw_panel.find( w ) ;
			if ( it != kw_panel.end() )
			{
				shadow.replace( j, it->second.kw_len, it->second.kw_len, it->second.kw_col ) ;
			}
			if ( h.hl_ifLogic )
			{
				if ( w == "IF" )
				{
					++h.hl_oIf ;
				}
				else if ( w == "ELSE" )
				{
					if ( h.hl_oIf == 0 )
					{
						shadow.replace( j, 4, 4, G_WHITE ) ;
					}
					else if ( h.hl_oIf > 0 )
					{
						--h.hl_oIf ;
					}
				}
			}
			j = p1 - 1 ;
			continue ;
		}
		if ( h.hl_Paren )
		{
			if ( line[ j ] == '(' ) { ++oBrac1 ; shadow[ j ] = oBrac1 % 7 + 6 ; continue ; }
			if ( line[ j ] == ')' )
			{
				if ( oBrac1 == 0 ) { shadow[ j ] = G_WHITE ; }
				else               { shadow[ j ] = oBrac1 % 7 + 6 ; --oBrac1 ; }
				continue ;
			}
		}
	}
	h.hl_oBrac1   = oBrac1 ;
	h.hl_oComment = oComment ;
}
