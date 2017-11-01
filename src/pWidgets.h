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

ofstream aplog(ALOG) ;


class dynArea
{
	public:
		dynArea(){
				dynArea_DataInsp  = false ;
				dynArea_DataOutsp = false ;
				dynArea_UserModsp = false ;
				dynArea_DataModsp = false ;
				dynArea_Field     = ""    ;
				dynArea_FieldIn   = ""    ;
			 }

	private:
		int    dynArea_row       ;
		int    dynArea_col       ;
		int    dynArea_width     ;
		int    dynArea_depth     ;
		bool   dynArea_DataInsp  ;
		bool   dynArea_DataOutsp ;
		bool   dynArea_UserModsp ;
		bool   dynArea_DataModsp ;
		char   dynArea_DataIn    ;
		char   dynArea_DataOut   ;
		char   dynArea_UserMod   ;
		char   dynArea_DataMod   ;
		string dynArea_Field     ;
		string dynArea_FieldIn   ;
		string dynArea_shadow_name ;

		void dynArea_init( errblock& err, int MAXW, int MAXD, const string& line ) ;
		void setsize( int, int, int, int ) ;

       friend class pPanel ;
       friend class field  ;
} ;


class field
{
	private:
		field() {
				field_pwd          = false ;
				field_changed      = false ;
				field_active       = true  ;
				field_usecua       = true  ;
				field_colour       = 0     ;
				field_skip         = true  ;
				field_caps         = false ;
				field_paduser      = false ;
				field_padchar      = ' '   ;
				field_just         = 'L'   ;
				field_numeric      = false ;
				field_input        = false ;
				field_dynArea      = NULL  ;
				field_tb           = false ;
				field_scrollable   = false ;
				field_scroll_start = 1     ;
				field_shadow_value = ""    ;
			} ;

		unsigned int field_row          ;
		unsigned int field_col          ;
		unsigned int field_cole         ;
		unsigned int field_length       ;
		string       field_value        ;
		bool         field_pwd          ;
		bool         field_changed      ;
		bool         field_active       ;
		cuaType      field_cua          ;
		bool         field_usecua       ;
		uint         field_colour       ;
		bool         field_skip         ;
		bool         field_caps         ;
		bool         field_paduser      ;
		char         field_padchar      ;
		char         field_just         ;
		bool         field_numeric      ;
		bool         field_input        ;
		dynArea *    field_dynArea      ;
		bool         field_tb           ;
		bool         field_scrollable   ;
		unsigned int field_scroll_start ;
		string       field_shadow_value ;

		void field_init( errblock& err, int MAXW, int MAXD, const string& line ) ;
		void field_opts( errblock& err, string& )  ;
		bool cursor_on_field( uint row, uint col ) ;
		void display_field( WINDOW *, char, bool ) ;
		bool edit_field_insert( WINDOW * win, char ch, int row, char, bool ) ;
		bool edit_field_replace( WINDOW * win, char ch, int row, char, bool ) ;
		void edit_field_delete( WINDOW * win, int row, char, bool ) ;
		int  edit_field_backspace( WINDOW * win, int col, char, bool ) ;
		void field_remove_nulls_da()     ;
		void field_blank( WINDOW * win, char ) ;
		void field_clear( WINDOW * win, char ) ;
		void field_erase_eof( WINDOW * win, unsigned int col, char, bool ) ;
		bool field_dyna_input( uint col )  ;
		int  field_dyna_input_offset( uint col )  ;
		void field_DataMod_to_UserMod( string *, int ) ;
		void field_attr( errblock& err, string attrs ) ;
		void field_attr() ;
		void field_prep_input()   ;
		void field_prep_display() ;
		void field_set_caps()     ;
		int  end_of_field( WINDOW * win, uint col )   ;

       friend class pPanel ;
} ;


class literal
{
	private:
		literal() {
				literal_colour = 0  ;
				literal_name   = "" ;
			  }
		int     literal_row    ;
		int     literal_col    ;
		int     literal_cole   ;
		cuaType literal_cua    ;
		uint    literal_colour ;
		string  literal_value  ;
		string  literal_name   ;

		void literal_init( errblock& err, int MAXW, int MAXD, int& opt_field, const string& line ) ;
		void literal_display( WINDOW *, const string& ) ;
		bool cursor_on_literal( uint row, uint col ) ;
       friend class pPanel ;
} ;


class pdc
{
	public:
		pdc()   {
				pdc_name    = "" ;
				pdc_run     = "" ;
				pdc_parm    = "" ;
				pdc_unavail = "" ;
			} ;
		~pdc() {} ;
		pdc( const string& a, const string& b, const string& c, const string& d )
			{
				pdc_name    = a ;
				pdc_run     = b ;
				pdc_parm    = c ;
				pdc_unavail = d ;
			} ;
		string pdc_name ;
		string pdc_run  ;
		string pdc_parm ;
		string pdc_unavail ;
} ;


class abc
{
	public:
		string       abc_name ;
		unsigned int abc_col  ;
		unsigned int abc_maxh ;
		unsigned int abc_maxw ;

		abc()   {
				abc_maxh   = 0 ;
				abc_maxw   = 0 ;
				pd_created = false ;
			} ;

		~abc()  {
			if ( pd_created )
			{
				del_panel( panel ) ;
				delwin( win )      ;
			}
			} ;

		bool pdc_exists( const string& )   ;
		void add_pdc( const pdc& ) ;
		void display_abc_sel( WINDOW * )   ;
		void display_abc_unsel( WINDOW * ) ;
		void display_pd( uint, uint ) ;
		void hide_pd() ;
		pdc  retrieve_pdChoice( unsigned int row, unsigned int col ) ;

	private:
		bool   pd_created ;
		vector<pdc> pdcList ;
		WINDOW * win   ;
		PANEL  * panel ;
} ;


class Box
{
	public:
		Box()   {
				box_title = "" ;
			}

		void box_init( errblock& err, int MAXW, int MAXD, const string& line ) ;
		void display_box( WINDOW * ) ;

	private:
		int    box_row    ;
		int    box_col    ;
		int    box_width  ;
		int    box_depth  ;
		uint   box_colour ;
		string box_title  ;
		int    box_title_offset ;
} ;

