# generate_msql_tables.sql -- Create necessary tables for Euracom(mysql)
# generate_msql_tables.sql,v 1.2 1998/05/22 07:07:32 bus Exp

# Create table containing AVON

CREATE TABLE avon (
	nummer char( 10 ),		 
	name   char( 30 )
);

# Fill AVON table with some values

INSERT INTO avon ( nummer, name ) VALUES ( '+492364', 'Haltern (Westf.)' );
INSERT INTO avon ( nummer, name ) VALUES ( '+49228', 'Bonn' );

# Now create a table with "Well Known Numbers"

CREATE TABLE wkn (
	nummer char( 20 ),
	name   char( 40 )
);

# Again, fill with some example values

INSERT INTO wkn ( nummer, name) VALUES ( '+492364108537', 'Michael Bussmann' );
INSERT INTO wkn ( nummer, name) VALUES ( '+4923644095',   'Michael Bussmann; Telefax' );

# Table for charge data

CREATE TABLE euracom (
	int_no    int,		
	remote_no char( 20 ),
	vst_date  date,
        vst_time  time,
	sys_date  date,
        sys_time  time,
	einheiten int,	
	direction char( 1 ),
	length    int,
	pay       decimal(8,2),
	currency  char( 4 )
);

# Please notice there is no need to define an index on
# avon or wkn, because prefix_match won't use an index
 
# An index for vst_date or sys_date on table euracom might
# be useful, though

