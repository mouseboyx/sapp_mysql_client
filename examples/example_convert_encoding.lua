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

function OnScriptLoad()
        --CREATE TABLE chat_history (id bigint auto_increment,name varchar(20),message varchar(128),type int,player_index int,time timestamp,primary key(id));
        --the max length of a chat message that a client can send is 128, but I don't know if this can be exploited to send more
    
        register_callback(cb['EVENT_CHAT'], 'OnChat')
        
   
end

function OnChat(PlayerIndex,Message,Type)
    print(get_var(PlayerIndex,"$name")..','..PlayerIndex..','..Message..','..Type..','..string.len(Message))
    
        local con_ptr = sapp_mysql_client.get_mysql_con_wrapper("tcp://localhost:3306","root","12345","sapp_mysql_test")
        if con_ptr~=nil then  --script can crash if this check is not made (will be nil when there is a connection error)
            
            local query="insert into chat_history (name,message,type,player_index,time) values ("..insecure_sanitize_input(get_var(PlayerIndex,"$name"))..","..insecure_sanitize_input(Message)..","..Type..","..PlayerIndex..",CURRENT_TIMESTAMP);"
            print(query)
            sapp_mysql_client.mysql_query(con_ptr,query,true)  --we want to convert the encoding of this query because the dll can't perform queries with non-ascii characters unless they are converted to utf8
            
            
            sapp_mysql_client.delete_mysql_con(con_ptr) --avoid memory leaks
            
        end
    
end

function insecure_sanitize_input(input)  --do not rely on this for security purposes
    -- escape single quotes and backslashes
    input = string.gsub(input, "['\\]", "\\%0")
    -- wrap in single quotes
    input = "'" .. input .. "'"
    return input
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





