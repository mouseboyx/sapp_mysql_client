# sapp_mysql_client
Mysql client ffi library for the sapp plugin for Halo CE servers

The dll works but it's still considered a development release

## Dependencies
Visual C++ 2015 Redistributable x86 (Later versions may also be required)

On linux: winetricks vcrun2015

On windows google: Visual C++ 2015 Redistributable x86

[https://dev.mysql.com/downloads/connector/cpp/](https://dev.mysql.com/downloads/connector/cpp/)

Select operating system: Microsoft Windows

Select OS Version: X86 32-bit

Download the non-debug zip

libcrypto-1_1.dll

libssl-1_1.dll

[https://gnuwin32.sourceforge.net/downlinks/libiconv-bin-zip.php](https://gnuwin32.sourceforge.net/downlinks/libiconv-bin-zip.php)

libiconv2.dll

## Encoding Conversion
Basically the strings that you get from halo text sources like chat messages and player names are encoded as a byte 0-255, any characters past byte 127 is not ascii
The mysql c++ connector can handle ascii encoded strings, but as soon as there is a special character it cannot perform the query (I still dont know exactly why)
Changing the destination database or table character encoding seems to not have any effect and the error persists

The solution is to use iconv and offer a bool on the query C functions that auto converts query strings from WINDOWS-1252 to UTF-8 (bool convert_query_encoding)
If you want UTF-8 database results that are returned to be auto converted from UTF-8 to WINDOWS-1252 you can set the (bool convert_result_encoding) to true

These auto conversions will only work (to and from) or (from and to) both WINDOWS-1252 and UTF-8

## Multi-queries
All of the functions can do multi queries, but the functions that return results don't currently have the capability to return results from multiple queries. I wouldn't perform multi-queries unless you understand what happens in the dll when you do. 

Multi-queries would be like sending a single string "select * from foo;insert into bar (column_name) values ('value');select last_insert_id();" in one function call to the dll.
