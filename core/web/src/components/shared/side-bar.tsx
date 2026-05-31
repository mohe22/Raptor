import React, { useEffect, useMemo, useState } from "react";
import { cn } from "../../lib/utils";
import {
  ChevronLeft,
  ChevronRightIcon,
  ChevronDown,
  ChevronRight,
  Terminal,
  Server,
  Activity,
} from "lucide-react";
import { LayoutDashboard, PlugIcon, ServerIcon } from "lucide-react";
import { Link, useLocation } from "react-router";
import { useGetAllServers } from "../../features/servers/queries";
import { toast } from "sonner";
import type { ServerInfo } from "../../types/server";

const navItems = [
  { href: "/", label: "Dashboard", icon: LayoutDashboard },
  { href: "/servers", label: "Servers", icon: ServerIcon },
  { href: "/plugins", label: "Plugins", icon: PlugIcon },
];

declare const __APP_VERSION__: string;

const Sidebar: React.FC = () => {
  const location = useLocation();
  const pathname = location.pathname;
  const [isCollapsed, setIsCollapsed] = useState(false);
  const [expandedServerId, setExpandedServerId] = useState<string | null>(null);

  const { data, isLoading, error } = useGetAllServers();
  const servers: ServerInfo[] = data ?? [];
  console.log(servers);
  useEffect(() => {
    if (error) {
      toast.error("Failed to fetch servers", {
        description: error.message,
      });
    }
  }, [error]);

  const totalSessions = useMemo(
    () => servers.reduce((acc, s) => acc + s.sessionCounter, 0),
    [servers],
  );

  const onlineCount = useMemo(
    () => servers.filter((s) => s.status === "running").length,
    [servers],
  );

  return (
    <aside
      className={cn(
        "flex h-screen relative group flex-col border-r border-border bg-sidebar transition-all duration-300",
        isCollapsed ? "w-16" : "w-32 md:w-72",
      )}
    >
      <div className="flex h-14 items-center gap-2.5 border-b border-sidebar-border px-4 relative">
        <div className="flex h-8 w-8 items-center justify-center bg-primary shrink-0">
          <Terminal className="h-4 w-4 text-primary-foreground" />
        </div>

        {!isCollapsed && (
          <div className="flex flex-col">
            <span className="text-sm font-semibold tracking-tight text-foreground">
              C2 Raptor
            </span>
            <span className="font-mono text-[10px] text-muted-foreground">
              v{__APP_VERSION__}
            </span>
          </div>
        )}

        <button
          onClick={() => setIsCollapsed(!isCollapsed)}
          className="absolute hidden group-hover:block -right-3 top-4 bg-sidebar border border-border p-1.5 shadow-md hover:bg-sidebar-accent transition-colors"
        >
          {isCollapsed ? (
            <ChevronRightIcon className="h-4 w-4" />
          ) : (
            <ChevronLeft className="h-4 w-4" />
          )}
        </button>
      </div>
      {/* Status Bar */}
      {!isCollapsed && (
        <div className="flex items-center justify-between border-b border-sidebar-border bg-sidebar-accent/30 px-4 py-2">
          <div className="flex items-center gap-1.5">
            <Activity className="h-3 w-3 text-primary" />
            <span className="font-mono text-[10px] text-muted-foreground">
              {totalSessions} <span className="max-md:hidden">agents</span>
            </span>
          </div>
          <div className="flex items-center gap-1.5">
            <span className="status-online h-1.5 w-1.5 rounded-full" />
            <span className="font-mono text-[10px] text-muted-foreground">
              {onlineCount} <span className="max-md:hidden">online</span>
            </span>
          </div>
        </div>
      )}
      <nav className="flex-1 overflow-y-auto p-2">
        <div className="mb-4">
          {!isCollapsed && (
            <p className="mb-1.5 px-2 text-[10px] font-semibold uppercase tracking-widest text-muted-foreground">
              Navigation
            </p>
          )}
          {navItems.map((item) => {
            const Icon = item.icon;
            const isActive =
              pathname === item.href || pathname.startsWith(item.href + "/");

            return (
              <Link
                key={item.href}
                to={item.href}
                className={cn(
                  "flex items-center cursor-pointer gap-2.5 px-2.5 py-2 text-[13px] font-medium transition-all rounded-md",
                  isActive
                    ? "bg-primary/10 text-primary"
                    : "text-muted-foreground hover:bg-sidebar-accent hover:text-foreground",
                  isCollapsed && "justify-center",
                )}
                title={isCollapsed ? item.label : undefined}
              >
                <Icon className="h-4 w-4" />
                {!isCollapsed && item.label}
              </Link>
            );
          })}
        </div>

        <div>
          {!isCollapsed && (
            <p className="mb-1.5 px-2 text-[10px] font-semibold uppercase tracking-widest text-muted-foreground">
              Servers
            </p>
          )}

          {isLoading && !isCollapsed && (
            <p className="px-2 text-[11px] text-muted-foreground">Loading...</p>
          )}

          {servers.map((server) => {
            const isExpanded = expandedServerId === server.config.instanceName;

            return (
              <div key={server.config.instanceName}>
                <button
                  onClick={() =>
                    !isCollapsed &&
                    setExpandedServerId(
                      isExpanded ? null : server.config.instanceName,
                    )
                  }
                  className={cn(
                    "flex w-full items-center gap-2 px-2.5 py-1.5 text-[13px] font-medium transition-all rounded-md",
                    isExpanded && !isCollapsed
                      ? "bg-sidebar-accent text-foreground"
                      : "text-muted-foreground hover:bg-sidebar-accent hover:text-foreground",
                    isCollapsed && "justify-center",
                  )}
                  title={isCollapsed ? server.config.instanceName : undefined}
                >
                  <Server className="h-3.5 w-3.5" />

                  {!isCollapsed && (
                    <>
                      <span className="flex-1 truncate text-left font-mono text-xs">
                        {server.config.instanceName}
                      </span>
                      <span
                        className={cn(
                          "h-1.5 w-1.5 shrink-0 rounded-full",
                          server.status === "running"
                            ? "bg-green-500"
                            : server.status === "error"
                              ? "bg-destructive"
                              : "bg-muted-foreground",
                        )}
                      />
                      <span className="font-mono text-[10px] text-muted-foreground">
                        {server.sessionCounter}
                      </span>
                      {isExpanded ? (
                        <ChevronDown className="h-3.5 w-3.5 text-muted-foreground" />
                      ) : (
                        <ChevronRight className="h-3.5 w-3.5 text-muted-foreground" />
                      )}
                    </>
                  )}
                </button>
              </div>
            );
          })}
        </div>
      </nav>
    </aside>
  );
};

export default Sidebar;
