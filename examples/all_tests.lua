api_version = "1.12.0.0"

ffi = require("ffi")
ffi.cdef[[
     //set of functions that do not operate using async
    void* get_mysql_con_wrapper(const char* server, const char* username, const char* password,const char* database);
    void mysql_query(void* con,const char* query,bool convert_query_encoding);
    char*** mysql_query_results(void* con,const char* query, bool convert_query_encoding, bool convert_result_encoding);
    void free_mysql_data_ptr(char*** arr);
    void delete_mysql_con(void* con);
         
    //set of functions for making async queries that require results
    void* mysql_query_results_async_ptr(const char* server, const char* username, const char* password, const char* database, const char* query, bool convert_query_encoding, bool convert_result_encoding);
    bool get_mysql_query_results_async_status(void* mysqlresultasync);
    char*** join_mysql_query_results_async(void* mysqlresultasync);
    void delete_mysql_query_results_async_ptr(void* mysqlresultasync);
    
    //set of functions for making async queries without needing results
    void* mysql_query_async_ptr(const char* server, const char* username, const char* password, const char* database, const char* query, bool convert_query_encoding);
    bool get_mysql_query_async_status(void* mysqlqueryasync);
    void join_mysql_query_async(void* mysqlqueryasync);
    void delete_mysql_query_async_ptr(void* mysqlqueryasync);
   
        
]]
sapp_mysql_client = ffi.load("sapp_mysql_client")
--[[
    dependencies
    Visual C++ 2015 Redistributable x86 (Later versions may also be required)
        On linux: winetricks vcrun2015
        On windows google: Visual C++ 2015 Redistributable x86

    https://dev.mysql.com/downloads/connector/cpp/
    Select operating system: Microsoft Windows
    Select OS Version: X86 32-bit
    Download the non-debug zip
        libcrypto-1_1.dll
        libssl-1_1.dll

    https://gnuwin32.sourceforge.net/downlinks/libiconv-bin-zip.php
        libiconv2.dll

]]--
result_ptr_table={}
query_ptr_table={}
mysql_host="tcp://localhost:3306"
mysql_username="root"
mysql_password="12345"
mysql_database="sapp_mysql_test"
--CREATE TABLE test_table (id int auto_increment,test_string varchar(1000),test_int int,primary key(id));
function OnScriptLoad()
        --
        --- CONVERT ENCODING INFO: ---
        --[[
            Basically the strings that you get from halo text sources like chat messages and player names are encoded as a byte 0-255, any characters past byte 127 is not ascii
            The mysql c++ connector can handle ascii encoded strings, but as soon as there is a special character it cannot perform the query (I still dont know exactly why)
            Changing the destination database or table character encoding seems to not have any effect and the error persists
            
            The solution is to use iconv and offer a bool on the query C functions that auto converts query strings from WINDOWS-1252 to UTF-8 (bool convert_query_encoding)
            If you want UTF-8 database results that are returned to be auto converted from UTF-8 to WINDOWS-1252 you can set the (bool convert_result_encoding) to true
    
            These auto conversions will only work (to and from) or (from and to) both WINDOWS-1252 and UTF-8
        ]]--
        --
    

   
        --not async implementation
        local con_ptr = sapp_mysql_client.get_mysql_con_wrapper(mysql_host,mysql_username,mysql_password,mysql_database)
        if con_ptr~=nil then  --script can crash if this check is not made (will be nil when there is a connection error)
            sapp_mysql_client.mysql_query(con_ptr,'insert into test_table (test_string,test_int) values ("mysql from sapp",1);',false)
            
            result=mysql_query_results(con_ptr,'select * from test_table;',false,false) --refer to function below
            
            if (result~=nil) then
                for k,v in pairs(result) do
                    --print(k)
                    string=''
                    for kk,vv in pairs(v) do
                        
                        string=string..vv..','
                        
                    end
                    print(string)
                end
            else
                --result is nil means 0 rows, or there was an error
            end
            
            
            --multiple queries and queries with results can be made using the same con_ptr until it's deleted
            sapp_mysql_client.delete_mysql_con(con_ptr) --avoid memory leaks
            
        end
        
        
        
        
    
        --async implementation
        local class_result_ptr=sapp_mysql_client.mysql_query_results_async_ptr(mysql_host,mysql_username,mysql_password,mysql_database,"select * from test_table",true,true)
        if (class_result_ptr~=nil) then  --make sure the pointer is not nil
            table.insert(result_ptr_table,class_result_ptr); --create a structure to ogranize all the class pointers
        end
        
        local class_query_ptr=sapp_mysql_client.mysql_query_async_ptr(mysql_host,mysql_username,mysql_password,mysql_database,'insert into test_table (test_string,test_int) values ("mysql from sapp",1);',true)
        if (class_query_ptr~=nil) then  --make sure the pointer is not nil
            table.insert(query_ptr_table,class_query_ptr); --create a structure to ogranize all the class pointers
        end
        
        --if you were to create a loop like this using the async implementation, the queries would not necessarily be done in the order of the for loop, you would need to use sapp_mysql_client.get_mysql_query_results_async_status(class_ptr)==true to know when one has finished and then make the next one if you wanted a specific order (this goes for async queries that require results as well)
        --[[
        for i=1,10,1 do
            local class_query_ptr=sapp_mysql_client.mysql_query_async_ptr("tcp://localhost:3306","mouse","123","sapp_mysql_test",'insert into test_table (test_string,test_int) values ("mysql from sapp",'..i..');',true)
            if (class_query_ptr~=nil) then  --make sure the pointer is not nil
                table.insert(query_ptr_table,class_query_ptr); --create a structure to ogranize all the class pointers
            end
        end
        ]]--
        
    
        print('waiting...')
        --these timer functions work in conjunction with the async calls
        timer(250,"check_result_ptr") --timer loop to check on the status of the execution
        timer(250,"check_query_ptr") --timer loop to check on the status of the execution
        
   
end


function check_result_ptr()
    
    if (result_ptr_table[1]~=nil) then --if there is a pointer in the first position of the table
        if (sapp_mysql_client.get_mysql_query_results_async_status(result_ptr_table[1])==true) then --if that particular class has finished execution of do_async_work()
            print('async done')
            local data_ptr = sapp_mysql_client.join_mysql_query_results_async(result_ptr_table[1])
            if (data_ptr~=nil) then  --will be nil if some error occured or if there are 0 rows
                local result = {}
                local i = 0
                while data_ptr[i] ~= nil do --these while loops check the data from C for nullptr
                    --print('nonnil')
                    local row = {}
                    local j = 0
                    while data_ptr[i][j] ~= nil do --these while loops check the data from C for nullptr
                            table.insert(row, ffi.string(data_ptr[i][j]))  --no matter what type is in the mysql table, it's always converted to a string, use tonumber() for int, float etc... "" here is NULL in the table
                        j = j + 1
                    end
                    table.insert(result, row)
                    i = i + 1
                end
                
                --do stuff with the result
                    for k,v in pairs(result) do
                        --print(k)
                        string=''
                        for kk,vv in pairs(v) do
                            
                            string=string..vv..','
                            
                        end
                        print(string)
                    end
                --
            end

            sapp_mysql_client.delete_mysql_query_results_async_ptr(result_ptr_table[1]) --always call this when completed to delete the instance of the class to avoid memory leaks
            table.remove(result_ptr_table,1) --shift off element from the beginning of the lua table
            
            
            
        else
            print('waiting for async to complete result')
        end
    end
    
 
    timer(250,"check_result_ptr") --timer loop always runs in the background (you can create a better solution to manage this interaction)
end

function check_query_ptr()
     if (query_ptr_table[1]~=nil) then --if there is a pointer in the first position of the table
        if (sapp_mysql_client.get_mysql_query_async_status(query_ptr_table[1])==true) then --if that particular class has finished execution of do_async_work()
            sapp_mysql_client.join_mysql_query_async(query_ptr_table[1])
            
            print('query joined')
            sapp_mysql_client.delete_mysql_query_async_ptr(query_ptr_table[1]) --always call this when completed to delete the instance of the class to avoid memory leaks
            table.remove(query_ptr_table,1) --shift off element from the beginning of the lua table
            
        else
            print('waiting for async to complete query')
        end
    end
    timer(250,"check_query_ptr") --timer loop always runs in the background (you can create a better solution to manage this interaction)
end


function mysql_query_results(con_ptr,query,convert_query_encoding,convert_result_encoding) --for use with non async functions
    local data_ptr=sapp_mysql_client.mysql_query_results(con_ptr,query,convert_query_encoding,convert_result_encoding)
    if (data_ptr~=nil) then  --will be nil on error or 0 rows
        local result = {}
        local i = 0
        while data_ptr[i] ~= nil do --these while loops check the data from C for nullptr
            --print('nonnil')
            local row = {}
            local j = 0
            while data_ptr[i][j] ~= nil do --these while loops check the data from C for nullptr
                    table.insert(row, ffi.string(data_ptr[i][j]))  --no matter what type is in the mysql table, it's always converted to a string, use tonumber() for int, float etc... NULL will be empty string "" 
                j = j + 1
            end
            table.insert(result, row)
            i = i + 1
        end
        
        sapp_mysql_client.free_mysql_data_ptr(data_ptr) --needs to be called after sapp_mysql_client.mysql_query_results(con,query,convert_query_encoding,convert_result_encoding) with the data_ptr or memory will leak (if data_ptr is not nil)
        return result
    else
        print('data_ptr nil')
        return nil
    end
end

