/*
 *    Copyright 2011 Trinity College Dublin
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/* SQL script to create the  triggers for the bundles table that  */
/* automatically insert a row into bundles_aux when one is        */
/* inserted into bundles using the same key, and delete the       */
/* matching row in bundles_aux when a row in bundles in deleted.  */
/* The row is initially inserted with all NULL data.  An update   */
/* is then applied to insert the bits of the bundle info that are */
/* selected to go into the aux table. The update would be applied */
/* via ODBC from DTN2.                                            */
/*                                                                */
/* The triggers effectively ensure that the database remains      */
/* consistent with a corresponding row in bundles_aux with the    */
/* same key as each row in bundles.                               */
/*                                                                */
/* This script is fed into mysql/sqlite3 after the bundles table  */
/* is created by dtnd --init-db.  This is done by specifying the  */
/* file as the SchemaPostCreationFile in the correct DSN entry in */
/* odbc.ini.  Note that the user who performs this operation      */
/* has to have the SUPER privilege.  This is another of the       */
/* privileges that has to be given specifically - GRANT ALL isn't */
/* enough.  Need to use                                           */
/* GRANT SUPER ON *.* TO <user>@<host>;                           */
/* If you are using MySQL 5.1.6 or later then you can also use    */
/* the TRIGGER privilege which (I think) comes with GRANT ALL.    */
/*                                                                */
/* The format of the file is a hybrid that has to be edited for   */
/* submission to either mysql or sqlite3 command line client.     */
/* For mysql the comment lines starting with '--' have to be      */
/* uncommented by piping through "tr -d'-'".  This activates the  */
/* temporary change of delimiter needed to handle definition of   */
/* triggers and procedures that internally use the normal SQL     */
/* delimiter ';'. This is not needed for sqlite3 because it has a */
/* more sophisticated parser that recognises when it is inside    */
/* this sort of CREATE operation.                                 */
/* For sqlite3 the backslashes have to be removed (doh!) by       */
/* by piping through "tr -d '\\'". Unlike mysql, sqlite3 treats   */
/* newlines as white space and doesn't need them escaped.         */

/* Discard previous versions if they exist */
DROP TRIGGER IF EXISTS bundle_insert_trigger;
DROP TRIGGER IF EXISTS bundle_delete_trigger;

/* Create the triggers.  Note that because you need statement    */
/* delimiters inside the trigger definitions, the delimiter at   */
/* outer level is changed to | temporarily.                      */
--DELIMITER |
CREATE TRIGGER bundle_insert_trigger AFTER INSERT ON bundles \
	FOR EACH ROW BEGIN \
		INSERT INTO bundles_aux (the_key) VALUES (NEW.the_key); \
	END; \
--|

CREATE TRIGGER bundle_delete_trigger BEFORE DELETE ON bundles \
	FOR EACH ROW BEGIN \
		INSERT INTO bundles_del (the_key, bundle_id) VALUES (OLD.the_key, \
			( SELECT bundle_id FROM bundles_aux WHERE the_key = OLD.the_key )); \
		DELETE FROM bundles_aux WHERE the_key = OLD.the_key; \
	END; \
--|

--DELIMITER ;
