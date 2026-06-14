import { Header } from "./components/shared/header";
import { Card, CardContent } from "./components/ui/card";
import { usePoolStatus } from "./features/servers/queries";
import {
  Activity,
  ArrowDownRight,
  ArrowUpRight,
  Network,
  RadioIcon,
  Users,
} from "lucide-react";
import { cn, formatBytes, formatUptime } from "./lib/utils";
import { Link } from "react-router";
import type { ServerEntry } from "./types/server";
import { Skeleton } from "./components/ui/skeleton";
import { iconMap, serverTypeConfig } from "./lib/data";
import { LogRow } from "./components/shared/log-row";

function StatCardsSkeleton() {
  return (
    <div className="grid grid-cols-2 md:grid-cols-4 gap-3 mb-4">
      {Array.from({ length: 4 }).map((_, i) => (
        <Card key={i} className="bg-card border-border">
          <CardContent className="p-4">
            <div className="flex items-center justify-between">
              <div className="space-y-2">
                <Skeleton className="h-2.5 w-16" />
                <Skeleton className="h-7 w-10" />
                <Skeleton className="h-2.5 w-12" />
              </div>
              <Skeleton className="h-10 w-10 rounded" />
            </div>
          </CardContent>
        </Card>
      ))}
    </div>
  );
}

function ServerCardsSkeleton() {
  return (
    <div className="grid gap-3 md:grid-cols-2 lg:grid-cols-4">
      {Array.from({ length: 4 }).map((_, i) => (
        <Card key={i} className="bg-card border-border">
          <CardContent className="p-4">
            <div className="flex items-start justify-between mb-3">
              <div className="flex items-center gap-2">
                <Skeleton className="h-8 w-8 rounded" />
                <div className="space-y-1.5">
                  <Skeleton className="h-3 w-20" />
                  <Skeleton className="h-2.5 w-10" />
                </div>
              </div>
              <Skeleton className="h-2 w-2 rounded-full mt-1" />
            </div>
            <Skeleton className="h-2.5 w-32 mb-3" />
            <div className="grid grid-cols-3 gap-2 text-center">
              {Array.from({ length: 3 }).map((_, j) => (
                <div key={j} className="space-y-1 flex flex-col items-center">
                  <Skeleton className="h-4 w-6" />
                  <Skeleton className="h-2 w-10" />
                </div>
              ))}
            </div>
            <div className="mt-3 space-y-1">
              <div className="flex justify-between">
                <Skeleton className="h-2 w-10" />
                <Skeleton className="h-2 w-6" />
              </div>
              <Skeleton className="h-1 w-full rounded-full" />
            </div>
          </CardContent>
        </Card>
      ))}
    </div>
  );
}

function SessionListSkeleton() {
  return (
    <Card className="bg-card border-border">
      <CardContent className="p-0">
        <div className="divide-y divide-border">
          {Array.from({ length: 6 }).map((_, i) => (
            <div key={i} className="flex items-center gap-4 px-4 py-3">
              <Skeleton className="h-2 w-2 rounded-full shrink-0" />
              <div className="flex-1 space-y-1.5">
                <div className="flex items-center gap-2">
                  <Skeleton className="h-3 w-24" />
                  <Skeleton className="h-4 w-12 rounded" />
                </div>
                <Skeleton className="h-2.5 w-48" />
              </div>
              <div className="text-right hidden sm:block space-y-1.5">
                <Skeleton className="h-2.5 w-20" />
                <Skeleton className="h-2.5 w-16" />
              </div>
            </div>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}

function DashboardSkeleton() {
  return (
    <div className="flex-1 overflow-auto p-4">
      <StatCardsSkeleton />
      <div className="mb-4">
        <div className="flex items-center justify-between mb-3">
          <Skeleton className="h-3 w-28" />
          <Skeleton className="h-3 w-12" />
        </div>
        <ServerCardsSkeleton />
      </div>
      <div>
        <div className="flex items-center justify-between mb-3">
          <Skeleton className="h-3 w-24" />
          <Skeleton className="h-3 w-10" />
        </div>
        <SessionListSkeleton />
      </div>
    </div>
  );
}

function ServerCard({ server }: { server: ServerEntry }) {
  const typeConfig = serverTypeConfig[server.type];
  const TypeIcon = iconMap[typeConfig.icon];

  return (
    <Link to={`/server/${server.name}`}>
      <Card className="bg-card border-border hover:ring-primary/50 transition-all cursor-pointer group">
        <CardContent className="p-4">
          <div className="flex items-start justify-between mb-3">
            <div className="flex items-center gap-2">
              <div className="relative h-8 w-8 rounded flex items-center justify-center bg-secondary overflow-hidden">
                {server.status === "running" && (
                  <span className="absolute inset-0 rounded bg-primary/10 animate-[pulse-ring_2s_ease-out_infinite]" />
                )}
                <TypeIcon
                  className={cn(
                    "h-4 w-4 relative z-10",
                    typeConfig.color,
                    server.status === "running" &&
                      "animate-[pulse-icon_2s_ease-in-out_infinite]",
                  )}
                />
              </div>
              <div>
                <p className="text-xs font-semibold text-foreground group-hover:text-primary transition-colors">
                  {typeConfig.label}
                </p>
                <p className="text-[10px] font-mono text-muted-foreground">
                  {server.ipAddress}:{server.port}
                </p>
              </div>
            </div>
            <span
              className={cn(
                "h-2 w-2 rounded-full mt-1",
                server.status === "running"
                  ? "status-online animate-[pulse-icon_2s_ease-in-out_infinite]"
                  : "status-offline",
              )}
            />
          </div>

          <p className="text-[10px] font-mono text-muted-foreground truncate mb-3">
            {server.name}
          </p>

          <div className="grid grid-cols-2 gap-2 text-center">
            <div>
              <p className="text-sm font-bold text-foreground">
                {server.sessionCount}
              </p>
              <p className="text-[9px] text-muted-foreground uppercase">
                Agents
              </p>
            </div>
            <div>
              <p className="text-sm font-bold text-foreground truncate">
                {formatUptime(server.startTime)}
              </p>
              <p className="text-[9px] text-muted-foreground uppercase">
                Uptime
              </p>
            </div>
          </div>

          <div className="mt-3 flex items-center justify-between text-[9px]">
            <span className="text-muted-foreground flex items-center gap-1">
              <Network className="h-2.5 w-2.5" />
              <ArrowDownRight className="h-2.5 w-2.5 text-primary " />
              {formatBytes(server.bytesReceived)}
            </span>
            <span className="text-muted-foreground flex items-center gap-1">
              <ArrowUpRight className="h-2.5 w-2.5 text-warning" />
              {formatBytes(server.bytesSent)}
            </span>
          </div>
        </CardContent>
      </Card>
    </Link>
  );
}

function App() {
  const { data, isLoading } = usePoolStatus();

  // NOW check loading
  if (!data || isLoading) return <DashboardSkeleton />;

  return (
    <>
      <Header title="Dashboard" subtitle="Command & Control Overview" />
      <div className="flex-1 overflow-auto p-4">
        <div className="grid grid-cols-2 md:grid-cols-4 gap-3 mb-4">
          <Card className="bg-card border-border">
            <CardContent className="p-4">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-[10px] uppercase tracking-wider text-muted-foreground font-semibold">
                    Listeners
                  </p>
                  <p className="text-2xl font-bold text-foreground mt-1">
                    {data.totalServerCount}
                  </p>
                  <p className="text-[10px] text-primary font-mono">
                    {data.runningServerCount
                      ? `${data.runningServerCount} Running`
                      : "0 Running"}
                  </p>
                </div>
                <div className="h-10 w-10 bg-primary/10 flex items-center justify-center">
                  <RadioIcon className="text-primary animate-[pulse-icon_2s_ease-in-out_infinite]" />
                </div>
              </div>
            </CardContent>
          </Card>

          <Card className="bg-card border-border">
            <CardContent className="p-4">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-[10px] uppercase tracking-wider text-muted-foreground font-semibold">
                    Sessions
                  </p>
                  <p className="text-2xl font-bold text-foreground mt-1">
                    {data.totalSessionCount}
                  </p>
                  <p className="text-[10px] text-primary font-mono">
                    {data.activeSessionCount
                      ? `${data.activeSessionCount} active`
                      : "0 active"}
                  </p>
                </div>
                <div className="h-10 w-10 bg-chart-2/10 flex items-center justify-center">
                  <Users className="text-chart-2 animate-[bounce-users_2.4s_ease-in-out_infinite]" />
                </div>
              </div>
            </CardContent>
          </Card>

          <Card className="bg-card border-border">
            <CardContent className="p-4">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-[10px] uppercase tracking-wider text-muted-foreground font-semibold">
                    Traffic In
                  </p>
                  <p className="text-2xl font-bold text-foreground mt-1">
                    {formatBytes(data.totalBytesReceived)}
                  </p>
                  <div className="flex items-center gap-1 text-[10px] text-primary font-mono">
                    <ArrowDownRight className="h-3 w-3" />
                    incoming
                  </div>
                </div>
                <div className="h-10 w-10 bg-chart-3/10 flex items-center justify-center">
                  <Activity className="text-chart-3 animate-[arrow-in_1.8s_ease-in-out_infinite]" />{" "}
                </div>
              </div>
            </CardContent>
          </Card>

          <Card className="bg-card border-border">
            <CardContent className="p-4">
              <div className="flex items-center justify-between">
                <div>
                  <p className="text-[10px] uppercase tracking-wider text-muted-foreground font-semibold">
                    Traffic Out
                  </p>
                  <p className="text-2xl font-bold text-foreground mt-1">
                    {formatBytes(data.totalBytesSent)}
                  </p>
                  <div className="flex items-center gap-1 text-[10px] text-warning font-mono">
                    <ArrowUpRight className="h-3 w-3" />
                    outgoing
                  </div>
                </div>
                <div className="h-10 w-10 bg-warning/10 flex items-center justify-center">
                  <Activity className="text-warning animate-[arrow-out_1.8s_ease-in-out_infinite]" />{" "}
                </div>
              </div>
            </CardContent>
          </Card>
        </div>

        <div className="mb-4">
          <div className="flex items-center justify-between mb-3">
            <h2 className="text-xs font-semibold uppercase tracking-wider text-muted-foreground">
              Active Listeners
            </h2>
            <Link
              to="/servers"
              className="text-[10px] text-primary hover:underline font-mono"
            >
              View All
            </Link>
          </div>

          <div className="grid gap-3 md:grid-cols-2 lg:grid-cols-4">
            {data.servers.map((server: ServerEntry) => (
              <ServerCard key={server.name} server={server} />
            ))}
          </div>
        </div>

        {/*Logs for session*/}
        <div>
          <div className="flex items-center justify-between mb-3">
            <h2 className="text-xs font-semibold uppercase tracking-wider text-muted-foreground">
              Recent Session
            </h2>
            <span className="text-[10px] text-muted-foreground font-mono">
              {data.totalSessionCount} total
            </span>
          </div>
          <Card className="bg-card border-border">
            <CardContent className="p-0">
              <div className="h-80 overflow-y-auto divide-y divide-border scroll-smooth">
                {[...data.sessionLogs].reverse().map((log) => (
                  <LogRow key={log.id} log={log} />
                ))}
              </div>
            </CardContent>
          </Card>
        </div>
        {/*Logs for servers*/}
        <div className="mt-4">
          <div className="flex items-center justify-between mb-3">
            <h2 className="text-xs font-semibold uppercase tracking-wider text-muted-foreground">
              Server Logs
            </h2>
            <span className="text-[10px] text-muted-foreground font-mono">
              {data.serversLogs.length} entries
            </span>
          </div>
          <Card className="bg-card border-border">
            <CardContent className="p-0">
              <div className="h-80 overflow-y-auto divide-y divide-border scroll-smooth">
                {[...data.serversLogs].reverse().map((log) => (
                  <LogRow key={log.id} log={log} />
                ))}
              </div>
            </CardContent>
          </Card>
        </div>
      </div>
    </>
  );
}

export default App;
