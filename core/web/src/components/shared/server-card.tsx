import { Link } from "react-router";
import { iconMap, SERVER_STATUS_DOT, serverTypeConfig } from "../../lib/data";
import type { ServerInfo } from "../../types/server";
import { Card, CardContent } from "../ui/card";
import { cn, formatBytes } from "../../lib/utils";
import { Skeleton } from "../ui/skeleton";
import { PauseServerButton } from "../buttons/pause-server-button";
import { ResumeServerButton } from "../buttons/resume-server-button";
import { Popover, PopoverContent, PopoverTrigger } from "../ui/popover";
import { Button } from "../ui/button";
import { MoreVertical, Pencil } from "lucide-react";
import { StopServerButton } from "../buttons/stop-server.button";

export function ServerCardSkeleton() {
  return (
    <Card className="bg-card border-border">
      <CardContent className="p-4">
        <div className="flex items-start justify-between mb-3">
          <div className="flex items-center gap-2">
            <Skeleton className="h-8 w-8 rounded" />
            <div className="space-y-1.5">
              <Skeleton className="h-3 w-24" />
              <Skeleton className="h-2.5 w-10" />
            </div>
          </div>
          <Skeleton className="h-2 w-2 rounded-full mt-1" />
        </div>
        <Skeleton className="h-2.5 w-36 mb-3" />
        <div className="grid grid-cols-3 gap-2">
          {Array.from({ length: 3 }).map((_, i) => (
            <div key={i} className="flex flex-col items-center gap-1">
              <Skeleton className="h-5 w-8" />
              <Skeleton className="h-2 w-12" />
            </div>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}
export function ServerCard({ server }: { server: ServerInfo }) {
  const typeConfig = serverTypeConfig[server.type];
  const TypeIcon = iconMap[typeConfig?.icon ?? "server"];
  const statusDot = SERVER_STATUS_DOT[server.status];

  return (
    <Card className="bg-card border-border hover:ring-1 hover:ring-primary/50 transition-all cursor-pointer group">
      <CardContent className="p-4">
        <Link to={`/servers/${server.config.instanceName}`}>
          <div className="flex items-start justify-between mb-3">
            <div className="flex items-center gap-2">
              <div className="h-8 w-8 rounded flex items-center justify-center bg-secondary">
                <TypeIcon className={cn("h-4 w-4", typeConfig?.color)} />
              </div>
              <div>
                <p className="text-xs font-semibold text-foreground group-hover:text-primary transition-colors font-mono">
                  {server.config.instanceName}
                </p>
                <p className="text-[10px] text-muted-foreground font-mono">
                  {server.type}
                </p>
              </div>
            </div>
            <span className={cn("h-2 w-2 rounded-full mt-1", statusDot)} />
          </div>

          <p className="text-[10px] font-mono text-muted-foreground mb-3">
            {server.config.ip}:{server.config.port} • {server.config.ipType}
          </p>

          <div className="grid grid-cols-3 gap-2 text-center">
            <div>
              <p className="text-sm font-bold text-foreground">
                {server.sessionCounter}
              </p>
              <p className="text-[9px] text-muted-foreground uppercase">
                Sessions
              </p>
            </div>
            <div>
              <p className="text-sm font-bold text-foreground">
                {formatBytes(server.rxBytes)}
              </p>
              <p className="text-[9px] text-muted-foreground uppercase">RX</p>
            </div>
            <div>
              <p className="text-sm font-bold text-foreground">
                {formatBytes(server.txBytes)}
              </p>
              <p className="text-[9px] text-muted-foreground uppercase">TX</p>
            </div>
          </div>

          {server.error && (
            <p className="mt-2 text-[10px] text-destructive font-mono truncate">
              {server.error}
            </p>
          )}
        </Link>

        <div className="mt-3 flex items-center justify-between">
          {server.status === "running" ? (
            <PauseServerButton name={server.config.instanceName} />
          ) : (
            <ResumeServerButton name={server.config.instanceName} />
          )}

          <Popover>
            <PopoverTrigger>
              <Button size="icon-sm" className="cursor-pointer">
                <MoreVertical className="size-3.5" />
              </Button>
            </PopoverTrigger>
            <PopoverContent className="w-36 p-1">
              <div className="flex flex-col">
                <Link
                  to={`/update-server?server=${encodeURIComponent(server.config.instanceName)}`}
                  className="flex items-center gap-2 rounded px-2 py-1.5 text-xs text-muted-foreground hover:bg-secondary hover:text-foreground w-full"
                >
                  <Pencil className="size-3.5" /> Update
                </Link>
                <StopServerButton name={server.config.instanceName} />
              </div>
            </PopoverContent>
          </Popover>
        </div>
      </CardContent>
    </Card>
  );
}
