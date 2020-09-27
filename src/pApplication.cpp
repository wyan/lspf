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

boost::mutex pApplication::mtx ;

void ispexeci( pApplication*, const string&, errblock& ) ;

map<int, void*>pApplication::ApplUserData ;

vector<enqueue>pApplication::g_enqueues ;

vector<string>pApplication::notifies ;


pApplication::pApplication()
{
	uAppl               = NULL   ;
	testMode            = false  ;
	propagateEnd        = false  ;
	jumpEntered         = false  ;
	ControlErrorsReturn = false  ;
	ControlPassLRScroll = false  ;
	ControlNonDisplEnd  = false  ;
	lineOutPending      = false  ;
	lineInPending       = false  ;
	cmdTableLoaded      = false  ;
	nested              = false  ;
	errPanelissued      = false  ;
	abending            = false  ;
	abended             = false  ;
	addpop_active       = false  ;
	addpop_row          = 0      ;
	addpop_col          = 0      ;
	taskId              = 0      ;
	backgrd             = false  ;
	notifyEnded         = false  ;
	busyAppl            = true   ;
	terminateAppl       = false  ;
	applicationEnded    = false  ;
	abnormalEnd         = false  ;
	abnormalEndForced   = false  ;
	abnormalTimeout     = false  ;
	abnormalNoMsg       = false  ;
	reloadCUATables     = false  ;
	refreshlScreen      = false  ;
	zjobkey             = ""     ;
	rexxName            = ""     ;
	newpool             = false  ;
	newappl             = ""     ;
	passlib             = false  ;
	suspend             = true   ;
	SEL                 = false  ;
	selPanel            = false  ;
	setMessage          = false  ;
	inBuffer            = ""     ;
	outBuffer           = ""     ;
	reffield            = ""     ;
	PARM                = ""     ;
	currPanel           = NULL   ;
	prevPanel           = NULL   ;
	currtbPanel         = NULL   ;
	zappver             = ""     ;
	zapphelp            = ""     ;
	RC                  = 0      ;
	ZRC                 = 0      ;
	ZRSN                = 0      ;
	ZRESULT             = ""     ;
	waiting_on          = WAIT_NONE ;
	zerr1               = ""     ;
	zerr2               = ""     ;
	zerr3               = ""     ;
	zerr4               = ""     ;
	zerr5               = ""     ;
	zerr6               = ""     ;
	zerr7               = ""     ;
	zerr8               = ""     ;
	funcPOOL.define( errBlock, "ZRC", &ZRC, NOCHECK ) ;
	funcPOOL.define( errBlock, "ZRSN", &ZRSN, NOCHECK ) ;
	funcPOOL.define( errBlock, "ZRESULT", &ZRESULT, NOCHECK ) ;
}


pApplication::~pApplication()
{
	map<string, pPanel*>::iterator it ;

	while ( !popups.empty() )
	{
		pPanel* panl = static_cast<pPanel*>( popups.top().panl ) ;
		if ( panl )
		{
			panl->delete_panels( popups.top() ) ;
		}
		popups.pop() ;
	}

	for ( it = panelList.begin() ; it != panelList.end() ; ++it )
	{
		delete it->second ;
	}
}


void pApplication::init_phase1( selobj& sel, int taskid, void (* Callback)( lspfCommand& ) )
{
	// Setup various program parameters.
	// Variable services are not available at this time.

	zappname = sel.pgm  ;
	zparm    = sel.parm ;
	PARM     = sel.parm ;
	passlib  = sel.passlib ;
	newappl  = sel.newappl ;
	newpool  = sel.newpool ;
	suspend  = sel.suspend ;
	selPanel = sel.selPanel() ;
	nested   = sel.nested  ;
	options  = sel.options ;
	backgrd  = sel.backgrd ;
	taskId   = taskid      ;
	ptid     = ( sel.ptid == 0 ) ? taskId : sel.ptid ;
	errBlock.taskid = taskid ;
	errBlock.ptid   = ptid   ;
	lspfCallback    = Callback ;
}


void pApplication::init_phase2()
{
	// Before being dispatched in its own thread, set the search paths and
	// create implicit function pool variables defined as INTEGER (kept across a vreset).

	// ZZ variables are for internal use only.  Don't use in applications.

	zzplib = p_poolMGR->get( errBlock, "ZPLIB", PROFILE ) ;
	zzmlib = p_poolMGR->get( errBlock, "ZMLIB", PROFILE ) ;
	zztlib = p_poolMGR->get( errBlock, "ZTLIB", PROFILE ) ;
	zztabl = p_poolMGR->get( errBlock, "ZTABL", PROFILE ) ;

	zzpusr = p_poolMGR->get( errBlock, "ZPUSR", PROFILE ) ;
	zzmusr = p_poolMGR->get( errBlock, "ZMUSR", PROFILE ) ;
	zztusr = p_poolMGR->get( errBlock, "ZTUSR", PROFILE ) ;
	zztabu = p_poolMGR->get( errBlock, "ZTABU", PROFILE ) ;

	funcPOOL.put2( errBlock, "ZTDTOP",   0 ) ;
	funcPOOL.put2( errBlock, "ZTDSELS",  0 ) ;
	funcPOOL.put2( errBlock, "ZTDDEPTH", 0 ) ;
	funcPOOL.put2( errBlock, "ZTDROWS",  0 ) ;
	funcPOOL.put2( errBlock, "ZTDVROWS", 0 ) ;
	funcPOOL.put2( errBlock, "ZCURPOS",  1 ) ;
	funcPOOL.put2( errBlock, "ZCURINX",  0 ) ;
	funcPOOL.put2( errBlock, "ZZCRP",    0 ) ;
	funcPOOL.put2( errBlock, "ZSBTASK",  0 ) ;

	lscreen     = ds2d( p_poolMGR->get( errBlock, "ZSCREEN", SHARED ) ) ;
	lscreen_num = ds2d( p_poolMGR->get( errBlock, "ZSCRNUM", SHARED ) ) ;

	startDate = p_poolMGR->get( errBlock, "ZJ4DATE", SHARED ) ;
	startTime = p_poolMGR->get( errBlock, "ZTIMEL", SHARED ) ;

	cond_mstr = backgrd ? &cond_batch : &cond_lspf ;

	llog( "I", "Phase 2 initialisation complete" << endl ; )
}


void pApplication::run()
{
	// Start running the user application and catch any exceptions.
	// Not all exceptions are caught - eg. segmentation faults

	// Cleanup during termination:
	// 1) Close any tables opened by this application if a ptid.
	// 2) Unload the application command table if loaded by this application.
	// 3) Release any global enqueues held by this task.
	// 4) Send a notify if the batch job has ended/abended, if requested.

	string t ;
	string ztime ;

	try
	{
		application() ;
	}
	catch ( pApplication::xTerminate )
	{
		llog( "E", "Application "+ zappname +" aborting..." << endl ) ;
	}
	catch (...)
	{
		try
		{
			abendexc() ;
		}
		catch (...)
		{
			llog( "E", "An abend has occured during abend processing" << endl ) ;
			llog( "E", "Calling abend() only to terminate application" << endl ) ;
			abend_nothrow() ;
		}
	}

	if ( ptid == taskId )
	{
		p_tableMGR->closeTablesforTask( errBlock ) ;
	}

	if ( cmdTableLoaded )
	{
		p_tableMGR->destroyTable( errBlock, get_applid() + "CMDS" ) ;
	}

	if ( notifyEnded )
	{
		vget( "ZTIMEL", SHARED ) ;
		vcopy( "ZTIMEL", ztime, MOVE ) ;
		t = abnormalEnd ? " HAS ABENDED" : " HAS ENDED" ;
		notify( ztime.substr( 0, 8 ) + " JOB " + d2ds( taskid(), 5 ) + t ) ;
	}

	boost::lock_guard<boost::mutex> lock( mtx ) ;

	for ( auto it = g_enqueues.begin() ; it != g_enqueues.end() ; )
	{
		if ( it->tasks.count( taskId ) > 0 )
		{
			it->tasks.erase( taskId ) ;
			if ( it->tasks.size() == 0 )
			{
				it = g_enqueues.erase( it ) ;
				continue ;
			}
		}
		++it ;
	}

	t = backgrd ? " background " : " " ;
	llog( "I", "Shutting down"+ t +"application: " + zappname +" Taskid: " << taskId << endl ) ;

	terminateAppl    = true  ;
	applicationEnded = true  ;
	busyAppl         = false ;

	cond_mstr->notify_all() ;
}


void pApplication::wait_event( WAIT_REASON w )
{
	waiting_on = w ;
	busyAppl   = false ;

	cond_mstr->notify_all() ;

	while ( !busyAppl )
	{
		boost::mutex::scoped_lock lk( mutex ) ;
		cond_appl.wait( lk ) ;
		lk.unlock() ;
		if ( terminateAppl )
		{
			RC = 20 ;
			llog( "E", "Application terminating.  Cancelling wait_event" << endl ) ;
			abend() ;
		}
	}
	waiting_on = WAIT_NONE ;
}


bool pApplication::isprimMenu()
{
	if ( currPanel ) { return currPanel->primaryMenu ; }
	return false ;
}


void pApplication::get_home( uint& row, uint& col )
{
	if ( currPanel ) { currPanel->get_home( row, col ) ; }
	else             { row = 0 ; col = 0               ; }
}


string pApplication::get_applid()
{
	return p_poolMGR->get( errBlock, "ZAPPLID", SHARED ) ;
}


void pApplication::get_cursor( uint& row, uint& col )
{
	if ( currPanel ) { currPanel->get_cursor( row, col ) ; }
	else             { row = 0 ; col = 0                 ; }
}


string pApplication::get_current_panelDesc()
{
	if ( currPanel ) { return currPanel->get_panelDesc() ; }
	return "" ;
}


string pApplication::get_current_screenName()
{
	return p_poolMGR->get( errBlock, "ZSCRNAME", SHARED ) ;
}


string pApplication::get_panelid()
{
	if ( currPanel ) { return currPanel->panelid ; }
	return "" ;
}


bool pApplication::inputInhibited()
{
	if ( currPanel ) { return currPanel->inputInhibited() ; }
	return false ;
}


bool pApplication::msgInhibited()
{
	if ( currPanel ) { return currPanel->msgInhibited() ; }
	return false ;
}


void pApplication::display_pd( errblock& err )
{
	if ( currPanel ) { currPanel->display_pd( err ) ; }
}


void pApplication::msgResponseOK()
{
	if ( currPanel ) { currPanel->msgResponseOK() ; }
}


bool pApplication::error_msg_issued()
{
	if ( currPanel ) { return currPanel->error_msg_issued() ; }
	return false ;
}


void pApplication::set_userAction( USR_ACTIONS act )
{
	if ( currPanel ) { currPanel->set_userAction( act) ; }
	usr_action = act ;
}


bool pApplication::end_pressed()
{
	return ( usr_action == USR_CANCEL ||
		 usr_action == USR_END    ||
		 usr_action == USR_EXIT   ||
		 usr_action == USR_RETURN ) ;
}


void pApplication::store_scrname()
{
	zscrname = p_poolMGR->get( errBlock, "ZSCRNAME", SHARED ) ;
}


bool pApplication::errorsReturn()
{
	return ControlErrorsReturn ;
}


void pApplication::setTestMode()
{
	testMode = true ;
	errBlock.setDebugMode() ;
}


void pApplication::redraw_panel( errblock& err )
{
	if ( currPanel ) { currPanel->redraw_panel( err ) ; }
}


void pApplication::restore_Zvars( int screenid )
{
	// Restore various variables after stacked application has terminated

	if ( currPanel )
	{
		currPanel->update_keylist_vars( errBlock ) ;
		p_poolMGR->put( errBlock, "ZPRIM", currPanel->get_zprim(), SHARED ) ;
	}

	if ( p_poolMGR->get( errBlock, screenid, "ZSCRNAM2" ) == "PERM" )
	{
		zscrname = p_poolMGR->get( errBlock, screenid, "ZSCRNAME" ) ;
	}

	p_poolMGR->put( errBlock, "ZSCRNAME", zscrname, SHARED ) ;
}


void pApplication::display_id()
{
	if ( currPanel ) { currPanel->display_id( errBlock ) ; }
}


void pApplication::set_nondispl_enter()
{
	ControlNonDispl = true ;
	ControlNonDisplEnd = false ;
}


void pApplication::set_nondispl_end()
{
	ControlNonDispl = true ;
	ControlNonDisplEnd = true ;
}


void pApplication::clr_nondispl()
{
	ControlNonDispl = false ;
	ControlNonDisplEnd = false ;
}


void pApplication::set_msg( const string& msg_id )
{
	// Display a message on current panel using msg_id

	if ( !currPanel ) { return ; }

	get_message( msg_id ) ;
	if ( RC == 0 )
	{
		currPanel->clear_msg_loc() ;
		currPanel->set_panel_msg( zmsg, zmsgid ) ;
		currPanel->display_msg( errBlock )       ;
	}
}


void pApplication::set_msg1( const slmsg& t, const string& msgid )
{
	// Propagate setmsg() to the current application using the short-long-message object.
	// zmsg1 and zmsgid1 are used to store messages issued by the setmsg() service after variable substitution.

	zmsg1      = t     ;
	zmsgid1    = msgid ;
	setMessage = true  ;
}


void pApplication::display_setmsg()
{
	// Display the message stored by the setmsg() service.

	if ( setMessage && currPanel )
	{
		currPanel->set_panel_msg( zmsg1, zmsgid1 ) ;
		currPanel->display_msg( errBlock ) ;
		setMessage = false ;
	}
}


void pApplication::clear_msg()
{
	if ( currPanel ) { currPanel->clear_msg() ; }
}


bool pApplication::show_help_member()
{
	return ( currPanel->get_msg() == "" || currPanel->showLMSG || currPanel->MSG.lmsg == "" ) ;
}


void pApplication::save_errblock()
{
	serBlock = errBlock ;
	errBlock.clear()    ;
	errBlock.setServiceCall() ;
}


void pApplication::restore_errblock()
{
	errBlock = serBlock ;
}


bool pApplication::nretriev_on()
{
	if ( currPanel ) { return currPanel->get_nretriev() ; }
	return false ;
}


string pApplication::get_nretfield()
{
	if ( currPanel ) { return currPanel->get_nretfield() ; }
	return "" ;
}


void pApplication::toggle_fscreen()
{
	currPanel->toggle_fscreen( addpop_active, addpop_row, addpop_col ) ;
	currPanel->display_panel( errBlock ) ;
}


void pApplication::set_addpop( pApplication* p )
{
	addpop_row    = p->addpop_row ;
	addpop_col    = p->addpop_col ;
	addpop_active = p->addpop_active ;
}


void pApplication::set_zlibd( bool passlib, pApplication* p )
{
	if ( not passlib )
	{
		zlibd = p->zlibd ;
	}
	else
	{
		for ( auto it = p->zlibd.begin() ; it != p->zlibd.end() ; ++it )
		{
			if ( not it->second.empty() )
			{
				zlibd[ it->first ].push( it->second.top() ) ;
			}
		}
	}
}


map<string, pPanel*>::iterator pApplication::createPanel( const string& p_name )
{
	map<string, pPanel*>::iterator it ;
	pair<map<string, pPanel*>::iterator, bool> result ;

	const string e1 = "Error creating panel " + p_name ;

	errBlock.setRC( 0 ) ;

	it = panelList.find( p_name ) ;
	if ( it != panelList.end() ) { return it ; }

	if ( !isvalidName( p_name ) )
	{
		errBlock.setcall( e1, "PSYE021A", p_name ) ;
		checkRCode( errBlock ) ;
		return it ;
	}

	pPanel* p_panel     = new pPanel ;
	p_panel->p_funcPOOL = &funcPOOL  ;
	p_panel->lrScroll   = ControlPassLRScroll ;
	p_panel->Rexx       = ( rexxName != "" )  ;
	p_panel->selPanel( selPanel ) ;
	p_panel->init( errBlock ) ;
	if ( errBlock.error() )
	{
		delete p_panel ;
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return it ;
	}

	p_panel->loadPanel( errBlock, p_name, get_search_path( s_ZPLIB ) ) ;

	if ( errBlock.RC0() )
	{
		result = panelList.insert( pair<string, pPanel*>( p_name, p_panel ) ) ;
		it = result.first ;
		load_keylist( p_panel ) ;
	}
	else
	{
		delete p_panel ;
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
	}
	errBlock.clearsrc() ;

	return it ;
}


void pApplication::display( string p_name,
			    const string& p_msg,
			    const string& p_cursor,
			    int p_curpos,
			    const string& p_buffer,
			    const string& p_retbuf )
{
	map<string,pPanel*>::iterator it ;

	const string e1 = "Error during DISPLAY of panel " + p_name ;
	const string e2 = "Error processing )INIT section of panel "   ;
	const string e3 = "Error processing )REINIT section of panel " ;
	const string e4 = "Error processing )PROC section of panel "   ;
	const string e5 = "Error during update of panel " ;
	const string e6 = "Error updating field values of panel " ;
	const string e7 = "Error processing )ATTR section of panel " ;
	const string e8 = "Background job attempted to display panel " ;

	bool doReinit = false ;

	RC = 0 ;

	if ( not busyAppl )
	{
		llog( "E", "Invalid method state" <<endl ) ;
		llog( "E", "Application is in a wait state.  Method cannot invoke display services." <<endl ) ;
		RC = 20 ;
		return ;
	}

	if ( backgrd )
	{
		llog( "B", e8 + p_name <<endl ) ;
		RC = 8 ;
		return ;
	}

	if ( currPanel )
	{
		currPanel->hide_popup() ;
		currPanel->clear_msg() ;
	}

	if ( p_name == "" )
	{
		if ( !currPanel )
		{
			errBlock.setcall( e1, "PSYE021C" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		p_name   = currPanel->panelid ;
		doReinit = true ;
	}

	if ( p_cursor != "" && !isvalidName( p_cursor ) )
	{
		errBlock.setcall( e1, "PSYE023I", p_cursor ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	cbuffer = "" ;
	if ( p_buffer != "" )
	{
		cbuffer = funcPOOL.get( errBlock, 8, p_buffer ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		if ( strip( cbuffer ) != "" )
		{
			set_nondispl_enter() ;
		}
	}

	if ( p_retbuf != "" && !isvalidName( p_retbuf ) )
	{
		errBlock.setcall( e1, "PSYE031E", "return buffer", p_retbuf ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	it = createPanel( p_name ) ;
	if ( errBlock.error() ) { return ; }

	prevPanel = currPanel ;
	currPanel = it->second ;

	if ( propagateEnd )
	{
		if ( prevPanel && prevPanel->panelid == p_name )
		{
			propagateEnd = false ;
		}
		else
		{
			set_nondispl_enter() ;
		}
	}

	currPanel->init_control_variables() ;

	currPanel->set_msg( p_msg ) ;
	currPanel->set_cursor( p_cursor, p_curpos ) ;
	currPanel->set_msgloc( "" ) ;

	currPanel->set_popup( addpop_active, addpop_row, addpop_col ) ;

	p_poolMGR->put( errBlock, "ZPANELID", p_name, SHARED, SYSTEM ) ;

	p_poolMGR->put( errBlock, "ZVERB", propagateEnd ? "RETURN" : "", SHARED ) ;
	usr_action = USR_ENTER ;

	if ( doReinit )
	{
		currPanel->display_panel_reinit( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e3 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		set_ZVERB_panel_resp_re_init() ;
		set_panel_zvars() ;
	}
	else
	{
		currPanel->display_panel_init( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e2 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		currPanel->display_panel_attrs( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e7 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		set_ZVERB_panel_resp_re_init() ;
		set_panel_zvars() ;
		currPanel->update_field_values( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e6 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		RC = errBlock.getRC() ;
	}

	if ( currPanel->get_msg() != "" )
	{
		get_message( currPanel->get_msg() ) ;
		if ( RC > 0 ) { return ; }
		currPanel->set_panel_msg( zmsg, zmsgid ) ;
	}
	else
	{
		currPanel->clear_msg() ;
	}

	set_screenName() ;

	while ( true )
	{
		currPanel->cursor_placement( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		currPanel->display_panel( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		if ( lineInOutDone && not ControlNonDispl )
		{
			refreshlScreen = true ;
			wait_event( WAIT_USER ) ;
			lineInOutDone  = false ;
			refreshlScreen = false ;
		}
		wait_event( WAIT_USER ) ;
		ControlDisplayLock = false ;
		refreshlScreen     = false ;
		reloadCUATables    = false ;
		currPanel->display_panel_update( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e5 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		if ( currPanel->get_msg() != "" )
		{
			get_message( currPanel->get_msg() ) ;
			if ( RC > 0 ) { return ; }
			currPanel->set_panel_msg( zmsg, zmsgid ) ;
			if ( not propagateEnd && not end_pressed() )
			{
				continue ;
			}
		}
		if ( currPanel->do_redisplay() ) { continue ; }

		currPanel->display_panel_proc( errBlock, 0 ) ;
		clr_nondispl() ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e4 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		set_ZVERB_panel_resp() ;
		set_panel_zvars() ;
		if ( propagateEnd || end_pressed() )
		{
			if ( usr_action == USR_RETURN ) { propagateEnd = true ; }
			RC = 8 ;
			return ;
		}

		if ( currPanel->get_msg() == "" ) { break ; }

		get_message( currPanel->get_msg() ) ;
		if ( RC > 0 ) { return ; }
		currPanel->set_panel_msg( zmsg, zmsgid ) ;
		currPanel->display_panel_reinit( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e3 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		RC = errBlock.getRC() ;
		set_ZVERB_panel_resp_re_init() ;
		set_panel_zvars() ;
	}
}


void pApplication::set_ZVERB_panel_resp_re_init()
{

	string dotResp = currPanel->get_resp() ;

	if ( currPanel->get_msg() == "" )
	{
		if ( dotResp == "ENTER" )
		{
			p_poolMGR->put( errBlock, "ZVERB", "", SHARED ) ;
			set_userAction( USR_ENTER ) ;
			set_nondispl_enter() ;
			propagateEnd = false ;
		}
		else if ( dotResp == "END" )
		{
			p_poolMGR->put( errBlock, "ZVERB", "END", SHARED ) ;
			set_userAction( USR_END ) ;
			set_nondispl_end() ;
			propagateEnd = false ;
		}
	}
	else if ( dotResp == "ENTER" || dotResp == "END" )
	{
		clr_nondispl() ;
		propagateEnd = false ;
	}
}


void pApplication::set_ZVERB_panel_resp()
{
	const string& dotResp = currPanel->get_resp() ;

	if ( dotResp == "ENTER" )
	{
		p_poolMGR->put( errBlock, "ZVERB", "", SHARED ) ;
		set_userAction( USR_ENTER ) ;
		propagateEnd = false ;
	}
	else if ( dotResp == "END" )
	{
		p_poolMGR->put( errBlock, "ZVERB", "END", SHARED ) ;
		set_userAction( USR_END ) ;
		propagateEnd = false ;
	}
}


void pApplication::set_panel_zvars()
{
	funcPOOL.put2( errBlock, "ZCURFLD", currPanel->get_cursor() ) ;
	funcPOOL.put2( errBlock, "ZCURPOS", currPanel->get_csrpos() ) ;
}


void pApplication::libdef( const string& lib,
			   const string& type,
			   const string& id,
			   const string& procopt )
{
	// libdef - Add/remove a list of paths to the search order for panels, messages and tables
	//          or a generic type

	// Format:
	//         Application-level libraries
	//         LIBDEF ZxLIB                    - remove LIBDEF for search
	//         LIBDEF ZxLIB PATH ID(path-list) - add path-list to the search path
	//         X - M, P or T
	//
	//         LIBDEF ZTABL                    - remove LIBDEF
	//         LIBDEF ZTABL PATH ID(path-list) - add path to LIBDEF lib-type
	//
	//         LIBDEF MYLIB                    - remove generic LIBDEF
	//         LIBDEF MYLIB PATH ID(path-list) - add path to generic LIBDEF lib-type
	//
	// Path-list is a colon-separated list of directory names that must exist.

	// Search order for non-generic lib-types: user-level, application-level, system-level

	// RC = 0   Normal completion
	// RC = 4   Removing a LIBDEF that was not in effect
	//          STKADD specified but no stack in effect
	// RC = 8   COND specified but a LIBDEF is already in effect
	// RC = 16  No paths in the ID() parameter, or invalid file name
	// RC = 20  Severe error

	const string e1 = "LIBDEF Error" ;

	int i ;
	int p ;

	bool proc_cond   ;
	bool proc_uncond ;
	bool proc_stack  ;
	bool proc_stkadd ;

	string dirname   ;

	map<string,stack<string>>::iterator it ;
	pair<map<string, stack<string>>::iterator, bool> result ;

	RC = 0 ;

	if ( !isvalidName( lib ) )
	{
		errBlock.setcall( e1, "PSYS012U" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( procopt != "" && !findword( procopt, "COND UNCOND STACK STKADD" ) )
	{
		errBlock.setcall( e1, "PSYE022I", procopt ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	proc_cond   = ( procopt == "COND"   ) ;
	proc_uncond = ( procopt == "UNCOND" ) ;
	proc_stack  = ( procopt == "STACK" || procopt == "" ) ;
	proc_stkadd = ( procopt == "STKADD" ) ;

	if ( type == "" )
	{
		if ( id != "" || procopt != "" )
		{
			errBlock.setcall( e1, "PSYE023K" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		it = zlibd.find( lib ) ;
		if ( it == zlibd.end() || it->second.empty() )
		{
			RC = 4 ;
			return ;
		}
		it->second.pop() ;
		if ( it->second.empty() )
		{
			zlibd.erase( it ) ;
		}
	}
	else if ( type == "PATH" )
	{
		p = getpaths( id ) ;
		if ( p == 0 )
		{
			errBlock.setcall( e1, "PSYE022F", 16 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		for ( i = 1 ; i <= p ; ++i )
		{
			dirname = getpath( id, i ) ;
			try
			{
				if ( !exists( dirname ) || !is_directory( dirname ) )
				{
					errBlock.setcall( e1, "PSYE023L", dirname, 16 ) ;
					checkRCode( errBlock ) ;
					return ;
				}
			}
			catch ( boost::filesystem::filesystem_error &e )
			{
				errBlock.setcall( e1, "PSYS012C", e.what() ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			catch (...)
			{
				errBlock.setcall( e1, "PSYS012C", "Entry: "+ dirname ) ;
				checkRCode( errBlock ) ;
				return ;
			}
		}
		it = zlibd.find( lib ) ;
		if ( proc_cond && ( it != zlibd.end() && !it->second.empty() ) )
		{
			RC = 8 ;
			return ;
		}
		else if ( proc_stkadd && ( it == zlibd.end() || it->second.empty() ) )
		{
			RC = 4 ;
			return ;
		}
		if ( it == zlibd.end() )
		{
			result = zlibd.insert( pair<string, stack<string>>( lib, stack<string>() ) ) ;
			it = result.first ;
		}
		if ( proc_cond || proc_uncond )
		{
			if ( it->second.empty() )
			{
				it->second.push( id ) ;
			}
			else
			{
				it->second.top() = id ;
			}
		}
		else if ( proc_stack )
		{
			it->second.push( id ) ;
		}
		else
		{
			it->second.top() = mergepaths( id, it->second.top() ) ;
		}
	}
	else
	{
		errBlock.setcall( e1, "PSYE022G", type ) ;
		checkRCode( errBlock ) ;
	}
}


void pApplication::qlibdef( const string& lib, const string& type_var, const string& id_var )
{
	// query libdef status for lib-type lib

	const string e1 = "QLIBDEF Error" ;

	map<string,stack<string>>::iterator it ;

	RC = 0 ;

	if ( !isvalidName( lib ) )
	{
		errBlock.setcall( e1, "PSYS012U" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	it = zlibd.find( lib ) ;
	if ( it == zlibd.end() || it->second.empty() )
	{
		RC = 4 ;
		return ;
	}

	if ( type_var != "" )
	{
		funcPOOL.put1( errBlock, type_var, "PATH" ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}

	if ( id_var != "" )
	{
		funcPOOL.put1( errBlock, id_var, it->second.top() ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
}


string pApplication::get_search_path( s_paths p )
{
	// Return the search path depending on the LIBDEFs in effect

	// Search Order:
	//                No LIBDEF       LIBDEF
	//                ----------------------
	//                                ZMUSR
	// Messages       ZMLIB           LIBDEF
	//                                ZMLIB
	//                ----------------------
	//                                ZPUSR
	// Panels         ZPLIB           LIBDEF
	//                                ZPLIB
	//                ----------------------
	//                                ZTUSR
	// Table Input    ZTLIB           LIBDEF
	//                                ZTLIB
	//                ----------------------
	//                                ZTABU
	// Table Output   ZTABL           LIBDEF
	//                                ZTABL
	//                ----------------------

	string* zzxusr = NULL ;
	string* zzxlib = NULL ;

	map<string,stack<string>>::iterator it ;

	switch ( p )
	{
	case s_ZMLIB:
		zzxusr = &zzmusr ;
		it     = zlibd.find( "ZMLIB" ) ;
		zzxlib = &zzmlib ;
		break ;

	case s_ZPLIB:
		zzxusr = &zzpusr ;
		it     = zlibd.find( "ZPLIB" ) ;
		zzxlib = &zzplib ;
		break ;

	case s_ZTLIB:
		zzxusr = &zztusr ;
		it     = zlibd.find( "ZTLIB" ) ;
		zzxlib = &zztlib ;
		break ;

	case s_ZTABL:
		zzxusr = &zztabu ;
		it     = zlibd.find( "ZTABL" ) ;
		zzxlib = &zztabl ;
		break ;
	}

	if ( it == zlibd.end() || it->second.empty() )
	{
		return *zzxlib ;
	}
	else if ( *zzxusr == "" )
	{
		return mergepaths( it->second.top(), *zzxlib ) ;
	}
	else
	{
		return mergepaths( *zzxusr, it->second.top(), *zzxlib ) ;
	}
}


string pApplication::get_search_path( const string& lib )
{
	// Return the search path depending on the LIBDEFs in effect

	const string e1 = "LIBRARY Error" ;

	if ( !isvalidName( lib ) )
	{
		errBlock.setcall( e1, "PSYS012U" ) ;
		checkRCode( errBlock ) ;
		return "" ;
	}

	if      ( lib == "ZMLIB" ) { return get_search_path( s_ZMLIB ) ; }
	else if ( lib == "ZPLIB" ) { return get_search_path( s_ZPLIB ) ; }
	else if ( lib == "ZTLIB" ) { return get_search_path( s_ZTLIB ) ; }
	else if ( lib == "ZTABL" ) { return get_search_path( s_ZTABL ) ; }

	auto it = zlibd.find( lib ) ;
	if ( it == zlibd.end() || it->second.empty() )
	{
		errBlock.setcall( e1, "PSYE022H", lib ) ;
		checkRCode( errBlock ) ;
		return "" ;
	}

	return it->second.top() ;
}


void pApplication::set_screenName()
{
	if ( p_poolMGR->get( errBlock, "ZSCRNAM1", SHARED ) == "ON" &&
	     p_poolMGR->get( errBlock, lscreen_num, "ZSCRNAM2" ) == "PERM" )
	{
		p_poolMGR->put( errBlock, "ZSCRNAME", p_poolMGR->get( errBlock, lscreen_num, "ZSCRNAME" ), SHARED ) ;
	}
}


string pApplication::get_zsel()
{
	string zsel = p_poolMGR->get( errBlock, "ZSEL", SHARED ) ;
	p_poolMGR->put( errBlock, "ZSEL", "", SHARED ) ;

	return zsel ;
}


const string pApplication::get_trail()
{
	if ( currPanel )
	{
		return currPanel->get_trail() ;
	}
	return "" ;
}


void pApplication::set_cursor( int row, int col )
{
	if ( currPanel ) { currPanel->set_cursor( row, col ) ; }
}


void pApplication::set_cursor_home()
{
	if ( currPanel ) { currPanel->set_cursor_home() ; }
}


void pApplication::vdefine( const string& names,
			    int* i_ad1,
			    int* i_ad2,
			    int* i_ad3,
			    int* i_ad4,
			    int* i_ad5,
			    int* i_ad6,
			    int* i_ad7,
			    int* i_ad8,
			    int* i_ad9,
			    int* i_ad10 )
{
	// RC = 0  Normal completion
	// RC = 20 Severe Error
	// (funcPOOL.define returns 0 or 20)

	int w ;

	const string e1 = "VDEFINE Error" ;

	string name ;

	RC = 0 ;

	w = words( names ) ;
	if ( ( w > 10 ) || ( w < 1 ) )
	{
		errBlock.setcall( e1, "PSYE022D" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( i_ad1 == NULL )
	{
		errBlock.setcall( e1, "PSYE022E" ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	name = word( names, 1 ) ;
	funcPOOL.define( errBlock, name, i_ad1 ) ;
	if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }

	if ( w > 1 )
	{
		if ( i_ad2 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 2 ) ;
		funcPOOL.define( errBlock, name, i_ad2 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 2 )
	{
		if ( i_ad3 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 3 ) ;
		funcPOOL.define( errBlock, name, i_ad3 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 3 )
	{
		if ( i_ad4 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 4 ) ;
		funcPOOL.define( errBlock, name, i_ad4 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 4 )
	{
		if ( i_ad5 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 5 ) ;
		funcPOOL.define( errBlock, name, i_ad5 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 5 )
	{
		if ( i_ad6 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 6 ) ;
		funcPOOL.define( errBlock, name, i_ad6 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}

	if ( w > 6 )
	{
		if ( i_ad7 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 7 ) ;
		funcPOOL.define( errBlock, name, i_ad7 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}

	if ( w > 7 )
	{
		if ( i_ad8 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 8 ) ;
		funcPOOL.define( errBlock, name, i_ad8 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}

	if ( w > 8 )
	{
		if ( i_ad9 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 9 ) ;
		funcPOOL.define( errBlock, name, i_ad9 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}

	if ( w > 9 )
	{
		if ( i_ad10 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 10 ) ;
		funcPOOL.define( errBlock, name, i_ad10 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
}



void pApplication::vdefine( const string& names,
			    string* s_ad1,
			    string* s_ad2,
			    string* s_ad3,
			    string* s_ad4,
			    string* s_ad5,
			    string* s_ad6,
			    string* s_ad7,
			    string* s_ad8,
			    string* s_ad9,
			    string* s_ad10 )
{
	// RC = 0  Normal completion
	// RC = 20 Severe Error
	// (funcPOOL.define returns 0 or 20)

	int w  ;

	string name ;
	const string e1 = "VDEFINE Error" ;

	RC = 0 ;

	w = words( names ) ;
	if ( ( w > 10 ) || ( w < 1 ) )
	{
		errBlock.setcall( e1, "PSYE022D" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( s_ad1 == NULL )
	{
		errBlock.setcall( e1, "PSYE022E" ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	name = word( names, 1 ) ;
	funcPOOL.define( errBlock, name, s_ad1 ) ;
	if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }

	if ( w > 1 )
	{
		if ( s_ad2 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 2 ) ;
		funcPOOL.define( errBlock, name, s_ad2 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 2 )
	{
		if ( s_ad3 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 3 ) ;
		funcPOOL.define( errBlock, name, s_ad3 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 3 )
	{
		if ( s_ad4 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 4 ) ;
		funcPOOL.define( errBlock, name, s_ad4 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 4 )
	{
		if ( s_ad5 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 5 ) ;
		funcPOOL.define( errBlock, name, s_ad5 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 5 )
	{
		if ( s_ad6 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 6 ) ;
		funcPOOL.define( errBlock, name, s_ad6 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 6 )
	{
		if ( s_ad7 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 7 ) ;
		funcPOOL.define( errBlock, name, s_ad7 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 7 )
	{
		if ( s_ad8 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 8 ) ;
		funcPOOL.define( errBlock, name, s_ad8 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 8 )
	{
		if ( s_ad9 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 9 ) ;
		funcPOOL.define( errBlock, name, s_ad9 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
	if ( w > 9 )
	{
		if ( s_ad10 == NULL )
		{
			errBlock.setcall( e1, "PSYE022E" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		name = word( names, 10 ) ;
		funcPOOL.define( errBlock, name, s_ad10 ) ;
		if ( errBlock.error() ) { checkRCode( errBlock ) ; return ; }
	}
}


void pApplication::vdelete( const string& names )
{
	// RC = 0  Normal completion
	// RC = 8  At least one variable not found
	// RC = 20 Severe error
	// (funcPOOL.dlete returns 0, 8 or 20)

	int i  ;
	int ws ;

	errBlock.setmaxRC( 0 ) ;

	ws = words( names ) ;
	for ( i = 1 ; i <= ws ; ++i )
	{
		funcPOOL.dlete( errBlock, word( names, i ) ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( "VDELETE Error" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		errBlock.setmaxRC() ;
	}
	RC = errBlock.getmaxRC() ;
}


void pApplication::vmask( const string& name, const string& type, const string& mask )
{
	// Set a mask for a function pool variable (must be vdefined first)
	// Partial implementation as no VEDIT panel statement yet so this is never used

	// RC = 0  Normal completion
	// RC = 8  Variable not found
	// RC = 20 Severe error
	// (funcPOOL.setmask returns 0, 8 or 20)

	const string e1    = "VMASK Error" ;
	const string fmask = "IDATE STDDATE ITIME STDTIME JDATE JSTD" ;

	if ( type == "FORMAT" )
	{
		if ( wordpos( mask, fmask ) == 0 )
		{
			errBlock.setcall( e1, "PSYE022P", mask, fmask ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( type == "USER" )
	{
		if ( mask.size() > 20 )
		{
			errBlock.setcall( e1, "PSYE022Q" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		else
		{
			for ( unsigned int i = 0 ; i < mask.size() ; ++i )
			{
				if ( mask[i] != 'A' && mask[i] != 'B' && mask[i] != '9' &&
				     mask[i] != 'H' && mask[i] != 'N' && mask[i] != 'V' &&
				     mask[i] != 'S' && mask[i] != 'X' && mask[i] != '(' &&
				     mask[i] != ')' && mask[i] != '-' && mask[i] != '/' &&
				     mask[i] != ',' && mask[i] != '.' )
				{
					errBlock.setcall( e1, "PSYE022S", mask ) ;
					checkRCode( errBlock ) ;
					return ;
				}
			}
		}
	}
	else
	{
		errBlock.setcall( e1, "PSYE022R", type ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	funcPOOL.setmask( errBlock, name, mask ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::vreset()
{
	// Remove implicit and defined variables from the function pool.
	// Redefine funtion pool system variables.

	// RC = 0  Normal completion
	// RC = 20 Severe error
	// (funcPOOL.reset returns 0)

	funcPOOL.reset( errBlock ) ;
	RC = errBlock.getRC() ;

	funcPOOL.define( errBlock, "ZRC", &ZRC, NOCHECK ) ;
	funcPOOL.define( errBlock, "ZRSN", &ZRSN, NOCHECK ) ;
	funcPOOL.define( errBlock, "ZRESULT", &ZRESULT, NOCHECK ) ;

	funcPOOL.put2( errBlock, "ZTDTOP",   0 ) ;
	funcPOOL.put2( errBlock, "ZTDSELS",  0 ) ;
	funcPOOL.put2( errBlock, "ZTDDEPTH", 0 ) ;
	funcPOOL.put2( errBlock, "ZTDROWS",  0 ) ;
	funcPOOL.put2( errBlock, "ZTDVROWS", 0 ) ;
	funcPOOL.put2( errBlock, "ZCURPOS",  1 ) ;
	funcPOOL.put2( errBlock, "ZCURINX",  0 ) ;
	funcPOOL.put2( errBlock, "ZZCRP",    0 ) ;
	funcPOOL.put2( errBlock, "ZSBTASK",  0 ) ;
}


void pApplication::vreplace( const string& name, const string& s_val )
{
	// RC = 0  Normal completion
	// RC = 20 Severe error
	// (funcPOOL.put returns 0, 20)

	funcPOOL.put1( errBlock, name, s_val ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( "VREPLACE Error" ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::vreplace( const string& name, int i_val )
{
	// RC = 0  Normal completion
	// RC = 20 Severe error
	// (funcPOOL.put returns 0 or 20)

	funcPOOL.put1( errBlock, name, i_val ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( "VREPLACE Error" ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::vget( const string& names, poolType pType )
{
	// RC = 0  Normal completion
	// RC = 8  Variable not found
	// RC = 20 Severe error
	// (funcPOOL.put returns 0 or 20)
	// (funcPOOL.getType returns 0, 8 or 20.  For RC = 8 create implicit function pool variable)
	// (poolMGR.get return 0, 8 or 20)

	int ws ;
	int i  ;

	string val  ;
	string name ;

	const string e1 = "VGET Error" ;

	dataType var_type ;

	errBlock.setmaxRC( 0 ) ;

	ws = words( names ) ;
	for ( i = 1 ; i <= ws ; ++i )
	{
		name = word( names, i ) ;
		val  = p_poolMGR->get( errBlock, name, pType ) ;
		if ( errBlock.RC0() )
		{
			var_type = funcPOOL.getType( errBlock, name ) ;
			if ( errBlock.RC0() )
			{
				if ( var_type == INTEGER )
				{
					funcPOOL.put1( errBlock, name, ds2d( val ) ) ;
				}
				else
				{
					funcPOOL.put1( errBlock, name, val ) ;
				}
			}
			else if ( errBlock.RC8() )
			{
				funcPOOL.put1( errBlock, name, val ) ;
			}
		}
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		errBlock.setmaxRC() ;
	}
	RC = errBlock.getmaxRC() ;
}


void pApplication::vput( const string& names, poolType pType )
{
	// RC = 0  Normal completion
	// RC = 8  Variable not found in function pool
	// RC = 12 Read-only variable
	// RC = 16 Truncation occured
	// RC = 20 Severe error
	// (funcPOOL.get returns 0, 8 or 20)
	// (funcPOOL.getType returns 0, 8 or 20)
	// (poolMGR.put return 0, 12, 16 or 20)

	int i  ;
	int ws ;

	string s_val ;
	string name  ;

	const string e1 = "VPUT Error" ;

	dataType var_type ;

	errBlock.setmaxRC( 0 ) ;

	ws = words( names ) ;
	for ( i = 1 ; i <= ws ; ++i )
	{
		name     = word( names, i ) ;
		var_type = funcPOOL.getType( errBlock, name ) ;
		if ( errBlock.RC0() )
		{
			if ( var_type == INTEGER )
			{
				s_val = d2ds( funcPOOL.get( errBlock, 0, var_type, name ) ) ;
			}
			else
			{
				s_val = funcPOOL.get( errBlock, 0, name ) ;
			}
			if ( errBlock.error() )
			{
				errBlock.setcall( e1 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			p_poolMGR->put( errBlock, name, s_val, pType ) ;
		}
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		errBlock.setmaxRC() ;
	}
	RC = errBlock.getmaxRC() ;
}


void pApplication::vcopy( const string& var, string& val, vcMODE mode )
{
	// Retrieve a copy of a dialogue variable name in var and move to variable val
	// (normal dialogue variable search order)
	// This routine is only valid for MODE=MOVE

	// RC = 0  Normal completion
	// RC = 8  Variable not found
	// RC = 16 Truncation occured
	// RC = 20 Severe error

	// (funcPOOL.get returns 0, 8 or 20)
	// (funcPOOL.getType returns 0, 8 or 20)
	// (poolMGR.get return 0, 8 or 20)

	dataType var_type ;

	const string e1 = "VCOPY Error" ;

	switch ( mode )
	{
	case LOCATE:
		errBlock.setcall( e1, "PSYE022A" ) ;
		checkRCode( errBlock ) ;
		return ;

	case MOVE:
		var_type = funcPOOL.getType( errBlock, var ) ;
		if ( errBlock.RC0() )
		{
			if ( var_type == INTEGER )
			{
				val = d2ds( funcPOOL.get( errBlock, 0, var_type, var ) ) ;
			}
			else
			{
				val = funcPOOL.get( errBlock, 0, var ) ;
			}
			if ( errBlock.error() )
			{
				errBlock.setcall( e1 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
		}
		else if ( errBlock.RC8() )
		{
			val = p_poolMGR->get( errBlock, var, ASIS ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e1 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
		}
		else
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	RC = errBlock.getRC() ;
}


void pApplication::vcopy( const string& var, string*& p_val, vcMODE mode )
{
	// Return the address of a dialogue variable name in var, in p_val pointer.
	// (normal dialogue variable search order)
	// This routine is only valid for MODE=LOCATE
	// MODE=LOCATE not valid for integer pointers as these may be in the variable pools as strings

	// RC = 0  Normal completion
	// RC = 8  Variable not found
	// RC = 16 Truncation occured
	// RC = 20 Severe error

	// (funcPOOL.get returns 0, 8 or 20)
	// (funcPOOL.getType returns 0, 8 or 20)
	// (funcPOOL.vlocate returns 0, 8 or 20)
	// (poolMGR.get return 0, 8 or 20)

	dataType var_type  ;

	const string e1 = "VCOPY Error" ;

	switch ( mode )
	{
	case LOCATE:
		var_type = funcPOOL.getType( errBlock, var ) ;
		if ( errBlock.RC0() )
		{
			if ( var_type == INTEGER )
			{
				errBlock.setcall( e1, "PSYE022C" ) ;
			}
			else
			{
				p_val = funcPOOL.vlocate( errBlock, var ) ;
			}
			if ( errBlock.error() )
			{
				errBlock.setcall( e1 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
		}
		else if ( errBlock.RC8() )
		{
			p_val = p_poolMGR->vlocate( errBlock, var, ASIS ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e1 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
		}
		else
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		break ;
	case MOVE:
		errBlock.setcall( e1, "PSYE022B" ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::verase( const string& names, poolType pType )
{
	// RC = 0  Normal completion
	// RC = 8  Variable not found
	// RC = 12 Read-only variable
	// RC = 20 Severe error
	// (poolMGR.erase return 0, 8 12 or 20)


	int i  ;
	int ws ;

	string name ;

	errBlock.setmaxRC( 0 ) ;

	ws = words( names ) ;
	for ( i = 1 ; i <= ws ; ++i )
	{
		name = word( names, i ) ;
		p_poolMGR->erase( errBlock, name, pType ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( "VERASE Error" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		errBlock.setmaxRC() ;
	}
	RC = errBlock.getmaxRC() ;
}


set<string>& pApplication::vlist( poolType pType, int lvl )
{
	return p_poolMGR->vlist( errBlock, RC, pType, lvl ) ;
}


set<string>& pApplication::vilist( vdType defn )
{
	return funcPOOL.vilist( RC, defn ) ;
}


set<string>& pApplication::vslist( vdType defn )
{
	return funcPOOL.vslist( RC, defn ) ;
}


void pApplication::addpop( const string& a_fld, int a_row, int a_col )
{
	// Create pop-up window and set row/col for the next panel display.
	// If addpop() is already active, store old values for next rempop()

	// Position of addpop is relative to row=1, col=3 or the previous addpop() position for this logical screen.
	// Defaults are 0,0 giving row=1, col=3

	// RC = 0  Normal completion
	// RC = 12 No panel displayed before addpop() service when using field parameter
	// RC = 20 Severe error

	const string e1 = "ADDPOP Error" ;

	popup t ;

	uint p_row = 0 ;
	uint p_col = 0 ;

	RC = 0 ;

	if ( a_fld != "" )
	{
		if ( !currPanel )
		{
			errBlock.setcall( e1, "PSYE022L", 12 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		if ( !currPanel->field_get_row_col( a_fld, p_row, p_col ) )
		{
			errBlock.setcall( e1, "PSYE022M", a_fld, currPanel->panelid, 20 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		a_row += p_row ;
		a_col += p_col - 4 ;
	}

	if ( addpop_active )
	{
		if ( currPanel )
		{
			currPanel->create_panels( t ) ;
		}
		t.row  = addpop_row ;
		t.col  = addpop_col ;
		popups.push( t ) ;
		a_row += addpop_row ;
		a_col += addpop_col ;
	}

	addpop_active = true ;
	addpop_row = ( a_row <  0 ) ? 1 : a_row + 2 ;
	addpop_col = ( a_col < -1 ) ? 2 : a_col + 4 ;

	if ( currPanel )
	{
		currPanel->show_popup() ;
	}
}


void pApplication::rempop( const string& r_all )
{
	// Remove pop-up window.  Restore previous addpop() if there is one.

	// RC = 0  Normal completion
	// RC = 16 No pop-up window exists at this level
	// RC = 20 Severe error

	const string e1 = "REMPOP Error" ;

	pPanel* panl ;

	RC = 0 ;

	if ( r_all != "" && r_all != "ALL" )
	{
		errBlock.setcall( e1, "PSYE022U", r_all ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( !addpop_active )
	{
		errBlock.setcall( e1, "PSYE022T", 16 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( r_all == "ALL" )
	{
		while ( !popups.empty() )
		{
			panl = static_cast<pPanel*>( popups.top().panl ) ;
			if ( panl )
			{
				panl->delete_panels( popups.top() ) ;
			}
			popups.pop() ;
		}
	}

	if ( !popups.empty() )
	{
		addpop_col = popups.top().col ;
		addpop_row = popups.top().row ;
		panl = static_cast<pPanel*>( popups.top().panl ) ;
		if ( panl )
		{
			panl->delete_panels( popups.top() ) ;
		}
		popups.pop() ;
	}
	else
	{
		addpop_active = false ;
		addpop_row    = 0 ;
		addpop_col    = 0 ;
	}
}


void pApplication::movepop( int row, int col )
{
	if ( addpop_active )
	{
		row = row - 1 ;
		col = col - 3 ;
		addpop_row = (row <  0 ) ? 1 : row + 2 ;
		addpop_col = (col < -1 ) ? 2 : col + 4 ;
		currPanel->set_popup( true, addpop_row, addpop_col ) ;
		currPanel->move_popup() ;
	}
}


void pApplication::control( const string& parm1, const string& parm2, const string& parm3 )
{
	// CONTROL ERRORS CANCEL - abend for RC >= 12
	// CONTROL ERRORS RETURN - return to application for any RC

	// CONTROL DISPLAY SAVE/RESTORE - SAVE/RESTORE status for a TBDISPL
	//         SAVE/RESTORE saves/restores the function pool variables associated with a tb display
	//         (the six ZTD* variables and the .ZURID.ln variables), and also the currtbPanel
	//         pointer for retrieving other pending sets via a tbdispl with no panel specified.
	//         Only necessary if a tbdispl invokes another tbdispl within the same task

	// CONTROL SPLIT  DISABLE - RC=8 if screen already split

	// CONTROL PASSTHRU LRSCROLL  PASON | PASOFF | PASQUERY

	// CONTROL REFLIST UPDATE
	// CONTROL REFLIST NOUPDATE

	// lspf extensions:
	// ----------------
	//
	// CONTROL CUA      RELOAD   - Reload the CUA tables
	// CONTROL CUA      NORELOAD - Stop reloading the CUA tables
	// CONTROL ABENDRTN DEFAULT  - Reset abend routine to the default, pApplication::cleanup_default
	// CONTROL REFLIST  ON       - REFLIST retrieve is on for this application.  ZRESULT will replace field.
	// CONTROL REFLIST  OFF      - REFLIST retrieve is off for this application.
	// CONTROL NOTIFY   JOBEND   - Send notify message when background job ends

	int i ;

	const string e1 = "CONTROL Error" ;

	map<string, pPanel*>::iterator it;

	errBlock.setRC( 0 ) ;

	if ( parm3 != "" && parm1 != "PASSTHRU" )
	{
		errBlock.setcall( e1, "PSYE022V", parm1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( parm1 == "CUA" )
	{
		if ( parm2 == "RELOAD" )
		{
			reloadCUATables = true ;
		}
		else if ( parm2 == "NORELOAD" )
		{
			reloadCUATables = false ;
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "CUA", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( parm1 == "DISPLAY" )
	{
		if ( parm2 == "LOCK" )
		{
			ControlDisplayLock = true ;
		}
		else if ( parm2 == "REFRESH" )
		{
			refreshlScreen = true ;
		}
		else if ( parm2 == "SAVE" )
		{
			stk_int.push( funcPOOL.get( errBlock, 0, INTEGER, "ZTDDEPTH", NOCHECK ) ) ;
			stk_int.push( funcPOOL.get( errBlock, 0, INTEGER, "ZTDROWS",  NOCHECK ) ) ;
			stk_int.push( funcPOOL.get( errBlock, 0, INTEGER, "ZTDSELS",  NOCHECK ) ) ;
			stk_int.push( funcPOOL.get( errBlock, 0, INTEGER, "ZTDTOP",   NOCHECK ) ) ;
			stk_int.push( funcPOOL.get( errBlock, 0, INTEGER, "ZTDVROWS", NOCHECK ) ) ;
			tbpanel_stk.push( currtbPanel ) ;
			if ( currtbPanel && currtbPanel->tb_depth > 0 )
			{
				urid_stk.push( stack<string>() ) ;
				for ( i = 0 ; i < currtbPanel->tb_depth ; ++i )
				{
					urid_stk.top().push( funcPOOL.get( errBlock, 8, ".ZURID."+d2ds( i ), NOCHECK ) ) ;
				}
			}
		}
		else if ( parm2 == "RESTORE" )
		{
			if ( tbpanel_stk.empty() )
			{
				errBlock.setcall( e1, "PSYE022W" ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			funcPOOL.put2( errBlock, "ZTDVROWS", stk_int.top() ) ;
			stk_int.pop() ;
			funcPOOL.put2( errBlock, "ZTDTOP", stk_int.top() ) ;
			stk_int.pop() ;
			funcPOOL.put2( errBlock, "ZTDSELS", stk_int.top() ) ;
			stk_int.pop() ;
			funcPOOL.put2( errBlock, "ZTDROWS", stk_int.top() ) ;
			stk_int.pop() ;
			funcPOOL.put2( errBlock, "ZTDDEPTH", stk_int.top() ) ;
			stk_int.pop() ;
			currtbPanel = tbpanel_stk.top() ;
			tbpanel_stk.pop() ;
			if ( currtbPanel && currtbPanel->tb_depth > 0 && !urid_stk.empty() )
			{
				stack<string>* ptr_stk = &urid_stk.top() ;
				i = ptr_stk->size() ;
				while ( !ptr_stk->empty() )
				{
					--i ;
					funcPOOL.put3( errBlock, ".ZURID."+d2ds( i ), ptr_stk->top() ) ;
					ptr_stk->pop() ;
				}
				urid_stk.pop() ;
			}
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "DISPLAY", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( parm1 == "NONDISPL" )
	{
		if ( parm2 == "ENTER" || parm2 == "" )
		{
			set_nondispl_enter() ;
		}
		else if ( parm2 == "END" )
		{
			set_nondispl_end() ;
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "NONDISPL", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( parm1 == "ERRORS" )
	{
		if ( parm2 == "RETURN" )
		{
			ControlErrorsReturn = true ;
		}
		else if ( parm2 == "CANCEL" )
		{
			ControlErrorsReturn = false ;
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "ERRORS", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( parm1 == "NOTIFY" )
	{
		if ( parm2 == "JOBEND" )
		{
			if ( backgrd ) { notifyEnded = true ; }
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "NOTIFY", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( parm1 == "PASSTHRU" )
	{
		if ( parm2 == "LRSCROLL" )
		{
			if ( parm3 == "PASON" )
			{
				ControlPassLRScroll = true ;
				for ( it = panelList.begin() ; it != panelList.end() ; ++it )
				{
					it->second->lrScroll = true ;
				}
			}
			else if ( parm3 == "PASOFF" )
			{
				ControlPassLRScroll = false ;
				for ( it = panelList.begin() ; it != panelList.end() ; ++it )
				{
					it->second->lrScroll = false ;
				}
			}
			else if ( parm3 == "PASQUERY" )
			{
				errBlock.setRC( ControlPassLRScroll ? 1 : 0 ) ;
			}
			else
			{
				errBlock.setcall( e1, "PSYE022X", "PASSTHRU LRSCROLL", parm3 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "PASSTHRU", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( parm1 == "REFLIST" )
	{
		if ( parm2 == "UPDATE" )
		{
			p_poolMGR->put( errBlock, lscreen_num, "ZREFUPDT", "Y" ) ;
		}
		else if ( parm2 == "NOUPDATE" )
		{
			p_poolMGR->put( errBlock, lscreen_num, "ZREFUPDT", "N" ) ;
		}
		else if ( parm2 == "ON" )
		{
			reffield = "#REFLIST" ;
		}
		else if ( parm2 == "OFF" )
		{
			reffield = "" ;
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "REFLIST", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( parm1 == "SPLIT" )
	{
		if ( parm2 == "ENABLE" )
		{
			ControlSplitEnable = true ;
		}
		else if ( parm2 == "DISABLE" )
		{
			if ( p_poolMGR->get( errBlock, "ZSPLIT", SHARED ) == "YES" )
			{
				errBlock.setRC( 8 ) ;
			}
			else
			{
				ControlSplitEnable = false ;
			}
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "SPLIT", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else if ( parm1 == "ABENDRTN" )
	{
		if ( parm2 == "DEFAULT" )
		{
			pcleanup = &pApplication::cleanup_default ;
		}
		else
		{
			errBlock.setcall( e1, "PSYE022X", "ABENDTRN", parm2 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	else
	{
		errBlock.setcall( e1, "PSYE022Y", parm1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	RC = errBlock.getRC() ;
}


void pApplication::control( const string& parm1, void (pApplication::*pFunc)() )
{
	// lspf extensions:
	//
	// CONTROL ABENDRTN ptr_to_routine - Set the routine to get control during an abend

	const string e1 = "CONTROL Error" ;

	errBlock.setRC( 0 ) ;

	if ( parm1 == "ABENDRTN" )
	{
		pcleanup = pFunc ;
	}
	else
	{
		errBlock.setcall( e1, "PSYE022Y", parm1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	RC = errBlock.getRC() ;
}


void pApplication::log( const string& msgid )
{
	// RC = 0   Normal completion
	// RC = 12  Message not found or message syntax error
	// RC = 20  Severe error

	string t ;

	getmsg( msgid, "ZERRSM", "ZERRLM" ) ;

	if ( RC == 0 )
	{
		vcopy( "ZERRSM", t, MOVE ) ;
		llog( "L", t << endl )     ;
		vcopy( "ZERRLM", t, MOVE ) ;
		llog( "L", t << endl )     ;
	}
}


void pApplication::qtabopen( const string& tb_list )
{
	// RC = 0   Normal completion
	// RC = 12  Variable name prefix too long (max 7 characters)
	// RC = 20  Severe error

	p_tableMGR->qtabopen( errBlock, funcPOOL, tb_list ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( "QTABOPEN Error" ) ;
		checkRCode( errBlock ) ;
	}
}


void pApplication::tbadd( const string& tb_name,
			  string tb_namelst,
			  const string& tb_order,
			  int tb_num_of_rows )
{
	// Add a new row to a table

	// RC = 0   Normal completion
	// RC = 8   For keyed tables only, row already exists
	// RC = 12  Table not open
	// RC = 20  Severe error

	const string e1 = "TBADD Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( tb_order != "" && tb_order != "ORDER" )
	{
		errBlock.setcall( e1, "PSYE023A", "TBADD", tb_order ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	if ( tb_num_of_rows < 0 || tb_num_of_rows > 65535 )
	{
		errBlock.setcall( e1, "PSYE023B", "TBADD", d2ds( tb_num_of_rows ) ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	getNameList( errBlock, tb_namelst ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	p_tableMGR->tbadd( errBlock, funcPOOL, tb_name, tb_namelst, tb_order, tb_num_of_rows ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbbottom( const string& tb_name,
			     const string& tb_savenm,
			     const string& tb_rowid_vn,
			     const string& tb_noread,
			     const string& tb_crp_name )
{
	// Move row pointer to the bottom

	// RC = 0   Normal completion
	// RC = 8   Table is empty.  CRP is set to 0
	// RC = 12  Table not open
	// RC = 20  Severe error

	const string e1 = "TBBOTTOM Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbbottom( errBlock, funcPOOL, tb_name, tb_savenm, tb_rowid_vn, tb_noread, tb_crp_name ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbclose( const string& tb_name, const string& tb_newname, string tb_paths )
{
	// Save and close the table (calls saveTable and destroyTable routines).
	// If table opened in NOWRITE mode, just remove table from storage.

	// If tb_paths is not specified, use ZTABL as the output path.  Error if blank.

	// RC = 0   Normal completion.
	// RC = 12  Table not open
	// RC = 16  Path error
	// RC = 20  Severe error

	const string e1 = "TBCLOSE Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( tb_newname != "" && !isvalidName( tb_newname ) )
	{
		errBlock.setcall( e1, "PSYE022J", tb_newname, "table" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( p_tableMGR->writeableTable( errBlock, tb_name, "TBCLOSE" ) )
	{
		if ( tb_paths == "" )
		{
			tb_paths = get_search_path( s_ZTABL ) ;
		}
		else if ( tb_paths.size() < 9 && tb_paths.find( '/' ) == string::npos )
		{
			tb_paths = get_search_path( iupper( tb_paths ) ) ;
			if ( errBlock.error() ) { return ; }
		}
		if ( tb_paths == "" )
		{
			errBlock.setcall( e1, "PSYE013C", 16 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		p_tableMGR->saveTable( errBlock, "TBCLOSE", tb_name, tb_newname, tb_paths ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	if ( errBlock.RC0() )
	{
		p_tableMGR->destroyTable( errBlock, tb_name, "TBCLOSE" ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbcreate( const string& tb_name,
			     string tb_keys,
			     string tb_names,
			     tbWRITE tb_WRITE,
			     tbREP tb_REP,
			     string tb_paths,
			     tbDISP tb_DISP )
{
	// Create a new table.
	// tb_paths is an input library to check if the table already exists if opened in WRITE mode.
	// Default to ZTLIB if blank.

	// RC = 0   Normal completion
	// RC = 4   Normal completion - Table exists and REPLACE speified
	// RC = 8   Table exists and REPLACE not specified or REPLACE specified and opend in SHARE mode or
	//          REPLACE specified and opened in NON_SHARE mode but not the owning task.
	// RC = 12  Table in use
	// RC = 16  WRITE specified but input library not specified (not currently used)
	// RC = 20  Severe error

	int ws ;
	int i  ;

	string w ;

	const string e1 = "TBCREATE Error" ;

	RC = 0 ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( tb_paths == "" )
	{
		tb_paths = get_search_path( s_ZTLIB ) ;
	}
	else if ( tb_paths.size() < 9 && tb_paths.find( '/' ) == string::npos )
	{
		tb_paths = get_search_path( iupper( tb_paths ) ) ;
		if ( errBlock.error() ) { return ; }
	}

	getNameList( errBlock, tb_keys ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	for ( ws = words( tb_keys ), i = 1 ; i <= ws ; ++i )
	{
		w = word( tb_keys, i ) ;
		if ( !isvalidName( w ) )
		{
			errBlock.setcall( e1, "PSYE022J", w, "key" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}

	getNameList( errBlock, tb_names ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	for ( ws = words( tb_names ), i = 1; i <= ws ; ++i )
	{
		w = word( tb_names, i ) ;
		if ( !isvalidName( w ) )
		{
			errBlock.setcall( e1, "PSYE022J", w, "field" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}

	p_tableMGR->tbcreate( errBlock, tb_name, tb_keys, tb_names, tb_REP, tb_WRITE, tb_paths, tb_DISP ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	RC = errBlock.getRC() ;
}


void pApplication::tbdelete( const string& tb_name )
{
	// Delete a row in the table.  For keyed tables, the table is searched with the current key.  For non-keyed tables the current CRP is used.

	// RC = 0   Normal completion
	// RC = 8   Row does not exist for a keyed table or for non-keyed table, CRP was at TOP(zero)
	// RC = 12  Table not open
	// RC = 20  Severe error

	const string e1 = "TBDELETE Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbdelete( errBlock, funcPOOL, tb_name ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbdispl( const string& tb_name,
			    string p_name,
			    const string& p_msg,
			    string p_cursor,
			    int p_csrrow,
			    int p_curpos,
			    string p_autosel,
			    const string& p_crp_name,
			    const string& p_rowid_nm,
			    const string& p_msgloc )
{
	// tbdispl with panel, no message - clear pending lines, rebuild scrollable area and display panel
	// tbdispl with panel, message    - clear pending lines, rebuild scrollable area and display panel and message
	// tbdispl no panel, no message   - retrieve next pending line.  If none, display panel
	// tbdispl no panel, message      - display panel with message.  No rebuilding of the scrollable area

	// Set CRP to first changed line or tbtop if there are no selected lines
	// ln is the tb screen line of the table CRP when invoking )REINIT and )PROC sections (CRP-ZTDTOP)

	// If .AUTOSEL and .CSRROW set in panel, override the parameters p_autosel and p_csrrow
	// Autoselect if the p_curpos CRN is visible

	// Store panel pointer in currtbPanel so that a CONTROL DISPLAY SAVE/RESTORE is only necessary
	// when a TBDISPL issues another TBDISPL and not for a display of an ordinary panel.

	// RC =  0  Normal completion
	// RC =  4  More than 1 row selected
	// RC =  8  End pressed
	// RC = 12  Panel, message or cursor field not found
	// RC = 20  Severe error

	int i   ;
	int ln  ;
	int idr ;
	int ztdtop  ;
	int ztdrows ;
	int ztdsels ;
	int exitRC  ;

	int    zscrolln ;
	string zscrolla ;

	bool tbscan ;
	bool rebuild = true ;

	string zzverb ;
	string URID ;
	string s ;

	map<string,pPanel*>::iterator it ;

	const string e1 = "Error during TBDISPL of panel "+ p_name ;
	const string e2 = "Error processing )INIT section of panel "   ;
	const string e3 = "Error processing )REINIT section of panel " ;
	const string e4 = "Error processing )PROC section of panel "   ;
	const string e5 = "Error during update of panel " ;
	const string e6 = "Error updating field values of panel " ;
	const string e7 = "Error processing )ATTR section of panel " ;
	const string e8 = "Background job attempted to display panel " ;

	if ( not busyAppl )
	{
		llog( "E", "Invalid method state" <<endl ) ;
		llog( "E", "Application is in a wait state.  Method cannot invoke display services." <<endl ) ;
		RC = 20 ;
		return ;
	}

	if ( backgrd )
	{
		llog( "B", e8 + p_name <<endl ) ;
		RC = 8 ;
		return ;
	}

	RC = 0 ;
	ln = 0 ;

	if ( currPanel )
	{
		currPanel->hide_popup() ;
		currPanel->clear_msg() ;
	}

	prevPanel = currPanel ;

	if ( propagateEnd )
	{
		if ( !currtbPanel )
		{
			propagateEnd = false ;
		}
		else if ( prevPanel && prevPanel->panelid == p_name )
		{
			propagateEnd = false ;
		}
		else
		{
			set_nondispl_enter() ;
		}
	}

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( p_cursor != "" && !isvalidName( p_cursor ) )
	{
		errBlock.setcall( e1, "PSYE023I", p_cursor ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( p_autosel != "YES" && p_autosel != "NO" && p_autosel != "" )
	{
		errBlock.setcall( e1, "PSYE023J", p_autosel ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( p_crp_name != "" && !isvalidName( p_crp_name ) )
	{
		errBlock.setcall( e1, "PSYE022O", "CRP", p_crp_name ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( p_rowid_nm != "" && !isvalidName( p_rowid_nm ) )
	{
		errBlock.setcall( e1, "PSYE022O", "ROW", p_rowid_nm ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( p_msgloc != "" && !isvalidName( p_msgloc ) )
	{
		errBlock.setcall( e1, "PSYE031L", p_msgloc, "MSGLOC()" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( p_name != "" )
	{
		it = createPanel( p_name ) ;
		if ( errBlock.error() ) { return ; }
		currPanel   = it->second ;
		currtbPanel = it->second ;
		currPanel->tb_clear_linesChanged( errBlock ) ;
	}
	else
	{
		if ( currtbPanel == NULL )
		{
			errBlock.setcall( e1, "PSYE021C" ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		currPanel = currtbPanel ;
	}

	currPanel->init_control_variables() ;

	currPanel->set_msg( p_msg ) ;
	currPanel->tb_set_autosel( p_autosel == "YES" || p_autosel == "" ) ;
	currPanel->tb_set_csrrow( p_csrrow ) ;
	currPanel->set_msgloc( p_msgloc ) ;
	currPanel->set_cursor( p_cursor, p_curpos ) ;

	currPanel->set_popup( addpop_active, addpop_row, addpop_col ) ;

	p_poolMGR->put( errBlock, "ZVERB", "",  SHARED ) ;
	usr_action = USR_ENTER ;

	if ( p_name != "" )
	{
		currPanel->display_panel_init( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e2 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		currPanel->display_panel_attrs( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e7 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		set_ZVERB_panel_resp_re_init() ;
		set_panel_zvars() ;
	}
	else
	{
		tbquery( tb_name, "", "", "", "", "", "ZZCRP" ) ;
		i  = max( 1, funcPOOL.get( errBlock, 0, INTEGER, "ZZCRP", NOCHECK ) ) ;
		ln = i - funcPOOL.get( errBlock, 0, INTEGER, "ZTDTOP", NOCHECK ) ;
		currPanel->display_panel_reinit( errBlock, ln ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e3 + currPanel->panelid ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		set_ZVERB_panel_resp_re_init() ;
		set_panel_zvars() ;
		if ( !currPanel->tb_linesPending() || p_msg != "" )
		{
			rebuild = false ;
			p_name  = currPanel->panelid ;
			currPanel->tb_add_autosel_line( errBlock ) ;
		}
	}

	if ( currPanel->get_msg() != "" )
	{
		get_message( currPanel->get_msg() ) ;
		if ( RC > 0 ) { RC = 12 ; return ; }
		currPanel->set_panel_msg( zmsg, zmsgid ) ;
	}
	else
	{
		currPanel->clear_msg() ;
	}

	tbscan = currPanel->get_tbscan() ;
	if ( rebuild && p_name != "" )
	{
		p_tableMGR->fillfVARs( errBlock,
				       funcPOOL,
				       tb_name,
				       currPanel->get_tb_fields(),
				       currPanel->get_tb_clear(),
				       tbscan,
				       currPanel->tb_depth,
				       -1,
				       currPanel->tb_get_csrrow(),
				       idr ) ;
		currPanel->set_cursor_idr( idr ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e1 ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		currPanel->update_field_values( errBlock ) ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e6 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
	}

	set_screenName() ;

	while ( true )
	{
		if ( p_name != "" )
		{
			p_poolMGR->put( errBlock, "ZPANELID", p_name, SHARED, SYSTEM ) ;
			tbquery( tb_name, "", "", "", "", "", "ZZCRP" ) ;
			currPanel->tb_set_crp( funcPOOL.get( errBlock, 0, INTEGER, "ZZCRP", NOCHECK ) ) ;
			currPanel->cursor_placement( errBlock ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e1, "PSYE022N", currPanel->get_cursor() ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			currPanel->display_panel( errBlock ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e1 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			if ( lineInOutDone && not ControlNonDispl )
			{
				refreshlScreen = true ;
				wait_event( WAIT_USER ) ;
				lineInOutDone  = false ;
				refreshlScreen = false ;
			}
			wait_event( WAIT_USER ) ;
			ControlDisplayLock = false ;
			refreshlScreen     = false ;
			reloadCUATables    = false ;
			currPanel->clear_msg() ;
			currPanel->display_panel_update( errBlock ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e5 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			if ( currPanel->get_msg() != "" )
			{
				get_message( currPanel->get_msg() ) ;
				if ( RC > 0 ) { return ; }
				currPanel->set_panel_msg( zmsg, zmsgid ) ;
				if ( not propagateEnd && not end_pressed() )
				{
					continue ;
				}
			}
			if ( currPanel->do_redisplay() ) { continue ; }
			currPanel->tb_add_autosel_line( errBlock ) ;
			currPanel->tb_set_linesChanged( errBlock ) ;
		}

		exitRC = 0  ;
		if ( currPanel->tb_curidx > -1 )
		{
			URID = funcPOOL.get( errBlock, 8, ".ZURID."+d2ds( currPanel->tb_curidx ), NOCHECK ) ;
			if ( errBlock.error() )
			{
				checkRCode( errBlock ) ;
				return ;
			}
			if ( URID != "" )
			{
				tbskip( tb_name, 0, "", "", URID, "NOREAD", "ZCURINX" ) ;
			}
			else
			{
				funcPOOL.put2( errBlock, "ZCURINX", 0 ) ;
			}
		}
		else
		{
			funcPOOL.put2( errBlock, "ZCURINX", 0 ) ;
		}
		if ( currPanel->tb_get_lineChanged( errBlock, ln, URID ) )
		{
			tbskip( tb_name, 0, "", p_rowid_nm, URID, "", p_crp_name ) ;
			for ( auto it = currPanel->tb_fields.begin() ; it != currPanel->tb_fields.end() ; ++it )
			{
				s = *it ;
				funcPOOL.put2( errBlock, s, funcPOOL.get( errBlock, 0, s+"."+ d2ds( ln ), NOCHECK ) ) ;
				if ( errBlock.error() )
				{
					checkRCode( errBlock ) ;
					return ;
				}
			}
			ztdsels = funcPOOL.get( errBlock, 0, INTEGER, "ZTDSELS", NOCHECK ) ;
			if ( ztdsels > 1 ) { exitRC = 4; }
		}

		currPanel->display_panel_proc( errBlock, ln ) ;
		clr_nondispl() ;
		if ( errBlock.error() )
		{
			errBlock.setcall( e4 + p_name ) ;
			checkRCode( errBlock ) ;
			return ;
		}
		set_ZVERB_panel_resp() ;
		set_panel_zvars() ;
		zzverb = p_poolMGR->get( errBlock, "ZVERB", SHARED ) ;
		if ( propagateEnd || end_pressed() )
		{
			if ( usr_action == USR_RETURN ) { propagateEnd = true ; }
			RC = 8 ;
			return ;
		}

		if ( currPanel->get_msg() != "" )
		{
			get_message( currPanel->get_msg() ) ;
			if ( RC > 0 ) { RC = 12 ; return ; }
			currPanel->set_panel_msg( zmsg, zmsgid ) ;
			if ( p_name == "" )
			{
				p_name = currPanel->panelid ;
				p_poolMGR->put( errBlock, "ZPANELID", p_name, SHARED, SYSTEM ) ;
			}
			currPanel->display_panel_reinit( errBlock, ln ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e3 + p_name ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			set_ZVERB_panel_resp_re_init() ;
			set_panel_zvars() ;
			continue ;
		}
		ztdsels = funcPOOL.get( errBlock, 0, INTEGER, "ZTDSELS", NOCHECK ) ;
		if ( ztdsels == 0 && ( zzverb == "UP" || zzverb == "DOWN" ) )
		{
			zscrolla = p_poolMGR->get( errBlock, "ZSCROLLA", SHARED ) ;
			if ( zzverb == "UP" )
			{
				if ( zscrolla == "MAX" )
				{
					ztdtop = 1 ;
				}
				else
				{
					ztdtop   = funcPOOL.get( errBlock, 0, INTEGER, "ZTDTOP", NOCHECK ) ;
					zscrolln = ds2d( p_poolMGR->get( errBlock, "ZSCROLLN", SHARED ) ) ;
					ztdtop   = ( ztdtop > zscrolln ) ? ( ztdtop - zscrolln ) : 1 ;
				}
			}
			else
			{
				if ( zscrolla == "MAX" )
				{
					ztdtop = funcPOOL.get( errBlock, 0, INTEGER, "ZTDROWS", NOCHECK ) + 1 ;
				}
				else
				{
					ztdtop   = funcPOOL.get( errBlock, 0, INTEGER, "ZTDTOP", NOCHECK ) ;
					ztdrows  = funcPOOL.get( errBlock, 0, INTEGER, "ZTDROWS", NOCHECK ) ;
					zscrolln = ds2d( p_poolMGR->get( errBlock, "ZSCROLLN", SHARED ) ) ;
					ztdtop   = ( zscrolln + ztdtop > ztdrows ) ? ( ztdrows + 1 ) : ztdtop + zscrolln ;
				}
			}
			p_poolMGR->put( errBlock, "ZVERB", "", SHARED ) ;
			p_tableMGR->fillfVARs( errBlock,
					       funcPOOL,
					       tb_name,
					       currPanel->get_tb_fields(),
					       currPanel->get_tb_clear(),
					       tbscan,
					       currPanel->tb_depth,
					       ztdtop,
					       0,
					       idr ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e6 + p_name ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			currPanel->update_field_values( errBlock ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e6 + p_name ) ;
				checkRCode( errBlock ) ;
				return ;
			}
			currPanel->set_cursor_home() ;
			continue ;
		}
		break ;
	}

	if ( funcPOOL.get( errBlock, 0, INTEGER, "ZTDSELS", NOCHECK ) == 0 )
	{
		tbtop( tb_name ) ;
		if ( p_crp_name != "" )
		{
			funcPOOL.put2( errBlock, p_crp_name, 0 ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e1 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
		}
		if ( p_rowid_nm != "" )
		{
			funcPOOL.put2( errBlock, p_rowid_nm, "" ) ;
			if ( errBlock.error() )
			{
				errBlock.setcall( e1 ) ;
				checkRCode( errBlock ) ;
				return ;
			}
		}
	}

	currPanel->tb_remove_lineChanged() ;
	RC = exitRC ;
}


void pApplication::tbend( const string& tb_name )
{
	// Close a table without saving (calls destroyTable routine).
	// If opened share, use count is reduced and table removed from storage when use count = 0.

	// RC = 0   Normal completion.
	// RC = 12  Table not open
	// RC = 20  Severe error

	const string e1 = "TBEND Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->destroyTable( errBlock, tb_name, "TBEND" ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = 0 ;
}


void pApplication::tberase( const string& tb_name, string tb_paths )
{
	// Erase a table file from the table output library

	// If tb_paths is not specified, use ZTABL as the output library.  Error if blank.

	// RC = 0   Normal completion
	// RC = 8   Table does not exist
	// RC = 12  Table in use
	// RC = 16  Path does not exist
	// RC = 20  Severe error

	const string e1 = "TBERASE Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( tb_paths == "" )
	{
		tb_paths = get_search_path( s_ZTABL ) ;
	}
	else if ( tb_paths.size() < 9 && tb_paths.find( '/' ) == string::npos )
	{
		tb_paths = get_search_path( iupper( tb_paths ) ) ;
		if ( errBlock.error() ) { return ; }
	}
	if ( tb_paths == "" )
	{
		errBlock.setcall( e1, "PSYE013C", 16 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	p_tableMGR->tberase( errBlock, tb_name, tb_paths ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbexist( const string& tb_name )
{
	// Test for the existance of a row in a keyed table

	// RC = 0   Normal completion
	// RC = 8   Row does not exist or not a keyed table
	// RC = 12  Table not open
	// RC = 20  Severe error

	const string e1 = "TBEXISTS Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbexist( errBlock, funcPOOL, tb_name ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}



void pApplication::tbget( const string& tb_name,
			  const string& tb_savenm,
			  const string& tb_rowid_vn,
			  const string& tb_noread,
			  const string& tb_crp_name )
{
	const string e1 = "TBGET Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbget( errBlock, funcPOOL, tb_name, tb_savenm, tb_rowid_vn, tb_noread, tb_crp_name  ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbmod( const string& tb_name,
			  string tb_namelst,
			  const string& tb_order )
{
	// Update/add a row in a table
	// Tables with keys        : Same as tbadd if row not found
	// Tables with without keys: Same as tbadd

	// RC = 0   Okay.  Keyed tables - row updated.  Non-keyed tables new row added
	// RC = 8   Row did not match - row added for keyed tables
	// RC = 16  Numeric conversion error
	// RC = 20  Severe error

	const string e1 = "TBMOD Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( tb_order != "" && tb_order != "ORDER" )
	{
		errBlock.setcall( e1, "PSYE023A", "TBMOD", tb_order ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	getNameList( errBlock, tb_namelst ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	p_tableMGR->tbmod( errBlock, funcPOOL, tb_name, tb_namelst, tb_order ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbopen( const string& tb_name,
			   tbWRITE tb_WRITE,
			   string tb_paths,
			   tbDISP tb_DISP )
{
	// Open an existing table, reading it from a file.
	// If aleady opened in SHARE mode, increment use count

	// RC = 0   Normal completion
	// RC = 8   Table does not exist in search path
	// RC = 12  Table already open by this or another task
	// RC = 16  paths not allocated
	// RC = 20  Severe error

	const string e1 = "TBOPEN Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( tb_paths == "" )
	{
		tb_paths = get_search_path( s_ZTLIB ) ;
	}
	else if ( tb_paths.size() < 9 && tb_paths.find( '/' ) == string::npos )
	{
		tb_paths = get_search_path( iupper( tb_paths ) ) ;
		if ( errBlock.error() ) { return ; }
	}

	p_tableMGR->loadTable( errBlock, tb_name, tb_WRITE, tb_paths, tb_DISP ) ;

	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbput( const string& tb_name, string tb_namelst, const string& tb_order )
{
	// Update the current row in a table
	// Tables with keys        : Keys must match CRP row
	// Tables with without keys: CRP row updated

	// RC = 0   Normal completion
	// RC = 8   Keyed tables - key does not match current row.  CRP set to top (0)
	//          Non-keyed tables - CRP at top
	// RC = 12  Table not open
	// RC = 16  Numeric conversion error for sorted tables
	// RC = 20  Severe error

	const string e1 = "TBPUT Error" ;

	RC = 0 ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( tb_order != "" && tb_order != "ORDER" )
	{
		errBlock.setcall( e1, "PSYE023A", "TBPUT", tb_order ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	getNameList( errBlock, tb_namelst ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	p_tableMGR->tbput( errBlock, funcPOOL, tb_name, tb_namelst, tb_order ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbquery( const string& tb_name,
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
	const string e1 = "TBQUERY Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbquery( errBlock,
			     funcPOOL,
			     tb_name,
			     tb_keyn,
			     tb_varn,
			     tb_rownn,
			     tb_keynn,
			     tb_namenn,
			     tb_crpn,
			     tb_sirn,
			     tb_lstn,
			     tb_condn,
			     tb_dirn ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbsarg( const string& tb_name,
			   const string& tb_namelst,
			   const string& tb_dir,
			   const string& tb_cond_pairs )
{
	const string e1 = "TBSARG Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbsarg( errBlock, funcPOOL, tb_name, tb_namelst, tb_dir, tb_cond_pairs ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbsave( const string& tb_name, const string& tb_newname, string tb_paths )
{
	// Save the table to disk (calls saveTable routine).  Table remains open for processing.
	// Table must be open in WRITE mode.

	// If tb_paths is not specified, use ZTABL as the output path.  Error if blank.

	// RC = 0   Normal completion
	// RC = 12  Table not open or not open WRITE
	// RC = 16  Alternate name save error
	// RC = 20  Severe error

	RC = 0 ;

	const string e1 = "TBSAVE Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	if ( tb_newname != "" && !isvalidName( tb_newname ) )
	{
		errBlock.setcall( e1, "PSYE022J", tb_newname, "table" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( !p_tableMGR->writeableTable( errBlock, tb_name, "TBSAVE" ) )
	{
		errBlock.setcall( e1 ) ;
		if ( !errBlock.error() )
		{
			errBlock.seterrid( "PSYE014S", tb_name, 12 ) ;
		}
		checkRCode( errBlock ) ;
		return ;
	}

	if ( tb_paths == "" )
	{
		tb_paths = get_search_path( s_ZTABL ) ;
	}
	else if ( tb_paths.size() < 9 && tb_paths.find( '/' ) == string::npos )
	{
		tb_paths = get_search_path( iupper( tb_paths ) ) ;
		if ( errBlock.error() ) { return ; }
	}
	if ( tb_paths == "" )
	{
		errBlock.setcall( e1, "PSYE013C", 16 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	p_tableMGR->saveTable( errBlock, "TBSAVE", tb_name, tb_newname, tb_paths ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbscan( const string& tb_name,
			   const string& tb_namelst,
			   const string& tb_savenm,
			   const string& tb_rowid_vn,
			   const string& tb_dir,
			   const string& tb_read,
			   const string& tb_crp_name,
			   const string& tb_condlst )
{
	const string e1 = "TBSCAN Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbscan( errBlock, funcPOOL, tb_name, tb_namelst, tb_savenm, tb_rowid_vn, tb_dir, tb_read, tb_crp_name, tb_condlst ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}



void pApplication::tbskip( const string& tb_name,
			   int num,
			   const string& tb_savenm,
			   const string& tb_rowid_vn,
			   const string& tb_rowid,
			   const string& tb_noread,
			   const string& tb_crp_name )
{
	const string e1 = "TBSKIP Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbskip( errBlock, funcPOOL, tb_name, num, tb_savenm, tb_rowid_vn, tb_rowid, tb_noread, tb_crp_name ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbsort( const string& tb_name, string tb_fields )
{
	const string e1 = "TBSORT Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	getNameList( errBlock, tb_fields ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	p_tableMGR->tbsort( errBlock, tb_name, tb_fields ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}



void pApplication::tbtop( const string& tb_name )
{
	const string e1 = "TBTOP Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbtop( errBlock, tb_name ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


void pApplication::tbvclear( const string& tb_name )
{
	const string e1 = "TBVCLEAR Error" ;

	if ( !tableNameOK( tb_name, e1 ) ) { return ; }

	p_tableMGR->tbvclear( errBlock, funcPOOL, tb_name ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	RC = errBlock.getRC() ;
}


bool pApplication::tableNameOK( const string& tb_name, const string& err )
{
	RC = 0 ;
	errBlock.setRC( 0 ) ;

	if ( tb_name == "" )
	{
		errBlock.seterrid( "PSYE013H" ) ;
	}
	else if ( !isvalidName( tb_name ) )
	{
		errBlock.seterrid( "PSYE014Q", tb_name ) ;
	}
	if ( errBlock.error() )
	{
		errBlock.setcall( err ) ;
		checkRCode( errBlock ) ;
		return false ;
	}
	return true ;
}


void pApplication::edrec( const string& m_parm )
{
	// RC =  0  INIT    - Edit recovery table created
	//          QUERY   - Recovery not pending
	//          PROCESS - Recovery completed and data saved
	// RC =  4  INIT    - Edit recovery table already exists for this application
	//          QUERY   - Entry found in the Edit Recovery Table, recovery pending
	//          PROCESS - Recovery completed but user did not save data
	// RC = 20  Severe error

	int xRC ;

	string uprof ;

	const string qname   = "ISRRECOV" ;
	const string rname   = "*in progress*" ;
	const string tabName = get_applid() + "EDRT" ;
	const string v_list  = "ZEDSTAT ZEDTFILE ZEDBFILE ZEDMODE ZEDOPTS ZEDUSER" ;

	vcopy( "ZUPROF", uprof, MOVE ) ;

	errBlock.setRC( 0 ) ;

	if ( m_parm == "INIT" )
	{
		xRC = edrec_init( m_parm,
				  qname,
				  rname,
				  uprof,
				  tabName,
				  v_list ) ;
	}
	else if ( m_parm == "QUERY" )
	{
		xRC = edrec_query( m_parm,
				   qname,
				   rname,
				   uprof,
				   tabName,
				   v_list ) ;
	}
	else if ( m_parm == "PROCESS" )
	{
		xRC = edrec_process( m_parm,
				     qname,
				     rname,
				     uprof,
				     tabName,
				     v_list ) ;
	}
	else if ( m_parm == "CANCEL" )
	{
		xRC = edrec_cancel( m_parm,
				    qname,
				    rname,
				    uprof,
				    tabName,
				    v_list ) ;
	}
	else if ( m_parm == "DEFER" )
	{
		xRC = edrec_defer( qname, rname ) ;
	}
	else
	{
		errBlock.setcall( "EDREC Error" ) ;
		errBlock.seterrid( "PSYE023M", m_parm ) ;
		checkRCode( errBlock ) ;
		xRC = RC ;
	}

	if ( xRC >= 12 )
	{
		errBlock.setcall( "EDREC Error" ) ;
		checkRCode( errBlock ) ;
		xRC = RC ;
	}

	RC = xRC ;
}


int pApplication::edrec_init( const string& m_parm,
			      const string& qname,
			      const string& rname,
			      const string& uprof,
			      const string& tabName,
			      const string& v_list )
{
	// RC =  0  Edit recovery table created
	// RC =  4  Edit recovery table already exists for this application
	// RC = 20  Severe error

	int i ;
	int xRC ;

	string zedstat  ;
	string zedtfile ;
	string zedbfile ;
	string zeduser  ;
	string zedmode  ;
	string zedopts  ;

	const string e1 = "EDREC INIT Error" ;

	qscan( qname, rname, EXC, LOCAL ) ;
	if ( RC == 0 )
	{
		errBlock.setcall( e1, "PSYE023O", "INIT" ) ;
		checkRCode( errBlock ) ;
		return RC ;
	}

	vdefine( v_list, &zedstat, &zedtfile, &zedbfile, &zedmode, &zedopts, &zeduser ) ;
	if ( RC > 0 ) { return 20 ; }

	tbopen( tabName, WRITE, uprof ) ;
	if ( RC == 0 )
	{
		tbend( tabName ) ;
		xRC = 4 ;
	}
	else if ( RC == 8 )
	{
		tbcreate( tabName, "", "(ZEDSTAT,ZEDTFILE,ZEDBFILE,ZEDMODE,ZEDOPTS,ZEDUSER)", WRITE, NOREPLACE, uprof ) ;
		if ( RC > 0 )
		{
			vdelete( v_list ) ;
			return 20 ;
		}
		tbvclear( tabName ) ;
		zedstat = "0" ;
		for ( i = 0 ; i < EDREC_SZ ; ++i )
		{
			tbadd( tabName ) ;
			if ( RC > 0 )
			{
				tbend( tabName ) ;
				vdelete( v_list ) ;
				return 20 ;
			}
		}
		tbclose( tabName, "", uprof ) ;
		if ( RC > 0 )
		{
			vdelete( v_list ) ;
			return 20 ;
		}
		xRC = 0 ;
	}
	else
	{
		xRC = 20 ;
	}

	vdelete( v_list ) ;
	return xRC ;
}


int pApplication::edrec_query( const string& m_parm,
			       const string& qname,
			       const string& rname,
			       const string& uprof,
			       const string& tabName,
			       const string& v_list )
{
	// RC =  0  Recovery not pending
	// RC =  4  Entry found in the Edit Recovery Table, recovery pending
	// RC = 20  Severe error

	int row ;

	string zedstat  ;
	string zedtfile ;
	string zedbfile ;
	string zeduser  ;
	string zedmode  ;
	string zedopts  ;

	const string e1 = "EDREC QUERY Error" ;

	qscan( qname, rname, EXC, LOCAL ) ;
	if ( RC == 0 )
	{
		errBlock.setcall( e1, "PSYE023O", "QUERY" ) ;
		checkRCode( errBlock ) ;
		return RC ;
	}

	row = funcPOOL.get( errBlock, 8, INTEGER, "ZEDROW", NOCHECK ) ;

	vdefine( v_list, &zedstat, &zedtfile, &zedbfile, &zedmode, &zedopts, &zeduser ) ;
	if ( RC > 0 ) { return 20 ; }

	tbopen( tabName, WRITE, uprof ) ;
	if ( RC == 8 )
	{
		vdelete( v_list ) ;
		return 0 ;
	}

	tbskip( tabName, row ) ;

	while ( true )
	{
		tbskip( tabName, 1 ) ;
		if ( RC > 0 ) { break ; }
		++row ;
		if ( zedstat == "0" ) { continue ; }
		enq( qname, zedtfile ) ;
		if ( RC == 8 ) { continue ; }
		tbend( tabName ) ;
		vdelete( v_list ) ;
		funcPOOL.put2( errBlock, "ZEDTFILE", zedtfile ) ;
		funcPOOL.put2( errBlock, "ZEDBFILE", zedbfile ) ;
		funcPOOL.put2( errBlock, "ZEDOPTS",  zedopts ) ;
		funcPOOL.put2( errBlock, "ZEDMODE",  zedmode ) ;
		funcPOOL.put2( errBlock, "ZEDUSER",  zeduser ) ;
		funcPOOL.put2( errBlock, "ZEDROW",   row ) ;
		p_poolMGR->put( errBlock, "ZEDUSER", zeduser, SHARED ) ;
		enq( qname, rname, EXC, LOCAL ) ;
		return 4 ;
	}

	tbend( tabName ) ;
	vdelete( v_list ) ;
	funcPOOL.put2( errBlock, "ZEDROW", 0 ) ;

	return 0 ;
}


int pApplication::edrec_process( const string& m_parm,
				 const string& qname,
				 const string& rname,
				 const string& uprof,
				 const string& tabName,
				 const string& v_list )
{
	// RC =  0  PROCESS - Recovery completed and data saved
	// RC =  4  PROCESS - Recovery completed but user did not save data
	// RC = 20  Severe error

	// ZEDOPTS  - byte 0 - confirm cancel
	//          - byte 1 - preserve trailing spaces

	int xRC ;
	int row ;

	string zedstat  ;
	string zedtfile ;
	string zedbfile ;
	string zeduser  ;
	string zedmode  ;
	string zedopts  ;

	edit_parms e ;

	const string e1 = "EDREC PROCESS Error" ;

	qscan( qname, rname, EXC, LOCAL ) ;
	if ( RC == 8 )
	{
		errBlock.setcall( e1, "PSYE023N", "PROCESS" ) ;
		checkRCode( errBlock ) ;
		return RC ;
	}

	zedtfile = funcPOOL.get( errBlock, 0, "ZEDTFILE", NOCHECK ) ;
	zedbfile = funcPOOL.get( errBlock, 0, "ZEDBFILE", NOCHECK ) ;
	zedopts  = funcPOOL.get( errBlock, 0, "ZEDOPTS", NOCHECK  ) ;
	zedmode  = funcPOOL.get( errBlock, 0, "ZEDMODE", NOCHECK  ) ;
	row      = funcPOOL.get( errBlock, 0, INTEGER, "ZEDROW", NOCHECK ) ;

	try
	{
		if ( not exists( zedbfile ) )
		{
			throw runtime_error( "" ) ;
		}
	}
	catch (...)
	{
		deq( qname, zedtfile ) ;
		errBlock.setcall( e1, "PSYE023P", zedbfile ) ;
		checkRCode( errBlock ) ;
		return RC ;
	}

	deq( qname, rname, LOCAL ) ;

	e.edit_recovery = true ;
	e.edit_viewmode = ( zedmode == "V" ) ;
	e.edit_file     = zedtfile ;
	e.edit_bfile    = zedbfile ;
	e.edit_confirm  = ( zedopts.size() > 0 && zedopts[ 0 ] == '1' ) ? "YES" : "NO" ;
	e.edit_preserve = ( zedopts.size() > 1 && zedopts[ 1 ] == '1' ) ? "PRESERVE" : "" ;

	edit_rec( e ) ;
	xRC = ( RC < 5 ) ? RC : 20 ;

	deq( qname, zedtfile ) ;

	remove( zedbfile ) ;

	vdefine( v_list, &zedstat, &zedtfile, &zedbfile, &zedmode, &zedopts, &zeduser ) ;
	if ( RC > 0 ) { return 20 ; }

	tbopen( tabName, WRITE, uprof ) ;
	if ( RC > 0 )
	{
		vdelete( v_list ) ;
		return 20 ;
	}

	tbskip( tabName, row ) ;
	if ( RC > 0 )
	{
		tbend( tabName ) ;
		vdelete( v_list ) ;
		return 20 ;
	}

	tbvclear( tabName ) ;
	zedstat = "0" ;
	tbput( tabName ) ;
	if ( RC > 0 )
	{
		tbend( tabName ) ;
		vdelete( v_list ) ;
		return 20 ;
	}

	tbclose( tabName, "", uprof ) ;
	if ( RC > 0 )
	{
		vdelete( v_list ) ;
		return 20 ;
	}

	vdelete( v_list ) ;

	return xRC ;
}


int pApplication::edrec_cancel( const string& m_parm,
				const string& qname,
				const string& rname,
				const string& uprof,
				const string& tabName,
				const string& v_list )
{
	// RC =  0  Normal completion
	// RC = 20  Severe error

	string zedstat  ;
	string zedtfile ;
	string zedbfile ;
	string zeduser  ;
	string zedmode  ;
	string zedopts  ;

	const string e1 = "EDREC CANCEL Error" ;

	deq( qname, rname, LOCAL ) ;
	if ( RC == 8 )
	{
		errBlock.setcall( e1, "PSYE023N", "CANCEL" ) ;
		checkRCode( errBlock ) ;
		return RC ;
	}

	int row = funcPOOL.get( errBlock, 8, INTEGER, "ZEDROW", NOCHECK ) ;

	zedtfile = funcPOOL.get( errBlock, 8, "ZEDTFILE", NOCHECK ) ;
	zedbfile = funcPOOL.get( errBlock, 8, "ZEDBFILE", NOCHECK ) ;

	deq( qname, zedtfile ) ;

	remove( zedbfile ) ;

	vdefine( v_list, &zedstat, &zedtfile, &zedbfile, &zedmode, &zedopts, &zeduser ) ;
	if ( RC > 0 ) { return 20 ; }

	tbopen( tabName, WRITE, uprof ) ;
	if ( RC > 0 )
	{
		vdelete( v_list ) ;
		return 20 ;
	}

	tbskip( tabName, row ) ;
	if ( RC > 0 )
	{
		tbend( tabName ) ;
		vdelete( v_list ) ;
		return 20 ;
	}

	tbvclear( tabName ) ;
	zedstat = "0" ;
	tbput( tabName ) ;
	if ( RC > 0 )
	{
		tbend( tabName ) ;
		vdelete( v_list ) ;
		return 20 ;
	}

	tbclose( tabName, "", uprof ) ;
	if ( RC > 0 )
	{
		vdelete( v_list ) ;
		return 20 ;
	}

	vdelete( v_list ) ;
	if ( RC > 0 )
	{
		return 20 ;
	}

	funcPOOL.put2( errBlock, "ZEDTFILE", "" ) ;
	funcPOOL.put2( errBlock, "ZEDBFILE", "" ) ;

	return 0 ;
}


int pApplication::edrec_defer( const string& qname, const string& rname )
{
	// RC =  0  Normal completion
	// RC = 20  Severe error

	string zedtfile ;

	const string e1 = "EDREC DEFER Error" ;

	deq( qname, rname, LOCAL ) ;
	if ( RC == 8 )
	{
		errBlock.setcall( e1, "PSYE023N", "DEFER" ) ;
		checkRCode( errBlock ) ;
		return RC ;
	}

	zedtfile = funcPOOL.get( errBlock, 8, "ZEDTFILE", NOCHECK ) ;

	deq( qname, zedtfile ) ;

	return 0 ;
}


void pApplication::edit( const string& m_file,
			 const string& m_panel,
			 const string& m_macro,
			 const string& m_profile,
			 const string& m_lcmds,
			 const string& m_confirm,
			 const string& m_preserve )
{
	// RC =  0  Normal completion.  Data was saved.
	// RC =  4  Normal completion.  Data was not saved.
	//          No changes made or CANCEL entered.
	// RC = 14  File in use
	// RC = 20  Severe error

	string t ;

	edit_parms e ;

	e.edit_file     = m_file ;
	e.edit_panel    = m_panel ;
	e.edit_macro    = m_macro ;
	e.edit_profile  = m_profile ;
	e.edit_lcmds    = m_lcmds   ;
	e.edit_confirm  = m_confirm ;
	e.edit_preserve = m_preserve ;

	selct.clear() ;
	selct.pgm     = p_poolMGR->get( errBlock, "ZEDITPGM", PROFILE ) ;
	selct.newappl = ""      ;
	selct.newpool = false   ;
	selct.passlib = false   ;
	selct.suspend = true    ;
	selct.scrname = "EDIT"  ;
	selct.options = (void*)&e ;
	selct.ptid    = ptid    ;
	actionSelect() ;

	RC = ZRC ;
	if ( ZRC > 11 && isvalidName( ZRESULT ) )
	{
		t = p_poolMGR->get( errBlock, "ZVAL1", SHARED ) ;
		errBlock.setcall( "EDIT Error", ZRESULT, t, ZRC ) ;
		checkRCode( errBlock ) ;
	}
}


void pApplication::edit_rec( const edit_parms& e )
{
	// Used internally to call edit or view during edit recovery.

	// RC =  0  Normal completion.  Data was saved.
	// RC =  4  Normal completion.  Data was not saved.
	//          No changes made or CANCEL entered.
	// RC = 14  File in use
	// RC = 20  Severe error

	string t ;

	selct.clear() ;
	selct.pgm     = p_poolMGR->get( errBlock, "ZEDITPGM", PROFILE ) ;
	selct.newappl = ""      ;
	selct.newpool = false   ;
	selct.passlib = false   ;
	selct.suspend = true    ;
	selct.scrname = ( e.edit_viewmode ) ? "VIEW" : "EDIT" ;
	selct.options = (void*)&e ;
	selct.ptid    = ptid    ;
	actionSelect() ;

	RC = ZRC ;
	if ( ZRC > 11 && isvalidName( ZRESULT ) )
	{
		t = p_poolMGR->get( errBlock, "ZVAL1", SHARED ) ;
		errBlock.setcall( "EDIT Error", ZRESULT, t, ZRC ) ;
		checkRCode( errBlock ) ;
	}
}


void pApplication::browse( const string& m_file, const string& m_panel )
{
	string t ;

	browse_parms b ;

	b.browse_file  = m_file ;
	b.browse_panel = m_panel ;

	selct.clear() ;
	selct.pgm     = p_poolMGR->get( errBlock, "ZBRPGM", PROFILE ) ;
	selct.parm    = "" ;
	selct.newappl = "" ;
	selct.newpool = false ;
	selct.passlib = false ;
	selct.suspend = true  ;
	selct.scrname = "BROWSE" ;
	selct.options = (void*)&b ;
	selct.ptid    = ptid     ;
	actionSelect() ;

	RC = ZRC ;
	if ( ZRC > 11 && isvalidName( ZRESULT ) )
	{
		t = p_poolMGR->get( errBlock, "ZVAL1", SHARED ) ;
		errBlock.setcall( "BROWSE Error", ZRESULT, t, ZRC ) ;
		checkRCode( errBlock ) ;
	}
}


void pApplication::view( const string& m_file,
			 const string& m_panel,
			 const string& m_macro,
			 const string& m_profile,
			 const string& m_lcmds,
			 const string& m_confirm,
			 const string& m_chgwarn )
{
	string t ;

	edit_parms e ;

	e.edit_file     = m_file ;
	e.edit_panel    = m_panel ;
	e.edit_macro    = m_macro ;
	e.edit_profile  = m_profile ;
	e.edit_lcmds    = m_lcmds   ;
	e.edit_confirm  = m_confirm ;
	e.edit_chgwarn  = m_chgwarn ;
	e.edit_viewmode = true      ;

	selct.clear() ;
	selct.pgm     = p_poolMGR->get( errBlock, "ZVIEWPGM", PROFILE ) ;
	selct.newappl = ""      ;
	selct.newpool = false   ;
	selct.passlib = false   ;
	selct.suspend = true    ;
	selct.scrname = "VIEW"  ;
	selct.options = (void*)&e ;
	selct.ptid    = ptid    ;
	actionSelect() ;

	RC = ZRC ;
	if ( ZRC > 11 && isvalidName( ZRESULT ) )
	{
		t = p_poolMGR->get( errBlock, "ZVAL1", SHARED ) ;
		errBlock.setcall( "VIEW Error", ZRESULT, t, ZRC ) ;
		checkRCode( errBlock ) ;
	}
}


void pApplication::select( const string& cmd )
{
	// SELECT a function or panel in keyword format for use in applications,
	// ie PGM(abc) CMD(oorexx) PANEL(xxx) PARM(zzz) NEWAPPL PASSLIB SCRNAME(abc) etc.

	// No variable substitution is done at this level.

	const string e1 = "Error in SELECT command " ;

	if ( !selct.parse( errBlock, cmd ) )
	{
		errBlock.setcall( e1 + cmd ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( backgrd && selct.pgmtype == PGM_PANEL )
	{
		errBlock.setcall( e1 + cmd, "PSYE039T" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	selct.backgrd = backgrd ;
	selct.sync    = true    ;
	selct.ptid    = ptid    ;

	actionSelect() ;
}


void pApplication::select( const selobj& sel )
{
	// SELECT a function or panel using a SELECT object (internal use only)

	const string e1 = "Error in SELECT command" ;

	if ( backgrd && sel.pgmtype == PGM_PANEL )
	{
		errBlock.setcall( e1, "PSYE039T" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	selct = sel ;
	selct.backgrd = backgrd ;
	selct.sync    = true    ;
	selct.ptid    = ptid    ;
	actionSelect() ;
}


void pApplication::submit( const string& cmd )
{
	// In the background, SELECT a function in keyword format for use in applications,
	// ie PGM(abc) CMD(oorexx) PARM(zzz) NEWAPPL PASSLIB etc.

	// No variable substitution is done at this level.

	const string e1 = "Error in SUBMIT command" ;

	if ( !selct.parse( errBlock, cmd ) )
	{
		errBlock.setcall( e1 ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( selct.pgmtype == PGM_PANEL )
	{
		errBlock.setcall( e1 + cmd, "PSYE039T" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	selct.backgrd = true  ;
	selct.sync    = false ;
	actionSelect() ;
}


void pApplication::submit( const selobj& sel )
{
	// Submit for background processing a function using a SELECT object (internal use only)

	const string e1 = "Error in SUBMIT command" ;

	if ( sel.pgmtype == PGM_PANEL )
	{
		errBlock.setcall( e1, "PSYE039T" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	selct = sel ;
	selct.backgrd = true  ;
	selct.sync    = false ;
	actionSelect() ;
}


void pApplication::actionSelect()
{
	// RC =  0  Normal completion of the selection panel or function.  END was entered.
	// RC =  4  Normal completion.  RETURN was entered or EXIT specified on the selection panel
	// RC = 20  Abnormal termination of the called task.

	// If the application has abended, propagate back (only set if not controlErrorsReturn).

	// If abnormal termination in the selected task:
	// ZRC = 20  ZRSN = 999  Application program abended.
	// ZRC = 20  ZRSN = 998  SELECT PGM not found.
	// ZRC = 20  ZRSN = 997  SELECT CMD not found.
	// ZRC = 20  ZRSN = 996  Errors loading program.
	// Don't percolate these codes back to the calling program (set ZRSN = 0) so it doesn't
	// appear to be abending with these codes (20/999/Abended instead).

	// BUG: selct.pgm will be blank for PANEL/CMD/SHELL in error messages (resolved in lspf.cpp)

	int exitRC ;

	RC  = 0    ;
	SEL = true ;

	if ( not busyAppl )
	{
		llog( "E", "Invalid method state" <<endl ) ;
		llog( "E", "Application is in a wait state.  Method cannot invoke SELECT services." <<endl ) ;
		RC = 20 ;
		return ;
	}

	p_poolMGR->put( errBlock, "ZVERB", "",  SHARED ) ;

	wait_event( WAIT_SELECT ) ;

	if ( RC == 4 )
	{
		propagateEnd = true ;
		currPanel    = NULL ;
		if ( not selct.selPanl )
		{
			RC = 0 ;
		}
	}

	SEL = false ;

	if ( abnormalEnd )
	{
		abnormalNoMsg = true ;
		if ( ZRC == 20 && ZRESULT == "PSYS013J" )
		{
			switch ( ZRSN )
			{
			case 996:
				errBlock.setcall( "Error in SELECT command", "PSYS013H", selct.pgm ) ;
				break ;

			case 997:
				errBlock.setcall( "Error in SELECT command", "PSYS012X", word( selct.parm, 1 ) ) ;
				break ;

			case 998:
				errBlock.setcall( "Error in SELECT command", "PSYS012W", selct.pgm ) ;
				break ;

			}
			ZRSN = 0 ;
			checkRCode( errBlock ) ;
		}
		llog( "E", "Percolating abend to calling application.  Taskid: "<< taskId <<endl ) ;
		abend() ;
	}

	exitRC = RC ;
	if ( ZRC == 20 && ZRESULT == "PSYS013J" )
	{
		switch ( ZRSN )
		{
		case 996:
			vreplace( "ZVAL1", selct.pgm ) ;
			ZRESULT = "PSYS013H" ;
			break ;

		case 997:
			vreplace( "ZVAL1", word( selct.parm, 1 ) ) ;
			ZRESULT = "PSYS012X" ;
			break ;

		case 998:
			vreplace( "ZVAL1", selct.pgm ) ;
			ZRESULT = "PSYS012W" ;
			break ;

		}
	}

	selct.clear() ;
	RC = exitRC ;
}


void pApplication::pquery( const string& p_name,
			   const string& a_name,
			   const string& t_name,
			   const string& w_name,
			   const string& d_name,
			   const string& r_name,
			   const string& c_name )
{
	// RC =  0  Normal completion
	// RC =  8  Specified area not found on panel
	// RC = 20  Severe error

	const string e1 = "PQUERY Error" ;
	const string e2 = "PQUERY Error for panel "+p_name ;

	map<string,pPanel*>::iterator it ;

	errBlock.setRC( 0 ) ;

	if ( p_name == "" )
	{
		errBlock.setcall( e1, "PSYE019C", "PANEL" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( !isvalidName( p_name ) )
	{
		errBlock.setcall( e1, "PSYE021A", p_name ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( a_name == "" )
	{
		errBlock.setcall( e2, "PSYE019C", "area name" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( !isvalidName( a_name ) )
	{
		errBlock.setcall( e2, "PSYE023C", "area", a_name ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( t_name != "" && !isvalidName( t_name ) )
	{
		errBlock.setcall( e2, "PSYE023C", "area type", t_name ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( w_name != "" && !isvalidName( w_name ) )
	{
		errBlock.setcall( e2, "PSYE023C", "area width", w_name ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( d_name != "" && !isvalidName( d_name ) )
	{
		errBlock.setcall( e2, "PSYE023C", "area depth", d_name ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( r_name != "" && !isvalidName( r_name ) )
	{
		errBlock.setcall( e2, "PSYE023C", "area row number", r_name ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( c_name != "" && !isvalidName( c_name ) )
	{
		errBlock.setcall( e2, "PSYE023C", "area column number", c_name ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	it = createPanel( p_name ) ;
	if ( errBlock.error() ) { return ; }

	it->second->get_panel_info( errBlock,
				    a_name,
				    t_name,
				    w_name,
				    d_name,
				    r_name,
				    c_name ) ;
	RC = errBlock.getRC() ;
}


void pApplication::reload_keylist( pPanel* p )
{
	// Does an unconditional reload every time a PF key is pressed.
	// Need to find a way to detect a change (TODO).

	load_keylist( p ) ;
}

void pApplication::load_keylist( pPanel* p )
{
	string tabName  ;
	string tabField ;

	string uprof ;

	if ( p->keylistn == "" || p_poolMGR->get( errBlock, "ZKLUSE", PROFILE ) != "Y" )
	{
		return ;
	}

	tabName = p->keyappl + "KEYP" ;

	vcopy( "ZUPROF", uprof, MOVE ) ;
	tbopen( tabName, NOWRITE, uprof, SHARE ) ;
	if ( RC > 0 )
	{
		errBlock.setcall( "KEYLIST Error", "PSYE023E", tabName ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	tbvclear( tabName ) ;
	vreplace( "KEYLISTN", p->keylistn ) ;
	tbget( tabName ) ;
	if ( RC > 0 )
	{
		tbend( tabName ) ;
		errBlock.setcall( "KEYLIST Error", "PSYE023F", p->keylistn, tabName ) ;
		checkRCode( errBlock ) ;
		return  ;
	}

	vcopy( "KEY1DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(1),  tabField ) ;
	vcopy( "KEY2DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(2),  tabField ) ;
	vcopy( "KEY3DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(3),  tabField ) ;
	vcopy( "KEY4DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(4),  tabField ) ;
	vcopy( "KEY5DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(5),  tabField ) ;
	vcopy( "KEY6DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(6),  tabField ) ;
	vcopy( "KEY7DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(7),  tabField ) ;
	vcopy( "KEY8DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(8),  tabField ) ;
	vcopy( "KEY9DEF",  tabField, MOVE ) ; p->put_keylist( KEY_F(9),  tabField ) ;
	vcopy( "KEY10DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(10), tabField ) ;
	vcopy( "KEY11DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(11), tabField ) ;
	vcopy( "KEY12DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(12), tabField ) ;
	vcopy( "KEY13DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(13), tabField ) ;
	vcopy( "KEY14DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(14), tabField ) ;
	vcopy( "KEY15DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(15), tabField ) ;
	vcopy( "KEY16DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(16), tabField ) ;
	vcopy( "KEY17DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(17), tabField ) ;
	vcopy( "KEY18DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(18), tabField ) ;
	vcopy( "KEY19DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(19), tabField ) ;
	vcopy( "KEY20DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(20), tabField ) ;
	vcopy( "KEY21DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(21), tabField ) ;
	vcopy( "KEY22DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(22), tabField ) ;
	vcopy( "KEY23DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(23), tabField ) ;
	vcopy( "KEY24DEF", tabField, MOVE ) ; p->put_keylist( KEY_F(24), tabField ) ;
	vcopy( "KEYHELPN", p->keyhelpn, MOVE ) ;

	tbend( tabName ) ;
}


void pApplication::notify( const string& msg, bool subVars )
{
	boost::lock_guard<boost::mutex> lock( mtx ) ;

	notifies.push_back( subVars ? sub_vars( msg ) : msg ) ;
}


bool pApplication::notify_pending()
{
	boost::lock_guard<boost::mutex> lock( mtx ) ;

	return ( notifies.size() > 0 && p_poolMGR->get( errBlock, "ZNOTIFY", PROFILE ) == "Y" ) ;
}


bool pApplication::notify()
{
	boost::lock_guard<boost::mutex> lock( mtx ) ;

	if ( notifies.size() == 0 || p_poolMGR->get( errBlock, "ZNOTIFY", PROFILE ) != "Y" )
	{
		return false ;
	}

	auto it = notifies.begin() ;

	outBuffer      = *it  ;
	lineInOutDone  = true ;
	notifies.erase( it )  ;
	return true ;
}


void pApplication::rdisplay( const string& msg, bool subVars )
{
	// Display line mode output on the screen.  Cancel any screen refreshes as this will be done
	// as part of returning to full screen mode.
	// If running in the background, log message to the application log.

	RC = 0 ;

	outBuffer = subVars ? sub_vars( msg ) : msg ;

	if ( backgrd )
	{
		llog( "B", outBuffer << endl ) ;
	}
	else
	{
		if ( not busyAppl )
		{
			llog( "E", "Invalid method state" <<endl ) ;
			llog( "E", "Application is in a wait state.  Method cannot issue line output" <<endl ) ;
			RC = 20 ;
			return ;
		}
		lineInOutDone  = true  ;
		lineOutPending = true  ;
		refreshlScreen = false ;
		wait_event( WAIT_OUTPUT ) ;
		lineOutPending = false ;
	}
}


void pApplication::pull( const string& var )
{
	// Pull data from user in raw mode and put in the function pool variable 'var'.

	const string e1 = "PULL Error" ;

	RC = 0 ;

	if ( !isvalidName( var ) )
	{
		errBlock.setcall( e1, "PSYS012U" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( backgrd )
	{
		llog( "E", "Cannot pull data in batch mode" << endl ) ;
		RC = 20 ;
	}
	else
	{
		if ( not busyAppl )
		{
			llog( "E", "Invalid method state" <<endl ) ;
			llog( "E", "Application is in a wait state.  Method cannot pull input" <<endl ) ;
			RC = 20 ;
			return ;
		}
		lineInOutDone = true  ;
		lineInPending = true  ;
		wait_event( WAIT_USER ) ;
		lineInPending = false ;
		funcPOOL.put1( errBlock, var, inBuffer ) ;
	}
}


void pApplication::pull( string* str )
{
	// Pull data from user in raw mode and put in the passed string pointer.

	RC = 0 ;

	if ( backgrd )
	{
		llog( "E", "Cannot pull data in batch mode" << endl ) ;
		RC = 20 ;
	}
	else
	{
		if ( not busyAppl )
		{
			llog( "E", "Invalid method state" <<endl ) ;
			llog( "E", "Application is in a wait state.  Method cannot pull input" <<endl ) ;
			RC = 20 ;
			return ;
		}
		lineInOutDone = true  ;
		lineInPending = true  ;
		wait_event( WAIT_USER ) ;
		lineInPending = false ;
		*str = inBuffer ;
	}
}


void pApplication::setmsg( const string& msg, msgSET sType )
{
	// Retrieve message and store in zmsg1 and zmsgid1.

	RC = 0 ;

	if ( ( sType == COND ) && setMessage ) { return ; }

	get_message( msg ) ;
	if ( RC > 0 )
	{
		RC = 20 ;
		return  ;
	}

	zmsg1      = zmsg ;
	zmsgid1    = msg  ;
	setMessage = true ;
}


void pApplication::getmsg( const string& msg,
			   const string& smsg,
			   const string& lmsg,
			   const string& alm,
			   const string& hlp,
			   const string& typ,
			   const string& wndo )
{
	// Load message msg and substitute variables

	// RC = 0   Normal completion
	// RC = 12  Message not found or message syntax error
	// RC = 20  Severe error

	const string e1 = "GETMSG Error" ;

	slmsg tmsg ;

	if ( smsg != "" && !isvalidName( smsg ) )
	{
		errBlock.setcall( e1, "PSYE022O", "SHORT MESSAGE", smsg ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	if ( lmsg != "" && !isvalidName( lmsg ) )
	{
		errBlock.setcall( e1, "PSYE022O", "LONG MESSAGE", lmsg ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	if ( alm != "" && !isvalidName( alm ) )
	{
		errBlock.setcall( e1, "PSYE022O", "ALARM", alm ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	if ( hlp != "" && !isvalidName( hlp ) )
	{
		errBlock.setcall( e1, "PSYE022O", "HELP", hlp ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	if ( typ != "" && !isvalidName( typ ) )
	{
		errBlock.setcall( e1, "PSYE022O", "TYPE", typ ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	if ( wndo != "" && !isvalidName( wndo ) )
	{
		errBlock.setcall( e1, "PSYE022O", "WINDOW", wndo ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( !load_message( msg ) ) { return ; }

	tmsg      = msgList[ msg ] ;
	tmsg.smsg = sub_vars( tmsg.smsg ) ;
	tmsg.lmsg = sub_vars( tmsg.lmsg ) ;

	if ( !sub_message_vars( tmsg ) )
	{
		errBlock.seterror( "Invalid variable value" ) ;
		checkRCode( errBlock ) ;
		return ;
	}

	if ( smsg != "" )
	{
		funcPOOL.put1( errBlock, smsg, tmsg.smsg ) ;
		if ( errBlock.error() )
		{
			errBlock.seterror( e1 ) ;
			checkRCode( errBlock )  ;
			return ;
		}
	}
	if ( lmsg != "" )
	{
		funcPOOL.put1( errBlock, lmsg, tmsg.lmsg ) ;
		if ( errBlock.error() )
		{
			errBlock.seterror( e1 ) ;
			checkRCode( errBlock )  ;
			return ;
		}
	}
	if ( alm != "" )
	{
		funcPOOL.put1( errBlock, alm, tmsg.alm ? "YES" : "NO" ) ;
		if ( errBlock.error() )
		{
			errBlock.seterror( e1 ) ;
			checkRCode( errBlock )  ;
			return ;
		}
	}
	if ( typ != "" )
	{
		switch ( tmsg.type )
		{
		case IMT: funcPOOL.put1( errBlock, typ, "NOTIFY" )   ;
				break ;

		case WMT: funcPOOL.put1( errBlock, typ, "WARNING" )  ;
				break ;

		case AMT: funcPOOL.put1( errBlock, typ, "CRITICAL" ) ;
		}
		if ( errBlock.error() )
		{
			errBlock.seterror( e1 ) ;
			checkRCode( errBlock )  ;
			return ;
		}
	}
	if ( hlp != "" )
	{
		funcPOOL.put1( errBlock, hlp, tmsg.hlp ) ;
		if ( errBlock.error() )
		{
			errBlock.seterror( e1 ) ;
			checkRCode( errBlock )  ;
			return ;
		}
	}
	if ( wndo != "" )
	{
		funcPOOL.put1( errBlock, wndo, tmsg.resp ? "RESP" : "NORESP" ) ;
		if ( errBlock.error() )
		{
			errBlock.seterror( e1 ) ;
			checkRCode( errBlock )  ;
			return ;
		}
	}
	RC = errBlock.getRC() ;
}


string pApplication::get_help_member( int row, int col )
{
	RC = 0 ;

	return "M("+ zmsg.hlp+ ") " +
	       "F("+ currPanel->get_field_help( row, col )+ ") " +
	       "P("+ currPanel->get_help() +") " +
	       "A("+ zapphelp +") " +
	       "K("+ currPanel->keyhelpn +") "+
	       "PATHS("+ get_search_path( s_ZPLIB ) +")" ;
}


void pApplication::get_message( const string& p_msg )
{
	// Load messages from message library and copy slmsg object to zmsg for the message requested

	// Substitute any dialogue variables in the short and long messages
	// Substitute any dialogue varibles in .TYPE, .WINDOW, .HELP and .ALARM parameters
	// Set zmsgid

	RC = 0 ;

	if ( !load_message( p_msg ) ) { return ; }

	zmsg      = msgList[ p_msg ]      ;
	zmsg.smsg = sub_vars( zmsg.smsg ) ;
	zmsg.lmsg = sub_vars( zmsg.lmsg ) ;

	if ( !sub_message_vars( zmsg ) ) { RC = 20 ; return ; }

	zmsgid = p_msg ;
}


bool pApplication::load_message( const string& p_msg )
{
	// Message format: 1-5 alph char prefix
	//                 3 numeric chars
	//                 1 alph char suffix (optional and only if prefix is less than 5)

	// Read messages and store in msgList map (no variable substitution done at this point)
	// Return false if message not found in member or there is an error (but still store individual messages from member)
	// Error on duplicate message-id in the file member

	// The message file name is determined by truncating the message ID after the second digit of the number.
	// AB123A file AB12
	// G012 file G01

	// If in test mode, reload message each time it is requested
	// Routine sets the return code:
	// RC =  0  Normal completion
	// RC = 12  Message not found
	// RC = 20  Severe error

	int i  ;
	int j  ;

	char c ;

	string p_msg_fn ;
	string filename ;
	string mline    ;
	string paths    ;
	string tmp      ;
	string msgid    ;
	string smsg     ;
	string lmsg     ;

	bool lcontinue = false ;

	slmsg t ;

	RC = 0  ;

	if ( testMode ) { msgList.clear() ; }
	else if ( msgList.count( p_msg ) > 0 ) { return true ; }

	i = check_message_id( p_msg ) ;

	if ( i == 0 || ( p_msg.size() - i > 3 && !isalpha( p_msg.back() ) ) )
	{
		RC = 20 ;
		zerr1 = "Message-id format invalid (1): "+ p_msg ;
		checkRCode() ;
		return false ;
	}

	p_msg_fn = p_msg.substr( 0, i+2 ) ;

	paths = get_search_path( s_ZMLIB ) ;

	for ( i = getpaths( paths ), j = 1 ; j <= i ; ++j )
	{
		filename = getpath( paths, j ) + p_msg_fn ;
		try
		{
			if ( exists( filename ) )
			{
				if ( !is_regular_file( filename ) )
				{
					RC = 20 ;
					zerr1 = "Message file "+ filename +" is not a regular file" ;
					checkRCode() ;
					return false ;
				}
				break ;
			}
		}
		catch ( boost::filesystem::filesystem_error &e )
		{
			RC = 20 ;
			zerr1 = "I/O error accessing entry" ;
			zerr2 = e.what() ;
			checkRCode() ;
			return false ;
		}
		catch (...)
		{
			RC = 20 ;
			zerr1 = "I/O error accessing entry" ;
			zerr2 = filename ;
			checkRCode() ;
			return false ;
		}
	}
	if ( j > i )
	{
		RC = 12 ;
		zerr1 = "Message file "+ p_msg_fn +" not found in ZMLIB for message-id "+ p_msg ;
		checkRCode() ;
		return false ;
	}

	msgid = "" ;
	smsg  = "" ;
	lmsg  = "" ;
	tmp   = "" ;

	std::ifstream messages( filename.c_str() ) ;
	if ( !messages.is_open() )
	{
		RC = 20 ;
		zerr1 = "Error opening message file "+ filename ;
		checkRCode() ;
		return false ;
	}

	while ( getline( messages, mline ) )
	{
		trim( mline ) ;
		if ( mline == "" || mline.front() == '*' ) { continue ; }
		if ( mline.compare( 0, 2, "/*" ) == 0 )    { continue ; }
		if ( mline.compare( 0, p_msg_fn.size(), p_msg_fn ) == 0 )
		{
			if ( msgid != "" )
			{
				if ( lcontinue || !t.parse( smsg, lmsg ) )
				{
					RC = 20 ;
					messages.close() ;
					zerr1 = "Error (1) in message-id "+ msgid ;
					checkRCode() ;
					return false ;
				}
				if ( msgList.count( msgid ) > 0 )
				{
					RC = 20 ;
					messages.close() ;
					zerr1 = "Duplicate message-id found: "+ msgid ;
					checkRCode() ;
					return false ;
				}
				msgList[ msgid ] = t ;
			}
			msgid = word( mline, 1 )    ;
			smsg  = subword( mline, 2 ) ;
			lmsg  = "" ;
			tmp   = "" ;
			i = check_message_id( msgid ) ;
			if ( i == 0 || ( msgid.size() - i > 3 && !isalpha( msgid.back() ) ) )
			{
				RC = 20 ;
				zerr1 = "Message-id format invalid (2): "+ msgid ;
				checkRCode() ;
				return false ;
			}
		}
		else
		{
			if ( msgid == "" || ( lmsg != "" && !lcontinue ) )
			{
				RC = 20 ;
				messages.close() ;
				zerr1 = "Extraeneous data: "+ mline ;
				checkRCode() ;
				return false ;
			}
			lcontinue = ( mline.back() == '+' ) ;
			if ( lcontinue )
			{
				mline.pop_back() ;
				trim( mline )    ;
			}
			c = mline.front() ;
			if ( c == '\'' || c == '"' )
			{
				if ( mline.back() != c )
				{
					RC = 20 ;
					messages.close() ;
					zerr1 = "Error (2) in message-id "+ msgid ;
					checkRCode() ;
					return false ;
				}
				tmp = dquote( errBlock, c, mline ) ;
				if ( errBlock.error() )
				{
					RC = 20 ;
					messages.close() ;
					zerr1 = "Error (3) in message-id "+ msgid ;
					checkRCode() ;
					return false ;
				}
			}
			else
			{
				if ( words( mline ) > 1 )
				{
					RC = 20 ;
					messages.close() ;
					zerr1 = "Error (4) in message-id "+ msgid ;
					checkRCode() ;
					return false ;
				}
				tmp = mline ;
			}
			lmsg = lmsg + tmp ;
			if ( lmsg.size() > 512 )
			{
				RC = 20 ;
				messages.close() ;
				zerr1 = "Long message size exceeds 512 bytes for message-id "+ msgid ;
				checkRCode() ;
				return false ;
			}
		}
	}
	if ( messages.bad() )
	{
		RC = 20 ;
		messages.close() ;
		zerr1 = "Error while reading message file "+ filename ;
		checkRCode() ;
		return false ;
	}
	messages.close() ;

	if ( smsg != "" )
	{
		if ( lcontinue || !t.parse( smsg, lmsg ) )
		{
			RC = 20 ;
			messages.close() ;
			zerr1 = "Error (5) in message-id "+ msgid ;
			checkRCode() ;
			return false ;
		}
		if ( msgList.count( msgid ) > 0 )
		{
			RC = 20 ;
			messages.close() ;
			zerr1 = "Duplicate message-id found: "+ msgid ;
			checkRCode() ;
			return false ;
		}
		msgList[ msgid ] = t ;
	}

	if ( msgList.count( p_msg ) == 0 )
	{
		RC = 12 ;
		zerr1 = "Message-id "+ p_msg +" not found in message file "+ p_msg_fn ;
		checkRCode() ;
		return false ;
	}
	return true ;
}


bool pApplication::sub_message_vars( slmsg& t )
{
	// Get the dialogue variable value specified in message .T, .A, .H and .W options

	// Error if the dialgue variable is blank
	// This routine does not set the return code.  Set in the calling routine.

	// Use defaults for invalid values

	// .TYPE overrides .WINDOW and .ALARM

	string val ;

	if ( t.dvwin != "" )
	{
		t.lmwin = true ;
		vcopy( t.dvwin, val, MOVE ) ;
		trim( val ) ;
		if      ( val == "RESP"    || val == "R"  ) { t.smwin = true  ; t.resp = true ; }
		else if ( val == "NORESP"  || val == "N"  ) { t.smwin = true  ; }
		else if ( val == "LRESP"   || val == "LR" ) { t.resp  = true  ; }
		else if ( val == "LNORESP" || val == "LN" ) {                   }
		else if ( val == "" ) { return false ; }
	}
	if ( t.dvalm != "" )
	{
		vcopy( t.dvalm, val, MOVE ) ;
		trim( val ) ;
		if      ( val == "YES" ) { t.alm = true  ; }
		else if ( val == "NO"  ) { t.alm = false ; }
		else if ( val == ""    ) { return false  ; }
	}
	if ( t.dvtype != "" )
	{
		vcopy( t.dvtype, val, MOVE ) ;
		trim( val ) ;
		if      ( val == "N" ) { t.type = IMT ; t.alm = false ; }
		else if ( val == "W" ) { t.type = WMT ; t.alm = true  ; }
		else if ( val == "A" ) { t.type = AMT ; t.alm = true  ; }
		else if ( val == "C" ) { t.type = AMT ; t.alm = true  ; t.resp = true ; t.smwin = true ; t.lmwin = true ; }
		else if ( val == ""  ) { return false ; }
	}
	if ( t.dvhlp != "" )
	{
		vcopy( t.dvhlp, t.hlp, MOVE ) ;
	}
	return true ;
}


int pApplication::check_message_id( const string& msgid )
{
	// Return 0 if message-id format is incorrect, else the offset to the first numeric triplet

	int i ;
	int l ;

	l = msgid.size() ;

	if ( l < 4 || !isvalidName( msgid ) )
	{
		return 0 ;
	}

	l = l - 2 ;
	for ( i = 1 ; i < l ; ++i )
	{
		if ( isdigit( msgid[ i ] ) && isdigit( msgid[ i + 1 ] ) && isdigit( msgid[ i + 2 ] ) )
		{
			return i ;
		}
	}
	return 0 ;
}


string pApplication::sub_vars( string s )
{
	// In string, s, substitute variables starting with '&' for their dialogue value
	// The variable is delimited by any non-valid variable characters.
	// Rules: A '.' at the end of a variable, concatinates the value with the string following the dot
	//        .. reduces to .
	//        && reduces to & with no variable substitution

	size_t p1 ;
	size_t p2 ;

	string var ;
	string val ;

	const string validChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#$@" ;
	p1 = 0 ;

	while ( true )
	{
		p1 = s.find( '&', p1 ) ;
		if ( p1 == string::npos || p1 == s.size() - 1 ) { break ; }
		++p1 ;
		if ( s[ p1 ] == '&' )
		{
			s.erase( p1, 1 ) ;
			p1 = s.find_first_not_of( '&', p1 ) ;
			continue ;
		}
		p2  = s.find_first_not_of( validChars, p1 ) ;
		if ( p2 == string::npos ) { p2 = s.size() ; }
		var = upper( s.substr( p1, p2-p1 ) ) ;
		if ( isvalidName( var ) )
		{
			val = "" ;
			vcopy( var, val, MOVE ) ;
			if ( RC <= 8 )
			{
				if ( p2 < s.size() && s[ p2 ] == '.' )
				{
					s.replace( p1-1, var.size()+2, val ) ;
				}
				else
				{
					s.replace( p1-1, var.size()+1, val ) ;
				}
				p1 = p1 + val.size() - 1 ;
			}
		}
	}
	RC = 0   ;
	return s ;
}


void pApplication::enq( const string& maj, const string& min, enqDISP disp, enqSCOPE scope )
{
	// RC =  0 Normal completion
	// RC =  8 Enqueue already held by this, or another task if exclusive requested
	// RC = 20 Severe error

	errBlock.setRC( 0 ) ;
	vector<enqueue>* p_enqueues = ( scope == GLOBAL ) ? &g_enqueues : &l_enqueues ;

	const string e1 = "ENQ Service Error" ;

	RC = 0 ;

	check_qrname( e1, maj, min ) ;
	if ( errBlock.error() ) { return ; }

	boost::lock_guard<boost::mutex> lock( mtx ) ;

	for ( auto it = p_enqueues->begin() ; it != p_enqueues->end() ; ++it )
	{
		if ( it->maj_name == maj && it->min_name == min )
		{
			if ( it->disp == SHR && disp == SHR && it->tasks.count( taskId ) == 0 )
			{
				it->tasks.insert( taskId ) ;
				return ;
			}
			RC = 8 ;
			return ;
		}
	}

	p_enqueues->push_back( enqueue( maj, min, taskId, disp ) ) ;
}


void pApplication::deq( const string& maj, const string& min, enqSCOPE scope )
{
	// RC =  0 Normal completion
	// RC =  8 Enqueue not held by this task
	// RC = 20 Severe error

	errBlock.setRC( 0 ) ;
	vector<enqueue>* p_enqueues = ( scope == GLOBAL ) ? &g_enqueues : &l_enqueues ;

	const string e1 = "DEQ Service Error" ;

	check_qrname( e1, maj, min ) ;
	if ( errBlock.error() ) { return ; }

	boost::lock_guard<boost::mutex> lock( mtx ) ;

	RC = 8 ;
	for ( auto it = p_enqueues->begin() ; it != p_enqueues->end() ; ++it )
	{
		if ( it->maj_name == maj && it->min_name == min )
		{
			if ( it->tasks.count( taskId ) > 0 )
			{
				it->tasks.erase( taskId ) ;
				if ( it->tasks.size() == 0 )
				{
					p_enqueues->erase( it ) ;
				}
				RC = 0 ;
			}
			break ;
		}
	}
}


void pApplication::qscan( const string& maj, const string& min, enqDISP disp, enqSCOPE scope )
{
	// Scan for an enqueue.  Match on qname, rname, disposition and scope

	// RC =  0 Enqueue held
	// RC =  8 Enqueue is not held
	// RC = 20 Severe error

	errBlock.setRC( 0 ) ;
	vector<enqueue>* p_enqueues = ( scope == GLOBAL ) ? &g_enqueues : &l_enqueues ;

	const string e1 = "QSCAN Service Error" ;

	RC = 8 ;

	check_qrname( e1, maj, min ) ;
	if ( errBlock.error() ) { return ; }

	boost::lock_guard<boost::mutex> lock( mtx ) ;

	for ( auto it = p_enqueues->begin() ; it != p_enqueues->end() ; ++it )
	{
		if ( it->maj_name == maj && it->min_name == min )
		{
			if ( it->disp == disp )
			{
				RC = 0 ;
			}
			return ;
		}
	}
}


void pApplication::check_qrname( const string& e1, const string& maj, const string& min )
{
	if ( !isvalidName( maj ) )
	{
		errBlock.setcall( e1, "PSYS013F" ) ;
		checkRCode( errBlock ) ;
	}
	else if ( min == "" )
	{
		errBlock.setcall( e1, "PSYS013G" ) ;
		checkRCode( errBlock ) ;
	}
}


void pApplication::show_enqueues()
{
	boost::lock_guard<boost::mutex> lock( mtx ) ;

	llog( "I", ".ENQ" << endl ) ;
	llog( "-", "*************************************************************************************************************" << endl ) ;
	llog( "-", "Global enqueue vector size is "<< g_enqueues.size() << endl ) ;
	llog( "-", "" << endl ) ;
	llog( "-", "Local enqueues held by task "<< d2ds( taskId, 8 ) << endl ) ;
	llog( "-", "Exc/Share  Major Name  Minor Name "<< endl ) ;
	llog( "-", "---------  ----------  ---------- "<< endl ) ;

	for ( auto it = l_enqueues.begin() ; it != l_enqueues.end() ; ++it )
	{
		llog( "-", "" << setw( 11 ) << std::left << ( it->disp == EXC ? "EXCLUSIVE" : "SHARE" )
			      << setw( 8 )  << std::left << it->maj_name << "    " << it->min_name << endl ) ;
	}

	llog( "-", "" << endl ) ;
	llog( "-", "Global enqueues held by task "<< d2ds( taskId, 8 ) << endl ) ;
	llog( "-", "Exc/Share  Major Name  Minor Name "<< endl ) ;
	llog( "-", "---------  ----------  ---------- "<< endl ) ;

	for ( auto it = g_enqueues.begin() ; it != g_enqueues.end() ; ++it )
	{
		if ( it->tasks.count( taskId ) == 0 ) { continue ; }
		llog( "-", "" << setw( 11 ) << std::left << ( it->disp == EXC ? "EXCLUSIVE" : "SHARE" )
			      << setw( 8 )  << std::left << it->maj_name << "    " << it->min_name << endl ) ;
	}

	llog( "-", ""<< endl ) ;
	llog( "-", "All global enqueues"<< endl ) ;
	llog( "-", "Task      Exc/Share  Major Name  Minor Name "<< endl ) ;
	llog( "-", "--------  ---------  ----------  ---------- "<< endl ) ;

	for ( auto it = g_enqueues.begin() ; it != g_enqueues.end() ; ++it )
	{
		for ( auto itt = it->tasks.begin() ; itt != it->tasks.end() ; ++itt )
		{
			llog( "-", "" << d2ds( *itt, 8 ) << "  "
				      << setw( 11 ) << std::left << ( it->disp == EXC ? "EXCLUSIVE" : "SHARE" )
				      << setw( 8 )  << it->maj_name << "    " << it->min_name << endl ) ;
		}
	}

	llog( "-", "*************************************************************************************************************" << endl ) ;
}


void pApplication::info()
{
	llog( "I", ".INFO" << endl ) ;
	llog( "-", "*************************************************************************************************************" << endl ) ;
	llog( "-", "Application Information for "<< zappname << endl ) ;
	llog( "-", "                   Task ID: "<< d2ds( taskId, 8 ) << endl ) ;
	llog( "-", "            Parent Task ID: "<< d2ds( ptid, 8 ) << endl ) ;
	llog( "-", "          Shared Pool Name: "<< d2ds( shrdPool, 8 ) << endl ) ;
	llog( "-", "         Profile Pool Name: "<< p_poolMGR->get( errBlock, "ZAPPLID", SHARED ) << endl ) ;
	llog( "-", " " << endl ) ;
	llog( "-", "Application Description . : "<< zappdesc << endl ) ;
	llog( "-", "Application Version . . . : "<< zappver  << endl ) ;
	llog( "-", "Current Application Status: "<< get_status() << endl ) ;
	llog( "-", "Last Panel Displayed. . . : "<< currPanel->panelid << endl ) ;
	llog( "-", "Last Message Displayed. . : "<< zmsgid << endl ) ;
	llog( "-", "Number of Panels Loaded . : "<< panelList.size() << endl )  ;
	if ( rexxName != "" )
	{
		llog( "-", "Application running REXX. : "<< rexxName << endl ) ;
	}
	llog( "-", " " << endl ) ;
	if ( testMode )
	{
		llog( "-", "Application running in test mode"<< endl ) ;
	}
	if ( passlib )
	{
		llog( "-", "Application started with PASSLIB option"<< endl ) ;
	}
	if ( newpool )
	{
		llog( "-", "Application started with NEWPOOL option"<< endl ) ;
	}
	llog( "-", "*************************************************************************************************************" << endl ) ;
}


string pApplication::get_status()
{
	string t ;

	if ( applicationEnded )
	{
		t = ( abnormalEnd ) ? "Abended" : "Ended" ;
	}
	else
	{
		switch ( waiting_on )
		{
		case WAIT_NONE:
			t = ( abnormalEnd ) ? "Abending" : "Running" ;
			break ;

		case WAIT_OUTPUT:
			t = "Waiting on output" ;
			break ;

		case WAIT_SELECT:
			t = "Waiting on SELECT" ;
			break ;

		case WAIT_USER:
			t = "Waiting on user" ;
			break ;
		}
	}

	return t ;
}


const string& pApplication::get_jobkey()
{
	// Create a string that uniquely identifies this job.
	// Of the form: yyyyddd-hhmmsstt-nnnnn
	// where nnnn is the taskid

	// Used for creating spool file names, etc.

	string ztimel ;
	string zj4date ;

	if ( zjobkey == "" )
	{
		ztimel  = startTime ;
		zj4date = startDate ;
		zj4date.erase( 4, 1 ) ;
		ztimel.erase( 8, 1 ) ;
		ztimel.erase( 5, 1 ) ;
		ztimel.erase( 2, 1 ) ;
		zjobkey = zj4date + "-" + ztimel + "-" + d2ds( taskid(), 5 ) ;
	}

	return zjobkey ;
}


void pApplication::loadCommandTable()
{
	// Load application command table in the application task so it can be unloaded on task termination.
	// This is done during SELECT processing so LIBDEFs must be active with PASSLIB specified,
	// if being used to find the table.

	p_tableMGR->loadTable( errBlock, get_applid() + "CMDS", NOWRITE, get_search_path( s_ZTLIB ), SHARE ) ;
	if ( errBlock.RC0() )
	{
		cmdTableLoaded = true ;
	}
}


void pApplication::ispexec( const string& s )
{
	ispexeci( this, s, errBlock ) ;
	if ( errBlock.error() )
	{
		errBlock.setcall( "ISPEXEC Interface Error" ) ;
		checkRCode( errBlock ) ;
		return ;
	}
	errBlock.clearsrc() ;
}


void pApplication::checkRCode()
{
	// If the error panel is to be displayed, cancel CONTROL DISPLAY LOCK and remove any popup's

	// If this is issued as a result of a service call (a call from another thread ie. lspf), just return.
	// The calling thread needs to check further for errors as there is not much that can be done here.

	int RC1 ;

	if ( errBlock.ServiceCall() )
	{
		errBlock.seterror() ;
		return ;
	}

	RC1 = RC ;

	if ( zerr1 != "" ) { llog( "E", zerr1 << endl ) ; }
	if ( zerr2 != "" ) { llog( "E", zerr2 << endl ) ; }

	if ( not ControlErrorsReturn && RC >= 12 )
	{
		llog( "E", "RC="<< RC <<" CONTROL ERRORS CANCEL is in effect.  Aborting"<< endl ) ;
		vreplace( "ZAPPNAME", zappname ) ;
		vreplace( "ZERRRX", rexxName ) ;
		vreplace( "ZERR1",  zerr1 ) ;
		vreplace( "ZERR2",  zerr2 ) ;
		vreplace( "ZERR3",  zerr3 ) ;
		vreplace( "ZERR4",  zerr4 ) ;
		vreplace( "ZERR5",  zerr5 ) ;
		vreplace( "ZERR6",  zerr6 ) ;
		vreplace( "ZERR7",  zerr7 ) ;
		vreplace( "ZERR8",  zerr8 ) ;
		vreplace( "ZERRRC", d2ds( RC1 ) ) ;
		ControlDisplayLock  = false ;
		ControlErrorsReturn = true  ;
		selPanel            = false ;
		if ( addpop_active ) { rempop( "ALL" ) ; }
		display( "PSYSER1" )  ;
		if ( RC <= 8 ) { errPanelissued = true ; }
		abend() ;
	}
}


void pApplication::checkRCode( errblock err )
{
	// If the error panel is to be displayed, cancel CONTROL DISPLAY LOCK/NONDISPL and remove any popup's

	// Format: msg1   header - call description resulting in the error
	//         short  msg
	//         longer description

	// Set RC to the error code in the error block if we are returning to the program (CONTROL ERRORS RETURN).

	// Terminate processing if this routing is called during error processing.

	// If this is issued as a result of a service call (a call from another thread ie. lspf), just return.
	// The calling thread needs to check further for errors as there is not much that can be done here.

	string t ;

	if ( err.ServiceCall() )
	{
		return ;
	}

	if ( err.abending() )
	{
		llog( "E", "Errors have occured during error processing.  Terminating application."<<endl ) ;
		llog( "E", "Error Appl : "<< zappname << endl )  ;
		llog( "E", "Error REXX : "<< rexxName << endl )  ;
		llog( "E", "Error msg  : "<< err.msg1 << endl )  ;
		llog( "E", "Error RC   : "<< err.getRC() << endl ) ;
		llog( "E", "Error id   : "<< err.msgid << endl ) ;
		llog( "E", "Error ZVAL1: "<< err.val1 << endl )  ;
		llog( "E", "Error ZVAL2: "<< err.val2 << endl )  ;
		llog( "E", "Error ZVAL3: "<< err.val3 << endl )  ;
		llog( "E", "Source     : "<< err.getsrc() << endl ) ;
		abend() ;
	}

	errBlock.setAbending() ;

	if ( err.val1 != "" ) { vreplace( "ZVAL1", err.val1 ) ; }
	if ( err.val2 != "" ) { vreplace( "ZVAL2", err.val2 ) ; }
	if ( err.val3 != "" ) { vreplace( "ZVAL3", err.val3 ) ; }

	getmsg( err.msgid, "ZERRSM", "ZERRLM" ) ;

	vreplace( "ZERRMSG", err.msgid )  ;
	vreplace( "ZERRRC", d2ds( err.getRC() ) ) ;

	if ( ControlErrorsReturn )
	{
		RC = err.getRC() ;
		errBlock = err ;
		return ;
	}

	llog( "E", err.msg1 << endl ) ;

	vcopy( "ZERRSM", t, MOVE ) ;
	if ( t != "" ) { llog( "E", t << endl ) ; }
	vcopy( "ZERRLM", t, MOVE ) ;
	if ( t != "" )
	{
		llog( "E", t << endl ) ;
		splitZerrlm( t ) ;
	}

	llog( "E", "RC="<< err.getRC() <<" CONTROL ERRORS CANCEL is in effect.  Aborting" << endl ) ;

	vreplace( "ZAPPNAME", zappname ) ;
	vreplace( "ZERRRX", rexxName )  ;
	vreplace( "ZERR1",  err.msg1  ) ;

	vreplace( "ZERR2", "" ) ;
	vreplace( "ZERR3", err.getsrc() ) ;

	if ( err.getsrc() != "" )
	{
		if ( err.dialogsrc() )
		{
			vreplace( "ZERR2", "Current dialogue statement:" ) ;
		}
		else
		{
			if ( err.panelsrc() )
			{
				vreplace( "ZERR2",  "Panel statement where error was detected:" ) ;
			}
			else
			{
				vreplace( "ZERR2",  "Line where error was detected:" ) ;
			}
		}
	}

	clr_nondispl() ;
	ControlDisplayLock  = false ;
	ControlErrorsReturn = true  ;
	selPanel            = false ;

	if ( addpop_active ) { rempop( "ALL" ) ; }
	errBlock.clear() ;

	display( "PSYSER2" ) ;
	if ( RC <= 8 ) { errPanelissued = true ; }
	abend() ;
}


void pApplication::splitZerrlm( string t )
{
	size_t i ;

	int l    ;
	int maxw ;

	vdefine( "ZSCRMAXW", &maxw ) ;
	vget( "ZSCRMAXW", SHARED ) ;
	vdelete( "ZSCRMAXW" ) ;

	maxw = maxw - 6 ;
	l    = 0        ;
	do
	{
		++l ;
		if ( t.size() > size_t( maxw ) )
		{
			i = t.find_last_of( ' ', maxw ) ;
			i = ( i == string::npos ) ? maxw : i + 1 ;
			vreplace( "ZERRLM"+ d2ds( l ), t.substr( 0, i ) ) ;
			t.erase( 0, i ) ;
		}
		else
		{
			vreplace( "ZERRLM"+d2ds( l ), t ) ;
			t = "" ;
		}
	} while ( t.size() > 0 ) ;

}


/* ************************************ ***************************** ********************************* */
/* ************************************ Abnormal Termination Routines ********************************* */
/* ************************************ ***************************** ********************************* */

void pApplication::cleanup_default()
{
	// Dummy routine.  Override in the application so the customised one is called on an exception condition.
	// Use CONTROL ABENDRTN ptr_to_routine to set

	// Called on: abend()
	//            abendexc()
	//            set_timeout_abend()
}


void pApplication::abend()
{
	abend_nothrow() ;
	throw( pApplication::xTerminate() ) ;
}


void pApplication::abend( const string& msgid )
{
	errBlock.setcall( msgid, msgid ) ;
	checkRCode( errBlock ) ;
}


void pApplication::abend( const string& msgid, const string& val1 )
{
	errBlock.setcall( msgid, msgid, val1 ) ;
	checkRCode( errBlock ) ;
}


void pApplication::abend_nothrow()
{
	llog( "E", "Shutting down application: "+ zappname +" Taskid: " << taskId << " due to an abnormal condition" << endl ) ;
	abnormalEnd   = true  ;
	terminateAppl = true  ;
	SEL           = false ;

	ControlErrorsReturn = true ;
	(this->*pcleanup)() ;
	abended = true ;
}


void pApplication::uabend()
{
	// Abend application with no messages.

	abnormalEnd   = true  ;
	abnormalNoMsg = true  ;
	terminateAppl = true  ;
	SEL           = false ;

	ControlErrorsReturn = true ;
	(this->*pcleanup)() ;
	abended = true ;
	throw( pApplication::xTerminate() ) ;
}


void pApplication::uabend( const string& msgid, int callno )
{
	// Abend application with error screen.
	// Screen will show short and long messages from msgid, and the return code RC
	// Optional variables can be coded after the message id and are placed in ZVAL1, ZVAL2 etc.

	vreplace( "ZERRRC", d2ds( RC ) ) ;
	xabend( msgid, callno ) ;
}


void pApplication::uabend( const string& msgid, const string& val1, int callno )
{
	vreplace( "ZERRRC", d2ds( RC ) ) ;
	vreplace( "ZVAL1", val1 ) ;
	xabend( msgid, callno )   ;
}


void pApplication::uabend( const string& msgid, const string& val1, const string& val2, int callno )
{
	vreplace( "ZERRRC", d2ds( RC ) ) ;
	vreplace( "ZVAL1", val1 ) ;
	vreplace( "ZVAL2", val2 ) ;
	xabend( msgid, callno )   ;
}


void pApplication::uabend( const string& msgid, const string& val1, const string& val2, const string& val3, int callno )
{
	vreplace( "ZERRRC", d2ds( RC ) ) ;
	vreplace( "ZVAL1", val1 ) ;
	vreplace( "ZVAL3", val2 ) ;
	vreplace( "ZVAL4", val3 ) ;
	xabend( msgid, callno )   ;
}


void pApplication::xabend( const string& msgid, int callno )
{
	string t ;

	getmsg( msgid, "ZERRSM", "ZERRLM" ) ;
	vcopy( "ZERRLM", t, MOVE ) ;
	splitZerrlm( t ) ;

	t = "A user abend has occured in application "+ zappname ;
	if ( callno != -1 ) { t += " at call number " + d2ds( callno ) ; }

	llog( "E", "Shutting down application: "+ zappname +" Taskid: "<< taskId <<" due to a user abend" << endl ) ;

	vreplace( "ZAPPNAME", zappname ) ;
	vreplace( "ZERRRX", rexxName ) ;
	vreplace( "ZERRMSG", msgid ) ;
	vreplace( "ZERR1",  t  ) ;
	vreplace( "ZERR2",  "" ) ;
	vreplace( "ZERR3",  "" ) ;
	ControlDisplayLock  = false ;
	ControlErrorsReturn = true  ;
	selPanel            = false ;

	display( "PSYSER2" ) ;
	if ( RC <= 8 ) { errPanelissued = true ; }

	abnormalEnd   = true  ;
	terminateAppl = true  ;
	SEL           = false ;

	ControlErrorsReturn = true ;
	(this->*pcleanup)() ;
	abended = true ;
	throw( pApplication::xTerminate() ) ;
}


void pApplication::abendexc()
{
	llog( "E", "An unhandled exception has occured in application: "+ zappname +" Taskid: " << taskId << endl ) ;
	if ( !abending )
	{
		ControlErrorsReturn = true ;
		abending = true     ;
		(this->*pcleanup)() ;
	}
	else
	{
		llog( "E", "An abend has occured during abend processing.  Cleanup will not be called" << endl ) ;
	}
	llog( "E", "Exception: " << boost::current_exception_diagnostic_information() << endl ) ;
	llog( "E", "Shutting down application: " << zappname << " Taskid: " << taskId << endl ) ;
	abnormalEnd   = true  ;
	terminateAppl = true  ;
	SEL           = false ;
	abended       = true  ;
}


void pApplication::set_forced_abend()
{
	llog( "E", "Shutting down application: "+ zappname +" Taskid: "<< taskId <<" due to a forced condition" << endl ) ;
	abnormalEnd       = true  ;
	abnormalEndForced = true  ;
	terminateAppl     = true  ;
	SEL               = false ;
	abended           = true  ;
}


void pApplication::set_timeout_abend()
{
	llog( "E", "Shutting down application: "+ zappname +" Taskid: "<< taskId <<" due to a timeout condition" << endl ) ;
	abnormalEnd       = true  ;
	abnormalEndForced = true  ;
	abnormalTimeout   = true  ;
	terminateAppl     = true  ;
	backgrd           = true  ;
	SEL               = false ;

	map<string, pPanel*>::iterator it;
	for ( it = panelList.begin() ; it != panelList.end() ; ++it )
	{
		delete it->second ;
	}
	panelList.clear() ;

	ControlErrorsReturn = true ;
	(this->*pcleanup)() ;
	abended  = true  ;
	busyAppl = false ;
}
