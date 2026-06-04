
#include "core/session/manager.hpp"
#include "core/context.hpp"
#include "type.hpp"

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
    {
        std::unique_lock lock(mutex_);
        removed = sessions_.erase(id) > 0;
    }
    if (removed)
        Context::get().logs().info(Db::LogCategory::Session, "SESSION_REMOVED",
            std::format("id={} removed", id));
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
                const auto idle = session->idleSeconds();

                if (session->isDisconnected()) {
                    Context::get().logs().info(Db::LogCategory::Session, "SESSION_EXPIRED",
                        std::format("id={} addr={} proto={} uptime={}s",
                            id, session->getAddressStr(),
                            Common::Types::ToString(session->type()),
                            session->uptimeSeconds()));
                    toRemove.push_back(id);
                    continue;
                }

                // idle too long  mark disconnected and queue for removal
                if (idle >= Config::TIMEOUT_SECONDS) {
                    Context::get().logs().warn(Db::LogCategory::Session, "SESSION_TIMEOUT",
                        std::format("id={} addr={} proto={} idle={}s timeout={}s",
                            id, session->getAddressStr(),
                            Common::Types::ToString(session->type()),
                            idle, Config::TIMEOUT_SECONDS));
                    session->setStatus(Session::Status::Disconnected);
                    toRemove.push_back(id);
                    continue;
                }

                // idle but not yet timed out warn the session is going quiet
                if (idle >= Config::IDLE_SECONDS && session->isConnected()) {
                    Context::get().logs().debug(Db::LogCategory::Session, "SESSION_IDLE",
                        std::format("id={} addr={} proto={} idle={}s",
                            id, session->getAddressStr(),
                            Common::Types::ToString(session->type()),
                            idle));
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
        // wait for 1min or if running become false
        cv_.wait_for(lock, std::chrono::minutes(1), [this] { return !running_; });
    }
}
void SessionManager::onSessionCreated(const uint64_t id, const std::string& address, const Common::Types::ServerType type)  noexcept{
    Context::get().logs().info(Db::LogCategory::Session, "SESSION_CREATED",
        std::format("id={} addr={} protocol={}", id, address, Common::Types::ToString(type)));
}
void SessionManager::onSessionCreateFailed() noexcept{
    Context::get().logs().warn(Db::LogCategory::Session, "SESSION_CREATE_FAILED", "invalid address");
}
} // namespace Raptor::Core::Server
