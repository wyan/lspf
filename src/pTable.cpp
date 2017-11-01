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

#undef  MOD_NAME
#undef  LOGOUT
#define MOD_NAME pTable
#define LOGOUT   aplog


// *******************************************************************************************************************************
// *************************************************** TABLE SECTION *************************************************************
// *******************************************************************************************************************************

Table::~Table()
{
	// Free table row storage when deleting a table

	vector<vector<string>*>::iterator it ;

	for ( it = table.begin() ; it != table.end() ; it++ )
	{
		delete (*it) ;
	}
}


void Table::loadRow( errblock& err,
		     vector<string>* row )
{
	err.setRC( 0 ) ;

	if ( table.size() > 65535 )
	{
		err.seterrid( "PSYE013F" ) ;
		return ;
	}
	row->insert( row->begin(), d2ds( ++maxURID ) ) ;
	table.push_back( row ) ;
	changed = true ;
}


void Table::reserveSpace( int tot_rows )
{
	table.reserve( tot_rows ) ;
}


vector<vector<string>*>::iterator Table::getKeyItr( errblock& err,
						    fPOOL& funcPOOL )
{
	// Return the table row iterator of a row for a keyed table or table.end() if not found.
	// Use the column values from the function pool and set these to null if not found.

	// Set CRPX to the found row, in case the position is required (eg to set the CRP)

	int i ;

	string var ;

	vector<string> keys ;
	vector<vector<string>*>::iterator it ;

	keys.reserve( num_keys ) ;

	for ( i = 1 ; i <= num_keys ; i++ )
	{
		var = word( tab_keys, i ) ;
		keys.push_back( funcPOOL.get( err, 8, var ) ) ;
		if ( err.RC8() )
		{
			funcPOOL.put( err, var, "" ) ;
		}
		if ( err.error() ) { return table.end() ; }
	}

	CRPX = 1 ;
	for ( it = table.begin() ; it != table.end() ; it++ )
	{
		for ( i = 1 ; i <= num_keys ; i++ )
		{
			if ( (*it)->at( i ) != keys.at( i-1 ) )  { break ; }
		}
		if ( i > num_keys ) { return it ; }
		CRPX++ ;
	}
	return table.end() ;
}


void Table::loadfuncPOOL( errblock& err,
			  fPOOL& funcPOOL,
			  const string& tb_savenm )
{
	// Set row variables (including extension variables) in function pool from row pointed to by the CRP

	int i  ;
	int ws ;

	string tbelst ;

	vector<vector<string>*>::iterator it ;

	it = table.begin()   ;
	advance( it, CRP-1 ) ;

	for ( i = 1 ; i <= num_all ; i++ )
	{
		funcPOOL.put( err, word( tab_all, i ), (*it)->at( i ) ) ;
		if ( err.error() ) { return ; }
	}

	if ( (*it)->size() > num_all+1 )
	{
		tbelst = (*it)->at( num_all+1 ) ;
		for ( ws = words( tbelst ), i = 1 ; i <= ws ; i++ )
		{
			funcPOOL.put( err, word( tbelst, i ), (*it)->at( num_all+1+i ) ) ;
			if ( err.error() ) { return ; }
		}
		if ( tb_savenm != "" )
		{
			funcPOOL.put( err, tb_savenm, "("+ tbelst +")" ) ;
			if ( err.error() ) { return ; }
		}
	}
}




void Table::loadFields( errblock& err,
			fPOOL& funcPOOL,
			const string& tb_namelst,
			vector<string>* row )
{
	// Load row fields (including namelist but not the URID) from the function pool into string vector row

	int i  ;
	int ws ;

	string var ;

	for ( i = 1 ; i <= num_all ; i++ )
	{
		var = word( tab_all, i ) ;
		row->push_back( funcPOOL.get( err, 8, var ) ) ;
		if ( err.RC8() )
		{
			funcPOOL.put( err, var, "" ) ;
		}
		if ( err.error() ) { return ; }
	}

	if ( tb_namelst != "" )
	{
		row->push_back( space( tb_namelst ) ) ;
		for ( ws = words( tb_namelst ), i = 1 ; i <= ws ; i++ )
		{
			var = word( tb_namelst, i ) ;
			row->push_back( funcPOOL.get( err, 8, var ) ) ;
			if ( err.RC8() )
			{
				funcPOOL.put( err, var, "" ) ;
			}
			if ( err.error() ) { return ; }
		}
	}
}


void Table::tbadd( errblock& err,
		   fPOOL& funcPOOL,
		   string tb_namelst,
		   string tb_order,
		   int tb_num_of_rows )
{
	// Add a row to a table after the CRP (if not sorted) or in the sort position of a sorted table if
	// ORDER has been specified

	// RC = 0  Okay
	// RC = 8  Keyed tables only.  Row already exists, CRP set to TOP
	// RC = 16 Numeric conversion error
	// RC = 20 Severe error

	// Add extension variables as varname list + values to the row vector after defined keys and fields (start posn @ 1+all_flds)

	// If ORDER specified on a sorted table, sort the table again after adding the record and reset CRP

	string key  ;
	string val  ;
	string URID ;

	vector<string> keys ;
	vector<string>* row ;
	vector<vector<string>*>::iterator it ;

	iupper( tb_namelst ) ;
	iupper( tb_order )   ;

	err.setRC( 0 ) ;

	if ( table.size() > 65535 )
	{
		err.seterrid( "PSYE013F" ) ;
		return ;
	}

	if ( tb_num_of_rows > 0 )
	{
		table.reserve( table.size() + tb_num_of_rows ) ;
	}

	if ( tb_order == "" ) { sort_ir = "" ; }

	if ( num_keys > 0 )
	{
		if ( getKeyItr( err, funcPOOL ) != table.end() )
		{
			CRP = 0 ;
			err.setRC( 8 ) ;
			return ;
		}
	}

	row = new vector<string> ;
	row->clear() ;
	row->push_back( d2ds( ++maxURID ) ) ;

	loadFields( err, funcPOOL, tb_namelst, row ) ;
	if ( err.error() ) { delete row ; return ; }

	it = table.begin() ;
	advance( it, CRP ) ;
	it = table.insert( it, row ) ;
	CRP++ ;

	if ( tb_order == "ORDER" && sort_ir != "" )
	{
		URID = (*it)->at( 0 ) ;
		tbsort( err, sort_ir ) ;
		for ( it = table.begin() ; it != table.end() ; it++ )
		{
			CRP++ ;
			if ( URID == (*it)->at( 0 ) ) { break ; }
		}
	}
	changed = true ;
}


void Table::tbbottom( errblock& err,
		      fPOOL& funcPOOL,
		      string tb_savenm,
		      string tb_rowid_vn,
		      string tb_noread,
		      string tb_crp_name )
{
	// RC = 0  Okay
	// RC = 8  Table Empty.  CRP set to top
	// RC = 20 Severe error

	// ROWID name and CRP name are for output only

	err.setRC( 0 ) ;

	iupper( tb_savenm )   ;
	iupper( tb_rowid_vn ) ;
	iupper( tb_noread )   ;
	iupper( tb_crp_name ) ;

	if ( tb_savenm != "" && !isvalidName( tb_savenm ) )
	{
		err.seterrid( "PSYE013M", "SAVENAME", tb_savenm ) ;
		return ;
	}

	if ( tb_rowid_vn != "" && !isvalidName( tb_rowid_vn ) )
	{
		err.seterrid( "PSYE013M", "ROWID", tb_rowid_vn ) ;
		return ;
	}

	if ( tb_crp_name != "" && !isvalidName( tb_crp_name ) )
	{
		err.seterrid( "PSYE013M", "CRP", tb_crp_name ) ;
		return ;
	}

	if ( tb_noread != "" && tb_noread != "NOREAD" )
	{
		err.seterrid( "PSYE013M", "NOREAD", tb_noread ) ;
		return ;
	}

	if ( table.size() == 0 )
	{
		CRP = 0 ;
		err.setRC( 8 ) ;
		if ( tb_crp_name != "" )
		{
			funcPOOL.put( err, tb_crp_name, CRP ) ;
		}
		return  ;
	}

	CRP = table.size() ;

	if ( tb_savenm != "" )
	{
		funcPOOL.put( err, tb_savenm, "" ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_noread != "NOREAD" )
	{
		loadfuncPOOL( err, funcPOOL, tb_savenm ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_rowid_vn != "" )
	{
		funcPOOL.put( err, tb_rowid_vn, table.at( CRP-1 )->at( 0 ) ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_crp_name != "" )
	{
		funcPOOL.put( err, tb_crp_name, CRP ) ;
	}
}


void Table::tbdelete( errblock& err,
		      fPOOL& funcPOOL )
{
	// Delete a row in the table.  For keyed tables, this is the row pointed to by the current contents of the key variables
	// For non-keyed tables, this is the row pointed to by the CRP

	// CRP always points to the row before the one deleted

	// RC = 0  Okay
	// RC = 8  Keyed tables.  Row with key value does not exist.  CRP set to TOP
	//         Non-keyed tables.  CRP was at TOP (zero)
	// RC = 20 Severe error

	err.setRC( 0 ) ;

	vector<vector<string>*>::iterator it ;

	if ( num_keys > 0 )
	{
		it = getKeyItr( err, funcPOOL ) ;
		if ( it == table.end() )
		{
			CRP = 0 ;
			err.setRC( 8 ) ;
			return ;
		}
		delete (*it) ;
		table.erase( it ) ;
		CRP = CRPX - 1    ;
	}
	else
	{
		if ( CRP == 0 )
		{
			err.setRC( 8 ) ;
			return ;
		}
		else
		{
			CRP-- ;
			it = table.begin() ;
			advance( it, CRP ) ;
			delete (*it)       ;
			table.erase( it )  ;
		}
	}
	changed = true ;
}


void Table::tbexist( errblock& err,
		     fPOOL& funcPOOL )
{
	// Test for the existance of a row in a keyed table using the current value of the key variables.
	// CRP is positioned at this row if found else TOP.

	// RC = 0  Okay
	// RC = 8  Keyed tables.  Row does not exist.  CRP is set to TOP (zero)
	//         Non-keyed tables.  Not valid.  CRP is set to TOP (zero)
	// RC = 20 Severe error

	err.setRC( 0 ) ;

	CRP = 0 ;

	if ( num_keys == 0 ) { err.setRC( 8 ) ; return ; }

	if ( getKeyItr( err, funcPOOL ) == table.end() )
	{
		err.setRC( 8 ) ;
		return ;
	}
	CRP = CRPX ;
}


void Table::tbget( errblock& err,
		   fPOOL& funcPOOL,
		   string tb_savenm,
		   string tb_rowid_vn,
		   string tb_noread,
		   string tb_crp_name )
{
	// Access row in the table.  For table with keys, use the current value of the key in the dialogue variable.
	// For non-keyed tables, use the CRP

	// RC = 0  Okay
	// RC = 8  CRP was at TOP(zero) for non-keyed tables or key not found for keyed tables
	// RC = 20 Severe error

	// ROWID name and CRP name are for output only (not used for finding the record)

	vector<vector<string>*>::iterator it ;

	err.setRC( 0 ) ;

	iupper( tb_savenm )   ;
	iupper( tb_rowid_vn ) ;
	iupper( tb_noread )   ;
	iupper( tb_crp_name ) ;

	if ( tb_savenm != "" && !isvalidName( tb_savenm ) )
	{
		err.seterrid( "PSYE013M", "SAVENAME", tb_savenm ) ;
		return ;
	}

	if ( tb_rowid_vn != "" && !isvalidName( tb_rowid_vn ) )
	{
		err.seterrid( "PSYE013M", "ROWID", tb_rowid_vn ) ;
		return ;
	}

	if ( tb_crp_name != "" && !isvalidName( tb_crp_name ) )
	{
		err.seterrid( "PSYE013M", "CRP", tb_crp_name ) ;
		return ;
	}

	if ( tb_noread != "" && tb_noread != "NOREAD" )
	{
		err.seterrid( "PSYE013M", "NOREAD", tb_noread ) ;
		return ;
	}

	if ( num_keys > 0 )
	{
		it = getKeyItr( err, funcPOOL ) ;
		if ( it == table.end() )
		{
			CRP = 0 ;
		}
		else
		{
			CRP = CRPX ;
		}
	}

	if ( CRP == 0 )
	{
		if ( tb_crp_name != "" ) { funcPOOL.put( err, tb_crp_name, CRP ) ; }
		err.setRC( 8 ) ;
		return ;
	}

	if ( tb_savenm != "" )
	{
		funcPOOL.put( err, tb_savenm, "" ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_noread != "NOREAD" )
	{
		loadfuncPOOL( err, funcPOOL, tb_savenm ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_rowid_vn != "" )
	{
		funcPOOL.put( err, tb_rowid_vn, table.at( CRP-1 )->at( 0 ) ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_crp_name != "" )
	{
		funcPOOL.put( err, tb_crp_name, CRP ) ;
	}
}


void Table::tbmod( errblock& err,
		   fPOOL& funcPOOL,
		   string tb_namelst,
		   string tb_order )
{
	// Unconditionally update the current row in a table using the CRP.

	// tbmod - update row if match found on key (keyed tables),  else perform tbadd at the bottom of the table
	//         Non-keyed tables always same as a tbadd
	//         CRP always points to the row added/updated

	// RC = 0  Okay.  Keyed tables - row updated.  Non-keyed tables new row added
	// RC = 8  Row did not match - row added for keyed tables
	// RC = 16 Numeric conversion error
	// RC = 20 Severe error

	// Extension variables must be re-specified or they will be lost (tb_namelst)

	// If ORDER specified on a sorted table, sort again in case tbmod has changed the order and reset CRP

	string key  ;
	string val  ;
	string URID ;

	vector<string>* row ;
	vector<vector<string>*>::iterator it ;

	err.setRC( 0 ) ;

	if ( tb_order == "" ) { sort_ir = "" ; }

	iupper( tb_namelst ) ;
	iupper( tb_order )   ;

	if ( num_keys == 0 )
	{
		tbadd( err, funcPOOL, tb_namelst, tb_order, 0 ) ;
		return ;
	}

	it = getKeyItr( err, funcPOOL ) ;
	if ( it == table.end() )
	{
		CRP = table.size() ;
		tbadd( err, funcPOOL, tb_namelst, tb_order, 0 ) ;
		if ( err.RC0() ) { err.setRC( 8 ) ; }
	}
	else
	{
		row  = new vector<string> ;
		CRP  = CRPX ;
		URID = (*it)->at( 0 ) ;
		row->push_back( URID ) ;
		loadFields( err, funcPOOL, tb_namelst, row ) ;
		if ( err.error() ) { delete row ; return ; }
		(*it) = row ;
		if ( tb_order == "ORDER" && sort_ir != "" )
		{
			tbsort( err, sort_ir ) ;
			for ( it = table.begin() ; it != table.end() ; it++ )
			{
				CRP++ ;
				if ( URID == (*it)->at( 0 ) ) { break ; }
			}
		}
	}
	changed = true ;
}


void Table::tbput( errblock& err,
		   fPOOL& funcPOOL,
		   string tb_namelst,
		   string tb_order )
{
	// For keyed tables, key variables must match row at CRP

	// RC = 0   OK
	// RC = 8   Keyed tables - keys do not match current row or CRP was set to top.  CRP set to top (0)
	//          Non-keyed tables - CRP was at top
	// RC = 16  Numeric conversion error for sorted tables
	// RC = 20  Severe error

	// If ORDER specified on a sorted table, sort again in case tbput has changed the order and reset CRP

	int i ;

	string key  ;
	string val  ;
	string URID ;

	err.setRC( 0 ) ;

	vector<string>* row ;
	vector<vector<string>*>::iterator it ;

	if ( tb_order == "" ) { sort_ir = "" ; }

	iupper( tb_namelst ) ;
	iupper( tb_order )   ;

	if ( CRP == 0 ) { err.setRC( 8 ) ; return ; }

	it = table.begin() ;
	advance( it, CRP-1 ) ;

	row = new vector<string> ;

	URID = (*it)->at( 0 ) ;
	row->push_back( URID ) ;

	loadFields( err, funcPOOL, tb_namelst, row ) ;
	if ( err.error() ) { delete row ; return ; }

	for ( i = 1 ; i <= num_keys ; i++ )
	{
		if ( row->at( i ) != (*it)->at( i ) )
		{
			CRP = 0 ;
			err.setRC( 8 ) ;
			delete row ;
			return ;
		}
	}

	delete (*it) ;
	(*it) = row  ;

	if ( tb_order == "ORDER" && sort_ir != "" )
	{
		tbsort( err, sort_ir ) ;
		for ( it = table.begin() ; it != table.end() ; it++ )
		{
			CRP++ ;
			if ( URID == (*it)->at( 0 ) ) { break ; }
		}
	}
	changed = true ;
}


void Table::tbquery( errblock& err,
		     fPOOL& funcPOOL,
		     string tb_keyn,
		     string tb_varn,
		     string tb_rownn,
		     string tb_keynn,
		     string tb_namenn,
		     string tb_crpn,
		     string tb_sirn,
		     string tb_lstn,
		     string tb_condn,
		     string tb_dirn )
{
	err.setRC( 0 ) ;

	iupper( tb_keyn )   ;
	iupper( tb_varn )   ;
	iupper( tb_rownn )  ;
	iupper( tb_keynn )  ;
	iupper( tb_namenn ) ;
	iupper( tb_crpn )   ;
	iupper( tb_sirn )   ;
	iupper( tb_lstn )   ;
	iupper( tb_condn )  ;
	iupper( tb_dirn )   ;

	if ( tb_keyn != "" )  { funcPOOL.put( err, tb_keyn, tab_keys )  ; }
	if ( err.error() ) { return ; }
	if ( tb_varn != "" )  { funcPOOL.put( err, tb_varn, tab_flds )  ; }
	if ( err.error() ) { return ; }
	if ( tb_rownn != "" ) { funcPOOL.put( err, tb_rownn, table.size() ) ; }
	if ( err.error() ) { return ; }
	if ( tb_keynn != "" ) { funcPOOL.put( err, tb_keynn, num_keys ) ; }
	if ( err.error() ) { return ; }
	if ( tb_namenn != "" ) { funcPOOL.put( err, tb_namenn, num_flds ) ; }
	if ( err.error() ) { return ; }
	if ( tb_crpn != "" ) { funcPOOL.put( err, tb_crpn, CRP ) ; }
	if ( err.error() ) { return ; }
	if ( tb_sirn != "" ) { funcPOOL.put( err, tb_sirn, sort_ir ) ; }
	if ( err.error() ) { return ; }
	if ( tb_lstn != "" ) { funcPOOL.put( err, tb_lstn, sa_namelst ) ; }
	if ( err.error() ) { return ; }
	if ( tb_condn != "" ) { funcPOOL.put( err, tb_condn, sa_cond_pairs ) ; }
	if ( err.error() ) { return ; }
	if ( tb_dirn != "" ) { funcPOOL.put( err, tb_dirn, sa_dir ) ; }
	if ( err.error() ) { return ; }
}


void Table::tbsarg( errblock& err,
		    fPOOL& funcPOOL,
		    string tb_namelst,
		    string tb_dir,
		    string tb_cond_pairs )
{
	// Notes:  Current value of variables are used if not blank.  Condition is EQ
	// ARGLIST(namelst) is for extension variables, the values of which will be used in the search

	int  p1 ;
	int  p2 ;
	int  i  ;
	int  ws ;

	bool loope  ;

	string var  ;
	string val  ;
	string cond ;

	tbsearch t  ;

	err.setRC( 0 ) ;

	iupper( tb_namelst ) ;
	iupper( tb_dir )     ;
	iupper( trim( tb_cond_pairs ) ) ;

	if ( tb_cond_pairs != "" )
	{
		if ( tb_cond_pairs.front() != '(' || tb_cond_pairs.back() != ')' )
		{
			err.seterrid( "PSYE013O", tb_cond_pairs ) ;
			return ;
		}
		tb_cond_pairs = tb_cond_pairs.substr( 1, tb_cond_pairs.size()-2 ) ;
	}

	sarg.clear() ;

	if ( tb_dir == "" ) { tb_dir = "NEXT" ; }

	if ( tb_dir != "NEXT" && tb_dir != "PREVIOUS" )
	{
		err.seterrid( "PSYE013P", tb_dir ) ;
		return ;
	}

	for ( i = 1 ; i <= num_all ; i++ )
	{
		var = word( tab_all, i ) ;
		val = funcPOOL.get( err, 8, var ) ;
		if ( err.error() ) { return ; }
		if ( val == "" ) { continue ; }
		if ( val.back() == '*' ) { t.tbs_gen = true  ; t.tbs_vsize = val.size()-1 ; t.tbs_val = val.substr( 0, t.tbs_vsize ) ; }
		else                     { t.tbs_gen = false ; t.tbs_val = val  ;                                                      }
		t.tbs_cond  = s_EQ  ;
		t.tbs_ext   = false ;
		sarg[ var ] = t     ;
	}

	for ( ws = words( tb_namelst ), i = 1 ; i <= ws ; i++ )
	{
		var = word( tb_namelst, i ) ;
		if ( wordpos( var, tab_all ) > 0 )
		{
			err.seterrid( "PSYE013Q" ) ;
			return ;
		}
		val = funcPOOL.get( err, 8, var ) ;
		if ( err.RC8() )
		{
			funcPOOL.put( err, var, "" ) ;
			val = "" ;
		}
		else if ( err.error() ) { return ; }
		if ( val.back() == '*' ) { t.tbs_gen = true  ; t.tbs_vsize = val.size()-1 ; t.tbs_val = val.substr( 0, t.tbs_vsize ) ; }
		else                     { t.tbs_gen = false ; t.tbs_val = val  ;                                                      }
		t.tbs_cond  = s_EQ ;
		t.tbs_ext   = true ;
		sarg[ var ] = t    ;
	}

	p2    = 0     ;
	loope = false ;
	while ( true )
	{
		if ( tb_cond_pairs == "" ) { break ; }
		p1 = tb_cond_pairs.find( ',', p2 ) ;
		if ( p1 == string::npos )
		{
			err.seterrid( "PSYE013R" ) ;
			return ;
		}
		var = tb_cond_pairs.substr( p2, p1-p2 ) ;
		if ( wordpos( var, (tab_all + " " + tb_namelst ) ) == 0 )
		{
			err.seterrid( "PSYE013S", var ) ;
			return ;
		}
		p1++ ;
		p2 = tb_cond_pairs.find( ',', p1 ) ;
		if ( p2 == string::npos )
		{
			cond  = tb_cond_pairs.substr( p1 ) ;
			loope = true ;
		}
		else
		{
			cond = tb_cond_pairs.substr( p1, p2-p1 ) ;
		}
		p2++ ;
		if ( (sarg.find( var ) != sarg.end()) && !sarg[ var ].tbsSetcon( cond ) )
		{
			err.seterrid( "PSYE013T", cond ) ;
			return ;
		}
		if ( loope ) { break ; }
	}

	sa_dir        = tb_dir ;
	sa_namelst    = tb_namelst ;
	sa_cond_pairs = tb_cond_pairs ;
}


void Table::tbscan( errblock& err,
		    fPOOL& funcPOOL,
		    string tb_namelst,
		    string tb_savenm,
		    string tb_rowid_vn,
		    string tb_dir,
		    string tb_noread,
		    string tb_crp_name,
		    string tb_condlst )
{
	// Scan table from current CRP according to parameters tb_namelst/tb_condlst/tb_dir if specified or similar values from a previous tbsarg call
	// tb_namelst contains key, name and extenstion variables.
	// Scan on extension variables not currently supported.  Only one search argument/condition currently supported

	// RC = 0  Okay. Row found
	// RC = 8  Row not found

	// Current value of variables not in NAMELST make no difference to search
	// SAVENM is blank if there are no extension variables or if NOREAD is specified.  Not specifying var leaves it unchanged from before (obviously)
	// NULL values are not ignored in namelst.

	int  i   ;
	int p1   ;
	int ws   ;
	int size ;

	string val     ;
	string var     ;
	string tbelst  ;
	string s_dir   ;
	int    s_match ;
	bool   s_next  ;
	bool   found   ;
	bool   loope   ;

	tbsearch t     ;

	map<string, tbsearch> scan ;
	map<string, tbsearch>::iterator it ;

	err.setRC( 0 ) ;

	iupper( tb_namelst )  ;
	iupper( tb_savenm )   ;
	iupper( tb_rowid_vn ) ;
	iupper( tb_dir )      ;
	iupper( tb_noread )   ;
	iupper( tb_crp_name ) ;

	if ( tb_dir == "" ) { tb_dir = "NEXT" ; }

	if ( (tb_dir != "NEXT") && (tb_dir != "PREVIOUS") )
	{
		err.seterrid( "PSYE013P", tb_dir ) ;
		return ;
	}

	if ( tb_noread != "" && tb_noread != "NOREAD" )
	{
		err.seterrid( "PSYE013M", "NOREAD", tb_noread ) ;
		return ;
	}

	if ( tb_savenm != "" && !isvalidName( tb_savenm ) )
	{
		err.seterrid( "PSYE013M", "SAVENAME", tb_savenm ) ;
		return ;
	}

	if ( tb_rowid_vn != "" && !isvalidName( tb_rowid_vn ) )
	{
		err.seterrid( "PSYE013M", "ROWID", tb_rowid_vn ) ;
		return ;
	}

	if ( tb_crp_name != "" && !isvalidName( tb_crp_name ) )
	{
		err.seterrid( "PSYE013M", "CRP", tb_crp_name ) ;
		return ;
	}

	if ( tb_namelst == "" )
	{
		if ( sarg.size() == 0 )
		{
			err.seterrid( "PSYE013U" ) ;
			return ;
		}
		scan  = sarg   ;
		s_dir = sa_dir ;
	}
	else
	{
		if ( words( tb_condlst ) > words( tb_namelst ) )
		{
			err.seterrid( "PSYE013V" ) ;
			return  ;
		}
		for ( ws = words( tb_namelst ), i = 1 ; i <= ws ; i++ )
		{
			var = word( tb_namelst, i ) ;
			val = funcPOOL.get( err, 8, var ) ;
			if ( err.error() ) { return ; }
			if ( err.RC8() )
			{
				funcPOOL.put( err, var, "" ) ;
				val = "" ;
			}
			t.tbs_ext = false ;
			if ( val.back() == '*' )
			{
				t.tbs_gen   = true ;
				t.tbs_vsize = val.size()-1 ;
				t.tbs_val   = val.substr( 0, t.tbs_vsize ) ;
			}
			else
			{
				t.tbs_gen = false ;
				t.tbs_val = val   ;
			}
			scan[ var ] = t ;
			if ( !scan[ var ].tbsSetcon( word( tb_condlst, i )) )
			{
				err.seterrid( "PSYE013W" ) ;
				return  ;
			}
		}
		s_dir = tb_dir ;
	}

	found   = false ;
	s_next  = ( s_dir == "NEXT" ) ;
	size    = table.size()        ;

	while ( true )
	{
		if ( s_next ) { CRP++ ; if ( CRP > size ) break ; }
		else          { CRP-- ; if ( CRP < 1    ) break ; }
		s_match = 0     ;
		loope   = false ;
		for ( it = scan.begin() ; it != scan.end() ; it++ )
		{
			p1 = wordpos( it->first, tab_all ) ;
			if ( p1 == 0 )
			{
				if ( table.at( CRP-1 )->size() == num_all+1 ) { break ; }
				tbelst = table.at( CRP-1 )->at( 1+num_all ) ;
				p1 = wordpos( it->first, tbelst ) ;
				if ( p1 == 0 ) { break ; }
				p1 = 1 + num_all + p1 ;
			}
			if ( it->second.tbs_gen )
			{
				switch ( it->second.tbs_cond )
				{
				case s_EQ:
					if ( table.at( CRP-1 )->at( p1 ).substr( 0 , it->second.tbs_vsize ) != it->second.tbs_val ) { loope = true ; }
					break ;
				case s_NE:
					if ( table.at( CRP-1 )->at( p1 ).substr( 0 , it->second.tbs_vsize ) == it->second.tbs_val ) { loope = true ; }
					break ;
				case s_LE:
					if ( table.at( CRP-1 )->at( p1 ).substr( 0 , it->second.tbs_vsize )  > it->second.tbs_val ) { loope = true ; }
					break ;
				case s_LT:
					if ( table.at( CRP-1 )->at( p1 ).substr( 0 , it->second.tbs_vsize ) >= it->second.tbs_val ) { loope = true ; }
					break ;
				case s_GE:
					if ( table.at( CRP-1 )->at( p1 ).substr( 0 , it->second.tbs_vsize )  < it->second.tbs_val ) { loope = true ; }
					break ;
				case s_GT:
					if ( table.at( CRP-1 )->at( p1 ).substr( 0 , it->second.tbs_vsize ) <= it->second.tbs_val ) { loope = true ; }
				}
			}
			else
			{
				switch ( it->second.tbs_cond )
				{
				case s_EQ:
					if ( table.at( CRP-1 )->at( p1 ) != it->second.tbs_val ) { loope = true ; }
					break ;
				case s_NE:
					if ( table.at( CRP-1 )->at( p1 ) == it->second.tbs_val ) { loope = true ; }
					break ;
				case s_LE:
					if ( table.at( CRP-1 )->at( p1 )  > it->second.tbs_val ) { loope = true ; }
					break ;
				case s_LT:
					if ( table.at( CRP-1 )->at( p1 ) >= it->second.tbs_val ) { loope = true ; }
					break ;
				case s_GE:
					if ( table.at( CRP-1 )->at( p1 )  < it->second.tbs_val ) { loope = true ; }
					break ;
				case s_GT:
					if ( table.at( CRP-1 )->at( p1 ) <= it->second.tbs_val ) { loope = true ; }
				}
			}
			if ( loope ) { break ; }
			s_match++ ;
		}
		if ( s_match == scan.size() ) { found = true ; break ; }
	}
	if ( !found )
	{
		CRP = 0 ;
		if ( tb_crp_name != "" )
		{
			funcPOOL.put( err, tb_crp_name, CRP ) ;
			if ( err.error() ) { return ; }
		}
		err.setRC( 8 ) ;
		return ;
	}

	if ( tb_savenm != "" )
	{
		funcPOOL.put( err, tb_savenm, "" ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_noread != "NOREAD" )
	{
		loadfuncPOOL( err, funcPOOL, tb_savenm ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_crp_name != "" )
	{
		funcPOOL.put( err, tb_crp_name, CRP ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_rowid_vn != "" )
	{
		funcPOOL.put( err, tb_rowid_vn, table.at( CRP-1 )->at( 0 ) ) ;
		if ( err.error() ) { return ; }
	}
}


void Table::tbskip( errblock& err,
		    fPOOL& funcPOOL,
		    int num,
		    string tb_savenm,
		    string tb_rowid_vn,
		    string tb_rowid,
		    string tb_noread,
		    string tb_crp_name )
{
	// Move CRP to a position in the table and read the row into the dialogue variables
	// Position using tb_rowid (URID) if specified, else use num

	// RC = 0  Okay
	// RC = 8  CRP would be outside the table
	// RC = 12 Table not open
	// RC = 16 Truncation has occured
	// RC = 20 Severe error

	int i ;

	string val ;
	string var ;

	vector<vector<string>*>::iterator it ;

	err.setRC( 0 ) ;

	iupper( tb_savenm )   ;
	iupper( tb_rowid_vn ) ;
	iupper( tb_noread )   ;
	iupper( tb_crp_name ) ;

	if ( tb_savenm != "" && !isvalidName( tb_savenm ) )
	{
		err.seterrid( "PSYE013M", "SAVENAME", tb_savenm ) ;
		return ;
	}

	if ( tb_rowid_vn != "" && !isvalidName( tb_rowid_vn ) )
	{
		err.seterrid( "PSYE013M", "ROWID", tb_rowid_vn ) ;
		return ;
	}

	if ( tb_crp_name != "" && !isvalidName( tb_crp_name ) )
	{
		err.seterrid( "PSYE013M", "CRP", tb_crp_name ) ;
		return ;
	}

	if ( tb_noread != "" && tb_noread != "NOREAD" )
	{
		err.seterrid( "PSYE013M", "NOREAD", tb_noread ) ;
		return ;
	}

	if ( tb_rowid != "" )
	{
		CRP = 1 ;
		for ( it = table.begin() ; it != table.end() ; it++ )
		{
			if ( tb_rowid == (*it)->at( 0 ) ) { break ; }
			CRP++ ;
		}
		if ( it == table.end() )
		{
			CRP = 0 ;
			err.setRC( 8 ) ;
			return ;
		}
	}
	else
	{
		i = CRP + num  ;
		if ( ( i < 1 ) || ( i > table.size() ) ) { err.setRC( 8 ) ; return ; }
		CRP = i ;
	}

	if ( tb_savenm != "" )
	{
		funcPOOL.put( err, tb_savenm, "" ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_noread != "NOREAD" )
	{
		loadfuncPOOL( err, funcPOOL, tb_savenm ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_rowid_vn != "" )
	{
		funcPOOL.put( err, tb_rowid_vn, table.at( CRP-1 )->at( 0 ) ) ;
		if ( err.error() ) { return ; }
	}

	if ( tb_crp_name != "" )
	{
		funcPOOL.put( err, tb_crp_name, CRP ) ;
	}
}


void Table::tbsort( errblock& err,
		    string tb_fields )
{
	// FIELD
	// FIELD,C
	// FIELD,C,A,FIELD2
	// FIELD,C,A,FIELD2,N
	// FIELD,C,A,FIELD2,N,D

	int f1 ;
	int p1 ;

	string s_fields ;
	string s_temp   ;

	vector<string> s_parm ;
	vector<int> s_field   ;
	vector<bool> s_char   ;
	vector<bool> s_asc    ;

	int nsort ;

	string temp ;

	err.setRC( 0 ) ;

	iupper( tb_fields ) ;
	temp = tb_fields    ;

	while ( true )
	{
		p1 = tb_fields.find( ',' ) ;
		if ( p1 == string::npos )
		{
			s_parm.push_back( trim( tb_fields ) ) ;
			break ;
		}
		else
		{
			s_parm.push_back( strip( tb_fields.substr( 0, p1 ) ) ) ;
			tb_fields.erase( 0, p1+1 ) ;
		}
	}

	s_fields = "" ;
	while ( true )
	{
		if ( s_parm.size() == 0 ) { break ; }
		s_temp   = s_parm.front() ;
		s_fields = s_temp + " " + s_fields ;
		f1  = wordpos( s_temp, tab_all ) ;
		if ( f1 == 0 )
		{
			err.seterrid( "PSYE013X" ) ;
			return ;
		}
		s_field.push_back( f1 ) ;
		if ( s_parm.size() == 1 ) { break ; }
		s_parm.erase( s_parm.begin() ) ;

		s_temp = s_parm.front() ;
		if ( s_temp != "C" && s_temp != "N" )
		{
			err.seterrid( "PSYE013X" ) ;
			return ;
		}
		s_char.push_back( (s_temp == "C") ) ;
		if ( s_parm.size() == 1 ) { break ; }
		s_parm.erase( s_parm.begin() ) ;

		s_temp = s_parm.front() ;
		if ( s_temp != "A" && s_temp != "D" )
		{
			err.seterrid( "PSYE013X" ) ;
			return ;
		}
		s_asc.push_back( (s_temp == "A") ) ;
		s_parm.erase( s_parm.begin() ) ;
	}

	s_char.push_back( true ) ;
	s_asc.push_back( true )  ;

	nsort   = s_field.size() ;
	sort_ir = temp ;

	sort( table.begin(), table.end(),
		[ &s_field, &s_char, &s_asc, nsort ]( const vector<string>* a, const vector<string>* b )
		{
			for ( int i = 0 ; i < nsort ; i++ )
			{
				int j = s_field[ i ] ;
				if ( s_char[ i ] )
				{
					 if ( a->at( j ) == b->at( j ) ) { continue ; }
					 if ( s_asc[ i ] )
					 {
						 return a->at( j ) < b->at( j ) ;
					 }
					 else
					 {
						 return a->at( j ) > b->at( j ) ;
					 }
				}
				else
				{
					 int ia = ds2d( a->at( j ) ) ;
					 int ib = ds2d( b->at( j ) ) ;
					 if ( ia == ib ) { continue ; }
					 if ( s_asc[ i ] )
					 {
						 return ia < ib ;
					 }
					 else
					 {
						 return ia > ib ;
					 }
				}
			}
			return false ;
		} ) ;
	CRP = 0 ;
}


void Table::tbtop( errblock& err )
{
	err.setRC( 0 ) ;
	CRP = 0 ;
}


void Table::tbvclear( errblock& err,
		      fPOOL& funcPOOL )
{
	for ( int i = 1 ; i <= num_all ; i++ )
	{
		funcPOOL.put( err, word( tab_all, i ), "" ) ;
		if ( err.error() ) { return ; }
	}
}


void Table::fillfVARs( errblock& err,
		       fPOOL& funcPOOL,
		       const string& clear_flds,
		       bool scan,
		       int  depth,
		       int  posn )
{
	// Fill the function pool variables ( of the form table_fieldname.line ) from the table for depth lines
	// starting as table position posn. (Use CRP position if posn not specified)
	// Create function pool variable .ZURID.line to hold the URID of the table row corresponding to that screen line

	// BUGS:  Should only do fields on the tbmodel statement instead of all fields in the row (inc. extension variables)
	//        SCAN not supported yet

	int j    ;
	int k    ;
	int l    ;
	int size ;

	string var    ;
	string enames ;
	string sufx   ;

	vector<string>* row ;
	vector<vector<string>*>::iterator it ;

	err.setRC( 0 ) ;

	size = table.size() ;

	if ( posn == -1 ) { posn = CRP ; }
	if ( posn == 0  ) { posn = 1   ; }

	funcPOOL.put( err, "ZTDTOP", posn )  ;
	if ( err.error() ) { return ; }
	funcPOOL.put( err, "ZTDROWS", size ) ;
	if ( err.error() ) { return ; }

	for ( l = words( clear_flds ), k = 1 ; k <= l ; k++ )
	{
		for ( var = word( clear_flds, k ), j = 0 ; j < depth ; j++ )
		{
			funcPOOL.put( err, var + "." + d2ds( j ), "", NOCHECK ) ;
			if ( err.error() ) { return ; }
		}
	}

	row = new vector<string> ;
	row->push_back( ".ZURID" ) ;
	for ( k = 1 ; k <= num_all ; k++ )
	{
		row->push_back( word( tab_all, k ) ) ;
	}

	it = table.begin() ;
	advance( it, posn-1 ) ;

	for ( k = 0 ; k < depth && it != table.end() ; k++, it++ )
	{
		sufx = "." + d2ds( k ) ;
		for ( j = 0 ; j <= num_all ; j++ )
		{
			funcPOOL.put( err, row->at( j ) + sufx, (*it)->at( j ), NOCHECK ) ;
			if ( err.error() ) { return ; }
		}
		if ( (*it)->size() > num_all+1 )
		{
			enames = (*it)->at( num_all+1 ) ;
			for ( l = 1, j = num_all+2 ; j < (*it)->size() ; j++, l++ )
			{
				funcPOOL.put( err, word( enames, l ) + sufx, (*it)->at( j ), NOCHECK ) ;
				if ( err.error() ) { return ; }
			}
		}
	}

	for ( ; k < depth ; k++ )
	{
		sufx = "." + d2ds( k ) ;
		for ( j = 0 ; j <= num_all ; j++ )
		{
			funcPOOL.put( err, row->at( j ) + sufx, "", NOCHECK ) ;
			if ( err.error() ) { return ; }
		}
	}
}


void Table::saveTable( errblock& err,
		       const string& m_name,
		       const string& m_path )
{
	// Save table to disk.
	// Version 2 file format adds extension variable support and record/file end markers, 0xFF.

	string s ;
	int i    ;
	int j    ;
	int k    ;
	int size ;
	int evar ;
	std::ofstream otable ;

	err.setRC( 0 ) ;

	s = m_path != "" ? m_path : tab_path ;

	if ( s.back() != '/' ) { s += "/" ; }

	if ( exists( s ) )
	{
		if ( !is_directory( s ) )
		{
			err.seterrid( "PSYE013J", s ) ;
			return  ;
		}
	}
	else
	{
		err.seterrid( "PSYE013K", s, 16 ) ;
		return  ;
	}

	s += m_name ;
	if ( exists( s ) )
	{
		if ( !is_regular_file( s ) )
		{
			err.seterrid( "PSYE013L", s ) ;
			return  ;
		}
	}

	debug2( "Saving table "+ m_name +" in "+ s <<endl ) ;

	size = table.size() ;
	otable.open( s.c_str(), ios::binary | ios::out ) ;

	otable << (char)00  ;  //
	otable << (char)133 ;  // x085 denotes a table
	otable << (char)2   ;  // Table file format.  Version 2 (extension variable support)
	otable << (char)44  ;  // Header length
	otable << "HDR                                         " ;
	otable << (char)01  ;  // Number of fields following the header record (only the Sort Information Record for now)
	otable << (char)sort_ir.size() ;
	otable << sort_ir              ;
	otable << (char)( size >> 8 )  ;
	otable << (char)( size )       ;
	otable << (char)num_keys       ;
	otable << (char)num_flds       ;
	for ( j = 1 ; j <= num_all ; j ++ )
	{
		otable << (char)word( tab_all, j ).size() ;
		otable << word( tab_all, j ) ;
	}
	for ( i = 0 ; i < size ; i++ )
	{
		for ( j = 1 ; j <= num_all ; j++ )
		{
			k = table.at( i )->at( j ).size() ;
			otable << (char)( k >> 8 )        ;
			otable << (char)( k  )            ;
			otable << table.at( i )->at( j )  ;
		}
		evar = table.at( i )->size() - num_all - 1 ;
		otable << (char)( evar >> 8 ) ;
		otable << (char)( evar  )     ;
		for ( ; j < table.at( i )->size() ; j++ )
		{
			k = table.at( i )->at( j ).size() ;
			otable << (char)( k >> 8 )        ;
			otable << (char)( k  )            ;
			otable << table.at( i )->at( j )  ;
		}
		otable << (char)0xFF ;
	}
	otable << (char)0xFF ;
	otable.close() ;
	resetChanged() ;
}


void Table::cmdsearch( errblock& err,
		       fPOOL& funcPOOL,
		       const string& cmd )
{
	// cmdsearch is not part of the normal table services for application.
	// It's used for retrieving abbreviated commands from a command table.
	// Use tbsarg/tbscan for normal applications

	// RC = 0  Okay
	// RC = 8  Commmand not found
	// RC = 20 Severe error

	int trunc ;

	vector<vector<string>*>::iterator it ;

	err.setRC( 0 ) ;

	if ( !tab_cmds )
	{
		llog( "E", "cmdsearch issued for a non-command table." <<endl ) ;
		err.seterror() ;
		return  ;
	}

	for ( it = table.begin() ; it != table.end() ; it++ )
	{
		trunc = ds2d( (*it)->at( 2 ) ) ;
		if ( trunc == 0 )
		{
			if ( (*it)->at( 1 ) == cmd ) { break ; }
		}
		else
		{
			if ( abbrev( (*it)->at( 1 ), cmd, trunc ) ) { break ; }
		}
	}

	if ( it == table.end() )
	{
		err.setRC( 8 ) ;
		return ;
	}

	funcPOOL.put( err, "ZCTVERB", (*it)->at( 1 ) ) ;
	if ( err.error() ) { return ; }

	funcPOOL.put( err, "ZCTTRUNC", (*it)->at( 2 ) ) ;
	if ( err.error() ) { return ; }

	funcPOOL.put( err, "ZCTACT", (*it)->at( 3 ) ) ;
	if ( err.error() ) { return ; }

	funcPOOL.put( err, "ZCTDESC", (*it)->at( 4 ) ) ;
	if ( err.error() ) { return ; }
}


// *******************************************************************************************************************************
// *************************************************** TABLE MANAGER SECTION *****************************************************
// *******************************************************************************************************************************

tableMGR::~tableMGR()
{
	map<string, Table*>::iterator it ;

	for ( it = tables.begin() ; it != tables.end() ; it++ )
	{
		delete it->second ;
	}
}


void tableMGR::createTable( errblock& err,
			    const string& tb_name,
			    string keys, string flds,
			    tbREP m_REP,
			    tbWRITE m_WRITE,
			    const string& m_path,
			    tbDISP m_DISP )
{
	// RC =  0 Normal
	// RC =  4 Normal - duplicate table replaced
	// RC =  8 REPLACE not specified and table exists, or REPLACE specified with table open in SHARE
	// RC = 12 Table in use
	// RC = 20 Severe error

	err.setRC( 0 ) ;

	iupper( keys ) ;
	iupper( flds ) ;

	if ( err.debugMode() )
	{
		debug1( "Creating table >>"+ tb_name +"<<" <<endl ) ;
		debug1( "     with keys >>"+ keys    +"<<" <<endl ) ;
		debug1( "    and fields >>"+ flds    +"<<" <<endl ) ;
		debug1( "          path >>"+ m_path  +"<<" <<endl ) ;
	}

	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it != tables.end() )
	{
		if ( m_REP == REPLACE )
		{
			if ( m_DISP == SHARE )
			{
				err.setRC( 8 ) ;
				return ;
			}
			else
			{
				if ( it->second->ownerTask != err.taskid )
				{
					err.setRC( 8 ) ;
					return ;
				}
				else if ( it->second->tab_WRITE != m_WRITE )
				{
					err.seterrid( "PSYE014R", tb_name ) ;
					return ;
				}
				else
				{
					tables.erase( tb_name ) ;
					err.setRC( 4 ) ;
				}
			}
		}
		else
		{
			err.setRC( 8 ) ;
			return ;
		}
	}
	Table * t = new Table ;

	if ( getpaths( m_path ) > 0 ) { t->tab_path = getpath( m_path, 1 ) ; }
	t->ownerTask = err.taskid ;
	t->tab_WRITE = m_WRITE ;
	t->tab_DISP  = m_DISP  ;
	t->changed   = true    ;
	t->tab_keys  = space( keys ) ;
	t->num_keys  = words( keys ) ;
	t->tab_flds  = space( flds ) ;
	t->num_flds  = words( flds ) ;
	t->tab_all   = t->tab_keys + " " + t->tab_flds ;
	t->num_all   = t->num_keys + t->num_flds      ;
	t->tab_cmds  = ( t->tab_all == "ZCTVERB ZCTTRUNC ZCTACT ZCTDESC" ) ;
	tables[ tb_name ] = t ;
}


void tableMGR::loadTable( errblock& err,
			  const string& tb_name,
			  tbWRITE m_WRITE,
			  const string& m_path,
			  tbDISP m_DISP )
{
	// RC = 0   Normal completion
	// RC = 8   Table does not exist in search path
	// RC = 12  Inconsistent WRITE options
	// RC = 20  Severe error

	// If table already loaded, EXCLUSIVE can be changed to SHARE by the same task.
	// Any other combination is not valid.

	// Routine to load V1 and V2 format tables

	// TODO: Clean up dynamic storage (rows) if table fails to fully load

	int  i        ;
	int  j        ;
	int  k        ;
	int  l        ;
	int  ver      ;
	int  num_rows ;
	int  num_keys ;
	int  num_flds ;
	int  all_flds ;
	int  n1, n2   ;

	char x           ;
	char buf1[ 256 ] ;
	char * buf2      ;
	char z[ 2 ]      ;

	string filename ;
	string path  ;
	string s     ;
	string hdr   ;
	string sir   ;
	string val   ;
	string keys  ;
	string flds  ;
	string err1  ;

	size_t buf2Size = 1024 ;

	vector<string> * row ;

	std::ifstream table ;

	map<string, Table*>::iterator it ;

	err.setRC( 0 ) ;

	it = tables.find( tb_name ) ;
	if ( it != tables.end() )
	{
		if ( it->second->tab_DISP == SHARE && m_DISP == EXCLUSIVE )
		{
			err.seterrid( "PSYE013Z", tb_name ) ;
			return ;
		}
		if ( it->second->tab_DISP == EXCLUSIVE && it->second->ownerTask != err.taskid )
		{
			err.seterrid( "PSYE014A", tb_name, d2ds( it->second->ownerTask ) ) ;
			return ;
		}
		if ( it->second->tab_WRITE != m_WRITE )
		{
			err.seterrid( "PSYE014R", tb_name, 12 ) ;
			return ;
		}
		it->second->incRefCount() ;
		it->second->tab_DISP = m_DISP ;
		return ;
	}

	for ( i = getpaths( m_path ), j = 1 ; j <= i ; j++ )
	{
		path     = getpath( m_path, j ) ;
		filename = path + tb_name       ;
		if ( exists( filename ) )
		{
			if ( !is_regular_file( filename ) )
			{
				err.seterrid( "PSYE014B", tb_name ) ;
				return ;
			}
			else
			{
				break ;
			}
		}
	}
	if ( j > i )
	{
		err.setRC( 8 ) ;
		return ;
	}

	table.open( filename.c_str() , ios::binary | ios::in ) ;
	if ( !table.is_open() )
	{
		err.seterrid( "PSYE014D", tb_name, filename ) ;
		return ;
	}

	table.read( buf1, 2);
	if ( memcmp( buf1, "\x00\x85", 2 ) )
	{
		err.seterrid( "PSYE014E", tb_name, filename ) ;
		table.close() ;
		return ;
	}

	table.get( x ) ;
	ver = static_cast< int >( x ) ;
	if ( ver > 2 )
	{
		err.seterrid( "PSYE014F", d2ds( ver ), filename ) ;
		table.close() ;
		return ;
	}

	table.get( x ) ;
	i = static_cast< int >( x ) ;
	if ( i < 0 ) { i = 256 + i ; }
	table.read( buf1, i ) ;
	hdr.assign( buf1, i ) ;
	table.get( x ) ;
	i = static_cast< int >( x ) ;
	if ( i < 0 ) { i = 256 + i ; }
	for ( j = 0 ; j < i ; j++ )
	{
		table.get( x ) ;
		k = static_cast< int >( x ) ;
		if ( k < 0 ) { k = 256 + k ; }
		table.read( buf1, k ) ;
		switch ( j )
		{
		case 0: sir.assign( buf1, k ) ;
			break ;
		default:
			err.seterrid( "PSYE014G", d2ds( j+1 ) ) ;
			table.close() ;
			return ;
		}
	}
	table.read( z, 2 ) ;
	n1 = static_cast< int >( z[ 0 ] ) ;
	n2 = static_cast< int >( z[ 1 ] ) ;
	if ( n1 < 0 ) { n1 = 256 + n1 ; }
	if ( n2 < 0 ) { n2 = 256 + n2 ; }
	num_rows = n1 * 256 + n2 ;

	table.get( x ) ;
	n1 = static_cast< int >( x ) ;
	if ( n1 < 0 ) { n1 = 256 + n1 ; }
	num_keys = n1  ;
	table.get( x ) ;
	n1 = static_cast< int >( x ) ;
	if ( n1 < 0 ) { n1 = 256 + n1 ; }
	num_flds = n1 ;
	all_flds = num_keys + num_flds ;

	keys = "" ;
	flds = "" ;

	for ( j = 0 ; j < num_keys ; j++ )
	{
		table.get( x ) ;
		if ( table.fail() != 0 )
		{
			err.seterrid( "PSYE014H", tb_name, filename ) ;
			table.close() ;
			return ;
		}
		i = static_cast< int >( x ) ;
		if ( i < 0 ) { i = 256 + i ; }
		table.read( buf1, i ) ;
		keys = keys + s.assign( buf1, i ) + " " ;
	}
	for ( j = 0 ; j < num_flds ; j++ )
	{
		table.get( x ) ;
		if ( table.fail() != 0 )
		{
			err.seterrid( "PSYE014I", tb_name, filename ) ;
			table.close() ;
			return ;
		}
		i = static_cast< int >( x ) ;
		if ( i < 0 ) { i = 256 + i ; }
		table.read( buf1, i ) ;
		flds = flds + s.assign( buf1, i ) + " " ;
	}

	createTable( err, tb_name, keys, flds, NOREPLACE, m_WRITE, path, m_DISP ) ;
	if ( err.getRC() > 0 )
	{
		table.close() ;
		return ;
	}

	it = tables.find( tb_name ) ;
	it->second->reserveSpace( num_rows ) ;
	if ( sir != "" )
	{
		it->second->tbsort( err, sir ) ;
		if ( err.getRC() > 0 )
		{
			err.seterrid( "PSYE013N", sir ) ;
			return  ;
		}
	}
	buf2 = new char[ buf2Size ] ;
	for ( l = 0 ; l < num_rows ; l++ )
	{
		row = new vector<string> ;
		for ( j = 0 ; j < all_flds ; j++ )
		{
			table.read( z, 2 ) ;
			if ( table.fail() != 0 )
			{
				err.seterrid( "PSYE014M", tb_name, filename ) ;
				table.close() ;
				delete[] buf2 ;
				delete row    ;
				return  ;
			}
			n1 = static_cast< int >( z[ 0 ] ) ;
			n2 = static_cast< int >( z[ 1 ] ) ;
			if ( n1 < 0 ) { n1 = 256 + n1 ; }
			if ( n2 < 0 ) { n2 = 256 + n2 ; }
			i = n1 * 256 + n2 ;
			if ( i > buf2Size )
			{
				delete[] buf2 ;
				buf2Size = i  ;
				buf2     = new char[ buf2Size ] ;
			}
			table.read( buf2, i ) ;
			if ( table.fail() != 0 )
			{
				err.seterrid( "PSYE014M", tb_name, filename ) ;
				table.close() ;
				delete[] buf2 ;
				delete row    ;
				return ;
			}
			val.assign( buf2, i ) ;
			debug2( "Value read for row "<< l <<" position "<< j <<" '"+ val +"'" <<endl ) ;
			row->push_back( val ) ;
		}
		if ( ver > 1 )
		{
			table.read( z, 2 ) ;
			if ( table.fail() != 0 )
			{
				err.seterrid( "PSYE014M", tb_name, filename ) ;
				table.close() ;
				delete[] buf2 ;
				delete row    ;
				return  ;
			}
			n1 = static_cast< int >( z[ 0 ] ) ;
			n2 = static_cast< int >( z[ 1 ] ) ;
			if ( n1 < 0 ) { n1 = 256 + n1 ; }
			if ( n2 < 0 ) { n2 = 256 + n2 ; }
			i = n1 * 256 + n2 ;
			for ( j = 0 ; j < i ; j++ )
			{
				table.read( z, 2 ) ;
				if ( table.fail() != 0 )
				{
					err.seterrid( "PSYE014M", tb_name, filename ) ;
					table.close() ;
					delete[] buf2 ;
					delete row    ;
					return  ;
				}
				n1 = static_cast< int >( z[ 0 ] ) ;
				n2 = static_cast< int >( z[ 1 ] ) ;
				if ( n1 < 0 ) { n1 = 256 + n1 ; }
				if ( n2 < 0 ) { n2 = 256 + n2 ; }
				k = n1 * 256 + n2 ;
				if ( k > buf2Size )
				{
					delete[] buf2 ;
					buf2Size = i  ;
					buf2     = new char[ buf2Size ] ;
				}
				table.read( buf2, k ) ;
				if ( table.fail() != 0 )
				{
					err.seterrid( "PSYE014M", tb_name, filename ) ;
					table.close() ;
					delete[] buf2 ;
					delete row    ;
					return ;
				}
				val.assign( buf2, k ) ;
				row->push_back( val ) ;
			}
			table.read( z, 1 ) ;
			if ( table.fail() != 0 )
			{
				err.seterrid( "PSYE014M", tb_name, filename ) ;
				table.close() ;
				delete[] buf2 ;
				delete row    ;
				return ;
			}
			if ( z[ 0 ] != char(0xFF) )
			{
				err.seterrid( "PSYE014J", filename ) ;
				table.close() ;
				delete[] buf2 ;
				delete row    ;
				return ;
			}
		}
		it->second->loadRow( err, row ) ;
		if ( err.error() )
		{
			table.close() ;
			delete[] buf2 ;
			return ;
		}
	}
	if ( ver > 1 )
	{
		table.read( z, 1 ) ;
		if ( table.fail() != 0 )
		{
			err.seterrid( "PSYE014M", tb_name, filename ) ;
			table.close() ;
			delete[] buf2 ;
			return ;
		}
		if ( z[ 0 ] != char(0xFF) )
		{
			err.seterrid( "PSYE014K", filename ) ;
			table.close() ;
			delete[] buf2 ;
			return ;
		}
	}
	it->second->resetChanged() ;
	debug2( "Number of rows loaded from table " << l <<endl ) ;
	if ( l != num_rows )
	{
		err.seterrid( "PSYE014L", d2ds( l ), d2ds( num_rows ) ) ;
	}
	table.close() ;
	delete[] buf2 ;
}


void tableMGR::saveTable( errblock& err,
			  const string& tb_name,
			  const string& m_newname,
			  const string& m_path )
{
	// This can be called by tbclose() or tbsave().

	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "SAVETABLE", tb_name, 12 ) ;
		return ;
	}
	it->second->saveTable( err, ( m_newname == "" ? tb_name : m_newname ), m_path ) ;
}


void tableMGR::destroyTable( errblock& err,
			     const string& tb_name )
{
	// RC =  0 Normal completion
	// RC = 12 Table not open

	map<string, Table*>::iterator it ;

	err.setRC( 0 ) ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "DESTROYTABLE", tb_name, 12 ) ;
		return ;
	}

	it->second->decRefCount() ;
	if ( it->second->notInUse() )
	{
		delete it->second  ;
		tables.erase( it ) ;
	}
}


void tableMGR::statistics()
{
	map<string, Table*>::iterator it    ;
	map<string, tbsearch>::iterator its ;

	llog( "-", "TABLE STATISTICS:" <<endl ) ;
	llog( "-", "         Number of tables loaded . . . " << tables.size() <<endl ) ;
	llog( "-", "          Table details:" <<endl ) ;
	for ( it = tables.begin() ; it != tables.end() ; it++ )
	{
		llog( "-", "" <<endl ) ;
		llog( "-", "                  Table: "+ it->first <<endl ) ;
		if ( it->second->tab_WRITE == WRITE )
		{
			llog( "-", "                 Status: Open in WRITE mode" <<endl ) ;
		}
		else
		{
			llog( "-", "                 Status: Open in NOWRITE mode" <<endl ) ;
		}
		if ( it->second->changed )
		{
			llog( "-", "                       : Modified since load or last save" <<endl ) ;
		}
		llog( "-", "            Owning Task: " << it->second->ownerTask <<endl ) ;
		if ( it->second->num_keys > 0 )
		{
			llog( "-", "                   Keys: " << setw(3) << it->second->tab_keys <<endl ) ;
		}
		llog( "-", "                 Fields: " << it->second->tab_flds <<endl ) ;
		llog( "-", "         Number of rows: " << it->second->table.size() <<endl ) ;
		llog( "-", "                   Path: " << it->second->tab_path <<endl ) ;
		llog( "-", "   Current Row Position: " << it->second->CRP <<endl ) ;
		if ( it->second->sarg.size() > 0 )
		{
			llog( "-", "Current Search Argument: " <<endl ) ;
			llog( "-", "        Condition Pairs: " << it->second->sa_cond_pairs <<endl ) ;
			llog( "-", "       Search Direction: " << it->second->sa_dir <<endl ) ;
			llog( "-", "    Extension Variables: " << it->second->sa_namelst <<endl ) ;
			for ( its = it->second->sarg.begin() ; its != it->second->sarg.end() ; its++ )
			{
				llog( "-", "             Field Name: "+ its->first <<endl ) ;
				if ( its->second.tbs_gen )
					llog( "-", "            Field Value: "+ its->second.tbs_val +" (generic search)" <<endl ) ;
				else
					llog( "-", "            Field Value: "+ its->second.tbs_val <<endl ) ;
				llog( "-", "        Field Condition: "+ its->second.tbs_scond <<endl ) ;
			}
		}
		if ( it->second->sort_ir != "" )
		{
			llog( "-", "Sort Information Record: "+ it->second->sort_ir <<endl ) ;
		}
	}
	llog( "-", "*************************************************************************************************************" <<endl ) ;
}


void tableMGR::fillfVARs( errblock& err,
			  fPOOL& funcPOOL,
			  const string& tb_name,
			  const string& clear_flds,
			  bool scan,
			  int  depth,
			  int  posn )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "FILLVARS", tb_name, 12 ) ;
		return ;
	}
	it->second->fillfVARs( err, funcPOOL, clear_flds, scan, depth, posn ) ;
}


void tableMGR::tbget( errblock& err,
		      fPOOL& funcPOOL,
		      const string& tb_name,
		      const string& tb_savenm,
		      const string& tb_rowid_vn,
		      const string& tb_noread,
		      const string& tb_crp_name  )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBGET", tb_name, 12 ) ;
		return ;
	}
	it->second->tbget( err, funcPOOL, tb_savenm, tb_rowid_vn, tb_noread, tb_crp_name ) ;
}


void tableMGR::tbmod( errblock& err,
		      fPOOL& funcPOOL,
		      const string& tb_name,
		      const string& tb_namelst,
		      const string& tb_order )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBMOD", tb_name, 12 ) ;
		return ;
	}
	it->second->tbmod( err, funcPOOL, tb_namelst, tb_order ) ;
}


void tableMGR::tbput( errblock& err,
		      fPOOL& funcPOOL,
		      const string& tb_name,
		      const string& tb_namelst,
		      const string& tb_order )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBPUT", tb_name, 12 ) ;
		return ;
	}
	it->second->tbput( err, funcPOOL, tb_namelst, tb_order ) ;
}


void tableMGR::tbadd( errblock& err,
		      fPOOL& funcPOOL,
		      const string& tb_name,
		      const string& tb_namelst,
		      const string& tb_order,
		      int tb_num_of_rows )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBADD", tb_name, 12 ) ;
		return ;
	}
	it->second->tbadd( err, funcPOOL, tb_namelst, tb_order, tb_num_of_rows ) ;
}


void tableMGR::tbbottom( errblock& err,
			 fPOOL& funcPOOL,
			 const string& tb_name,
			 const string& tb_savenm,
			 const string& tb_rowid_vn,
			 const string& tb_noread,
			 const string& tb_crp_name  )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBBOTTOM", tb_name, 12 ) ;
		return ;
	}
	it->second->tbbottom( err, funcPOOL, tb_savenm, tb_rowid_vn, tb_noread, tb_crp_name ) ;
}


void tableMGR::tbdelete( errblock& err,
			 fPOOL& funcPOOL,
			 const string& tb_name )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBDELETE", tb_name, 12 ) ;
		return ;
	}
	it->second->tbdelete( err, funcPOOL ) ;
}


void tableMGR::tberase( errblock& err,
			const string& tb_name,
			const string& tb_path )
{
	int i ;
	int j ;

	string filename ;
	map<string, Table*>::iterator it ;

	err.setRC( 0 ) ;

	it = tables.find( tb_name ) ;
	if ( it != tables.end() )
	{
		err.seterrid( "PSYE014N", tb_name, 12 ) ;
		return ;
	}

	for ( i = getpaths( tb_path ), j = 1 ; j <= i ; j++ )
	{
		filename = getpath( tb_path, j ) + tb_name ;
		if ( exists( filename ) )
		{
			if ( !is_regular_file( filename ) )
			{
				err.seterrid( "PSYE014B", filename, 16 ) ;
				return ;
			}
			else
			{
				break ;
			}
		}
	}
	if ( j > i )
	{
		err.setRC( 8 ) ;
		return ;
	}
	remove( filename ) ;
}


void tableMGR::tbexist( errblock& err,
			fPOOL& funcPOOL,
			const string& tb_name )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBEXIST", tb_name, 12 ) ;
		return ;
	}
	it->second->tbexist( err, funcPOOL ) ;
}


void tableMGR::tbquery( errblock& err,
			fPOOL& funcPOOL,
			const string& tb_name,
			const string& tb_keyn,
			const string& tb_varn,
			const string& tb_rownn,
			const string& tb_keynn,
			const string& tb_namenn,
			const string& tb_crpn,
			const string& tb_sirn,
			const string& tb_lstn,
			const string& tb_condn,
			const string& tb_dirn )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBQUERY", tb_name, 12 ) ;
		return ;
	}
	it->second->tbquery( err, funcPOOL, tb_keyn, tb_varn, tb_rownn, tb_keynn, tb_namenn, tb_crpn, tb_sirn, tb_lstn, tb_condn, tb_dirn ) ;
}


void tableMGR::tbsarg( errblock& err,
		       fPOOL& funcPOOL,
		       const string& tb_name,
		       const string& tb_namelst,
		       const string& tb_dir,
		       const string& tb_cond_pairs )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBSARG", tb_name, 12 ) ;
		return ;
	}
	it->second->tbsarg( err, funcPOOL, tb_namelst, tb_dir, tb_cond_pairs ) ;
}


void tableMGR::tbscan( errblock& err,
		       fPOOL& funcPOOL,
		       const string& tb_name,
		       const string& tb_namelst,
		       const string& tb_savenm,
		       const string& tb_rowid_vn,
		       const string& tb_dir,
		       const string& tb_noread,
		       const string& tb_crp_name,
		       const string& tb_condlst )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBSCAN", tb_name, 12 ) ;
		return ;
	}
	it->second->tbscan( err, funcPOOL, tb_namelst, tb_savenm, tb_rowid_vn, tb_dir, tb_noread, tb_crp_name, tb_condlst ) ;
}


void tableMGR::cmdsearch( errblock& err,
			  fPOOL& funcPOOL,
			  string tb_name,
			  const string& cmd,
			  const string& paths )
{
	// Search table for command 'cmd'.  Load table if not already in storage.

	// RC = 0  Okay
	// RC = 8  Command not found
	// RC = 12 Table not found (RC=8 from loadTable)
	// RC = 20 Severe error

	map<string, Table*>::iterator it ;

	tb_name += "CMDS" ;
	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		loadTable( err, tb_name, NOWRITE, paths, SHARE ) ;
		if ( err.error() )
		{
			llog( "E", "Command table "+ tb_name +" failed to load" <<endl ) ;
			err.setRC( 20 ) ;
			return ;
		}
		else if ( !err.RC0() )
		{
			err.setRC( 12 ) ;
			return ;
		}
		it = tables.find( tb_name ) ;
	}
	it->second->cmdsearch( err, funcPOOL, cmd ) ;
}


void tableMGR::tbskip( errblock& err,
		       fPOOL& funcPOOL,
		       const string& tb_name,
		       int num,
		       const string& tb_savenm,
		       const string& tb_rowid_vn,
		       const string& tb_rowid,
		       const string& tb_noread,
		       const string& tb_crp_name )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBSKIP", tb_name, 12 ) ;
		return ;
	}
	it->second->tbskip( err, funcPOOL, num, tb_savenm, tb_rowid_vn, tb_rowid, tb_noread, tb_crp_name ) ;
}


void tableMGR::tbsort( errblock& err,
		       const string& tb_name,
		       const string& tb_fields )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBSORT", tb_name, 12 ) ;
		return ;
	}
	it->second->tbsort( err, tb_fields ) ;
}


void tableMGR::tbtop( errblock& err,
		      const string& tb_name )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBTOP", tb_name, 12 ) ;
		return ;
	}
	it->second->tbtop( err ) ;
}


void tableMGR::tbvclear( errblock& err,
			 fPOOL& funcPOOL,
			 const string& tb_name )
{
	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "TBVCLEAR", tb_name, 12 ) ;
		return ;
	}
	it->second->tbvclear( err, funcPOOL ) ;
}


bool tableMGR::writeableTable( errblock& err,
			       const string& tb_name )
{
	err.setRC( 0 ) ;

	map<string, Table*>::iterator it ;

	it = tables.find( tb_name ) ;
	if ( it == tables.end() )
	{
		err.seterrid( "PSYE013G", "WRITEABLETABLE", tb_name, 12 ) ;
		return false ;
	}
	return ( it->second->tab_WRITE == WRITE ) ;
}
