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

/* SQL script to create the auxiliary bundles tables: bundles_aux */
/*                                                                */
/* This script needs to be fed into mysql/sqlite3 before the      */
/* bundles tables is created by dtnd --init-db.  This is done by  */
/* specifying the file as the SchemaPreCreationFile in the        */
/* correct DSN entry in odbc.ini.  Note that the user who         */
/* runs this operation has to have the correct privilege.         */
/*                                                                */
/* The format of the file is correct for direct use with mysql.   */
/* (but see create_trigger.sql in case you need to use the        */
/* DELIMITER capability in mysql).                                */
/* For sqlite3 the backslashes have to be removed (doh!) by       */
/* by piping through "tr -d '\\'". Unlike mysql, sqlite3 treats   */
/* newlines as white space and doesn't need them escaped.         */

/* Discard previous version if it exists */
DROP TABLE IF EXISTS bundles_aux;

/* Create the auxiliary table */
CREATE TABLE bundles_aux \
	( the_key INTEGER UNSIGNED NOT NULL, \
	  bundle_id INTEGER UNSIGNED DEFAULT NULL, \
	  source_eid VARCHAR(2000), \
	  dest_eid VARCHAR(2000), \
	  payload_file VARCHAR(2000), \
	  bpq_data VARCHAR(2000), \
	  PRIMARY KEY (the_key));

