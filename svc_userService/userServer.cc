#include <bite_scaffold/log.h>
#include "userServer.h"


namespace userService {

UserServer::UserServer(std::shared_ptr<UserServiceImpl> serviceImpl, 
                       std::shared_ptr<brpc::Server> server, 
                       std::shared_ptr<bitesvc::SvcWatcher> serviceWatcher,
                       std::shared_ptr<bitesvc::SvcProvider> serviceProvider)
    : _serviceImpl(serviceImpl)
    , _server(server)
    , _serviceWatcher(serviceWatcher)
    , _serviceProvider(serviceProvider) {
    INF("UserServer initialized");
}

UserServer::~UserServer() {
    INF("UserServer destroyed");
}

void UserServer::start() {
    _server->RunUntilAskedToQuit();
}

} // namespace userService