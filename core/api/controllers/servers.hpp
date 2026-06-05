#pragma once
#include <drogon/HttpController.h>
#include <drogon/drogon.h>
#include <string>
using namespace drogon;
class Server : public drogon::HttpController<Server>
{
  public:
    METHOD_LIST_BEGIN
    METHOD_ADD(Server::getServers,
               "/get-servers",
               Get,"AuthFilter");
    METHOD_ADD(Server::getServerById,
               "/get-server/{1}",
               Get,"AuthFilter");
    METHOD_ADD(Server::pauseServer,
               "/pause/{1}",
               Post,
               "AuthFilter");
    METHOD_ADD(Server::resumeServer,
               "/resume/{1}",
               Post,
               "AuthFilter");
    METHOD_ADD(Server::stopServer,
               "/stop/{1}",
               Post,
               "AuthFilter");
    METHOD_ADD(Server::getPoolStatus,
                  "/pool-status",
                  Get);
    METHOD_ADD(Server::createServer,
        "/create",
               Post,
               "AuthFilter");
    METHOD_ADD(Server::updateServer,
        "/update",
               Post,
               "AuthFilter");
    METHOD_LIST_END
    void getPoolStatus(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);
    void getServers(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback);
    void getServerById(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,const std::string&);
    void pauseServer(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const std::string& serverName);
    void resumeServer(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const std::string& serverName);
    void stopServer(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    const std::string& serverName);
    void updateServer(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);
    void createServer(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);

};
