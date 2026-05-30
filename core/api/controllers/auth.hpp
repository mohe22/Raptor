
#pragma once
#include <drogon/HttpController.h>

using namespace drogon;

class Auth : public HttpController<Auth>
{
public:
    METHOD_LIST_BEGIN

    METHOD_ADD(Auth::me, "/me", Get, "AuthFilter");
    METHOD_ADD(Auth::login, "/login", Post);
    METHOD_LIST_END
    void me(const HttpRequestPtr &req,
            std::function<void(const HttpResponsePtr &)> &&callback);
    void login(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);
};
