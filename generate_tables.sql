-- generate-tables.sql -- Create necessary tables for Euracom (PostgreSQL)
-- $Id: generate_tables.sql,v 1.3 1998/03/14 12:37:08 bus Exp $

--
-- Create table containing AVON
--
CREATE TABLE avon (
	nummer text,		-- e.g. "+492364"
	name text		-- maps to "Haltern (Westf.)
);

--
-- Fill AVON table with some values
--
INSERT INTO avon (nummer,name) VALUES ('+492364', 'Haltern (Westf.)');
INSERT INTO avon (nummer,name) VALUES ('+49228', 'Bonn');

--
-- Now create a table with "Well Known Numbers"
--
CREATE TABLE wkn (
	nummer text,		-- same syntax as above
	name text		-- dto.
);

--
-- Again, fill with some example values
--
INSERT INTO wkn (nummer,name) VALUES ('+492364108537', 'Michael Bussmann');
INSERT INTO wkn (nummer,name) VALUES ('+4923644095', 'Michael Bussmann; Telefax');

--
-- Table for charge data
--
CREATE TABLE euracom (
	int_no int2,		-- Internal number
	remote_no text,		-- Remote number (intl. code)
	vst_date datetime,	-- Date/Time from Euracom 
				-- could be wrong due to a firmware bug (1.11)
	sys_date datetime,	-- Date/Time when entry gets inserted by the
				-- program.  Make sure your clock is good.
	einheiten int2,		-- Number of units for this call
	direction char,		-- 'I` for incoming, 'O' for outgoing calls
	length int4,		-- Length of call in seconds (not supported yet)
	factor float4,		-- Cost for 1 unit
	pay float4,		-- Total cost (should be equal to einheiten*factor)
	currency char(4)	-- Name of currency.  Probably 'DEM' or 'EURO'
);

--
-- Please notice there is no need to define an index on
-- avon or wkn, because prefix_match won't use an index
-- 
-- An index for vst_date or sys_date on table euracom might
-- be useful, though
--
