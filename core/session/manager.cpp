
#include "core/session/manager.hpp"
#include "core/context.hpp"
#include "core/session/base.hpp"
#include "type.hpp"

namespace Raptor::Core::Server {

SessionManager::SessionManager() {
    sessions_.reserve(Core::Config::MAX_CONNECTIONS);
}


bool SessionManager::remove(const uint64_t id) {
    bool removed = false;
    std::string serverId;
    {
        std::unique_lock lock(mutex_);
        auto it = sessions_.find(id);
        if (it != sessions_.end()) {
            sessions_.erase(it);
            removed = true;
        }
    }
    if (removed)
        Context::get().logs().info(Db::LogCategory::Session, "SESSION_REMOVED",
            std::format("id={} removed", id),
            "", serverId);
    else
        Context::get().logs().warn(Db::LogCategory::Session, "SESSION_REMOVE_FAILED",
            std::format("id={} not found", id));
    return removed;
}



void SessionManager::clear() {
    size_t count = 0;
    {
        std::unique_lock lock(mutex_);
        count = sessions_.size();
        sessions_.clear();
    }
    Context::get().logs().info(Db::LogCategory::Session, "SESSION_CLEAR",
        std::format("{} sessions cleared", count));
}

Session::Base* SessionManager::find(const uint64_t id) const {

    std::shared_lock lock(mutex_);
    auto it = sessions_.find(id);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

bool SessionManager::contains(const uint64_t id) const {
    std::shared_lock lock(mutex_);
    return sessions_.contains(id);
}

size_t SessionManager::count() const noexcept {
    std::shared_lock lock(mutex_);
    return sessions_.size();
}

bool SessionManager::empty() const noexcept {
    std::shared_lock lock(mutex_);
    return sessions_.empty();
}

SessionsInfoList SessionManager::getSessionsInfo() const {
    std::shared_lock lock(mutex_);
    SessionsInfoList data;
    data.reserve(sessions_.size());
    for (const auto& [id, session] : sessions_) {
        const auto& reg = session->getRegistrationInfo();
        data.emplace_back(
            id,
            session->type(),
            session->status(),
            session->idleSeconds(),
            session->getAddressStr(),
            reg.hostname,
            reg.username,
            reg.os,
            reg.timezone
        );
    }
    return data;
}






void SessionManager::onSessionCreated(const uint64_t id, const std::string& address,
    const Common::Types::ServerType type,
    const std::string& serverId) noexcept {
        Context::get().logs().info(Db::LogCategory::Session, "SESSION_CREATED",
            std::format("id={} addr={} protocol={}", id, address, Common::Types::ToString(type)),
            "", serverId);
    }

    void SessionManager::onSessionCreateFailed(const std::string& serverId) noexcept {
        Context::get().logs().warn(Db::LogCategory::Session, "SESSION_CREATE_FAILED",
            "invalid address", "", serverId);
    }


    SessionsDetailsList SessionManager::getSessionsDetails() const noexcept {
        std::shared_lock lock(mutex_);
        SessionsDetailsList list;
        list.reserve(sessions_.size());
        for (const auto& [id, base] : sessions_) {
            const auto& reg = base->getRegistrationInfo();
            list.emplace_back(
                base->id(),
                base->type(),
                base->status(),
                base->idleSeconds(),
                base->connectedAtUnix(),
                base->getAddressStr(),
                reg.hostname,
                reg.username,
                reg.shell,
                reg.homeDir,
                reg.isAdmin,
                reg.isSudoer,
                reg.isDocker,
                reg.isVM,
                reg.isDomainJoined,
                reg.os,
                reg.arch,
                reg.pid,
                reg.processPath,
                reg.processName,
                reg.timezone,
                reg.locale,
                reg.domain,
                reg.internalIp,
                reg.macAddress,
                reg.dnsServer
            );
        }
        return list;
    }

} // namespace Raptor::Core::Server
