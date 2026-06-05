
#include "core/session/manager.hpp"
#include "core/context.hpp"
#include "core/session/base.hpp"
#include "type.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

namespace Raptor::Core::Server {

SessionManager::SessionManager() {
    sessions_.reserve(Core::Config::MAX_CONNECTIONS);
    startMonitor();
}

SessionManager::~SessionManager() {
    stopMonitor();
}


bool SessionManager::remove(const uint64_t id) {
    bool removed = false;
    std::string serverId;
    {
        std::unique_lock lock(mutex_);
        auto it = sessions_.find(id);
        if (it != sessions_.end()) {
            serverId = it->second->connectedTo();
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
        data.emplace_back(BriefSessionInfo{
            id,
            session->type(),
            session->status(),
            session->idleSeconds(),
            session->getAddressStr(),
            reg.hostname,
            reg.username,
            reg.os,
            reg.timezone
        });
    }
    return data;
}

void SessionManager::startMonitor() {
    running_ = true;
    thread_  = std::thread([this] { monitorLoop(); });
}

void SessionManager::stopMonitor() {
    running_ = false;
    cv_.notify_one();
    if (thread_.joinable())
        thread_.join();
}


void SessionManager::monitorLoop() {
    while (running_) {
        std::vector<uint64_t> toRemove;
        {
            std::shared_lock lock(mutex_);
            for (const auto& [id, session] : sessions_) {
                const auto  idle     = session->idleSeconds();
                const auto& serverId = session->connectedTo();

                if (session->isDisconnected()) {
                    Context::get().logs().info(Db::LogCategory::Session, "SESSION_EXPIRED",
                        std::format("id={} addr={} proto={} uptime={}s",
                            id, session->getAddressStr(),
                            Common::Types::ToString(session->type()),
                            session->uptimeSeconds()),
                        "", serverId);
                    toRemove.push_back(id);
                    continue;
                }

                if (idle >= Config::TIMEOUT_SECONDS) {
                    Context::get().logs().warn(Db::LogCategory::Session, "SESSION_TIMEOUT",
                        std::format("id={} addr={} proto={} idle={}s timeout={}s",
                            id, session->getAddressStr(),
                            Common::Types::ToString(session->type()),
                            idle, Config::TIMEOUT_SECONDS),
                        "", serverId);
                    session->setStatus(Session::Status::Disconnected);
                    toRemove.push_back(id);
                    continue;
                }

                if (idle >= Config::IDLE_SECONDS && session->isConnected()) {
                    Context::get().logs().debug(Db::LogCategory::Session, "SESSION_IDLE",
                        std::format("id={} addr={} proto={} idle={}s",
                            id, session->getAddressStr(),
                            Common::Types::ToString(session->type()),
                            idle),
                        "", serverId);
                    session->setStatus(Session::Status::Idle);
                }
            }
        }

        if (!toRemove.empty()) {
            std::unique_lock lock(mutex_);
            for (const auto id : toRemove)
                sessions_.erase(id);
        }

        std::shared_lock lock(mutex_);
        cv_.wait_for(lock, std::chrono::minutes(1), [this] { return !running_; });
    }
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


    SessionsDetailsList SessionManager::getSessionsDetails() const noexcept{
        std::shared_lock lock(mutex_);
        SessionsDetailsList list;
        list.reserve(sessions_.size());
        for (const auto& [id, base] : sessions_) {
            SessionDetails details;
            const auto& reg = base->getRegistrationInfo();
            details.id = base->id();
            details.protocol= base->type();
            details.status= base->status();
            details.idleSeconds= base->idleSeconds();
            details.connectedAt= base->connectedAt();
            details.remoteAddress   = base->getAddressStr();
            details.hostname= reg.hostname;
            details.username= reg.username;
            details.shell= reg.shell;
            details.homeDir= reg.homeDir;
            details.isAdmin= reg.isAdmin;
            details.isSudoer = reg.isSudoer;
            details.isDocker= reg.isDocker;
            details.isVm = reg.isVM;
            details.isDomainJoined  = reg.isDomainJoined;
            details.os= reg.os;
            details.arch= reg.arch;
            details.pid = reg.pid;
            details.processPath= reg.processPath;
            details.processName= reg.processName;
            details.timezone= reg.timezone;
            details.locale= reg.locale;
            details.domain = reg.locale;
            details.internalIp= reg.internalIp;
            details.macAddress= reg.macAddress;
            details.dns= reg.dnsServer;
            list.push_back(std::move(details));
        }
        return list;
    }
} // namespace Raptor::Core::Server
