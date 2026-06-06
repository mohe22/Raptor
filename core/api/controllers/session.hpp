
#pragma once
#include <drogon/HttpController.h>
#include <drogon/drogon.h>
using namespace drogon;
class Sessions : public drogon::HttpController<Sessions>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(Sessions::getSessionForServer,"/get-sessions/{1}",Get,"AuthFilter");
    METHOD_ADD(Sessions::getSessionById,"/get-session/{1}/{2}",Get,"AuthFilter");

    METHOD_LIST_END
    void getSessionForServer(const HttpRequestPtr& req,std::function<void(const HttpResponsePtr&)>&& callback, const std::string& serverName);
    void getSessionById(const HttpRequestPtr& req,std::function<void(const HttpResponsePtr&)>&& callback, const std::string& serverName,const std::string&);

};
