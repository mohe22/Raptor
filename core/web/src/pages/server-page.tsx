import { Link, useParams } from "react-router";
import { useGetServerById } from "../features/servers/queries";
import { Header } from "../components/shared/header";
import { useState, useEffect } from "react";
import { Button } from "../components/ui/button";
import {
  ArrowLeft,
  MoreVertical,
  Network,
  Pencil,
  Clock,
  TrendingUp,
} from "lucide-react";
import { SERVER_STATUS_DOT } from "../lib/data";
import { cn, formatBytes, formatUptime } from "../lib/utils";
import { PauseServerButton } from "../components/buttons/pause-server-button";
import { ResumeServerButton } from "../components/buttons/resume-server-button";
import {
  Popover,
  PopoverContent,
  PopoverTrigger,
} from "../components/ui/popover";
import { StopServerButton } from "../components/buttons/stop-server.button";
import { AlertCircleIcon } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "../components/ui/alert";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "../components/ui/card";
import { TrafficGraph } from "../components/shared/trafficGraph";

type Tab = "Overview" | "Session" | "Logs";

interface TrafficPoint {
  time: number;
  rx: number;
  tx: number;
}

export function ServerPage() {
  const { serverId } = useParams<{ serverId: string }>();
  const { data, isLoading, error } = useGetServerById(serverId);

  const [tab, setTab] = useState<Tab>("Overview");
  const [trafficHistory, setTrafficHistory] = useState<TrafficPoint[]>([]);
  const [lastRx, setLastRx] = useState(0);
  const [lastTx, setLastTx] = useState(0);

  useEffect(() => {
    if (!data) return;

    const now = Date.now();
    const newRx = data.rxBytes || 0;
    const newTx = data.txBytes || 0;

    const deltaRx = Math.max(0, newRx - lastRx);
    const deltaTx = Math.max(0, newTx - lastTx);

    setTrafficHistory((prev) => {
      let updated = [...prev, { time: now, rx: deltaRx, tx: deltaTx }];
      if (updated.length > 60) updated = updated.slice(-60);
      return updated;
    });

    setLastRx(newRx);
    setLastTx(newTx);
  }, [data?.rxBytes, data?.txBytes, data?.uptimeSeconds]);

  if (error) {
    return (
      <>
        <Header title="Error" subtitle="Could not load server" />
        <div className="flex-1 flex items-center justify-center p-4">
          <p className="text-xs text-destructive font-mono">{error.message}</p>
        </div>
      </>
    );
  }

  if (isLoading || !data) {
    return (
      <>
        <Header title="Loading server..." subtitle="" />
        <div className="flex-1 flex items-center justify-center p-4">
          <div className="animate-pulse text-muted-foreground">
            Loading server details...
          </div>
        </div>
      </>
    );
  }

  return (
    <>
      <Header
        title={data.config.instanceName}
        subtitle={`${data.type} • ${data.config.ip}:${data.config.port} • ${data.config.ipType}`}
      />

      <div className="flex-1 overflow-auto p-6 space-y-6">
        {/* Status Bar */}
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-4">
            <Link to="/servers">
              <Button
                variant="ghost"
                size="sm"
                className="gap-2 text-muted-foreground hover:text-foreground"
              >
                <ArrowLeft className="h-4 w-4" />
                Back to Servers
              </Button>
            </Link>

            <div className="flex items-center gap-3">
              <div className="flex items-center gap-2.5">
                <div
                  className={cn(
                    "h-2.5 w-2.5 rounded-full animate-pulse",
                    SERVER_STATUS_DOT[data.status],
                  )}
                />
                <span className="font-mono uppercase text-sm tracking-widest font-semibold text-foreground">
                  {data.status}
                </span>
              </div>
              <div className="flex items-center gap-1.5 text-muted-foreground text-sm font-mono">
                <Clock className="h-3.5 w-3.5" />
                {formatUptime(data.uptimeSeconds)}
              </div>
            </div>
          </div>

          <div className="flex items-center gap-3">
            {data.status === "running" ? (
              <PauseServerButton name={data.config.instanceName} />
            ) : (
              <ResumeServerButton name={data.config.instanceName} />
            )}
            <Popover>
              <PopoverTrigger>
                <Button size="icon" variant="ghost" className="h-9 w-9">
                  <MoreVertical className="h-4 w-4" />
                </Button>
              </PopoverTrigger>
              <PopoverContent className="w-40 p-1" align="end">
                <Link
                  to={`/update-server?server=${encodeURIComponent(data.config.instanceName)}`}
                  className="flex items-center gap-2 px-3 py-2 text-sm hover:bg-zinc-800 rounded cursor-pointer"
                >
                  <Pencil className="h-4 w-4" />
                  Update Server
                </Link>
                <StopServerButton name={data.config.instanceName} />
              </PopoverContent>
            </Popover>
          </div>
        </div>

        {data.error && (
          <Alert variant="destructive" className="border-red-900">
            <AlertCircleIcon className="h-4 w-4" />
            <AlertTitle>Server Error</AlertTitle>
            <AlertDescription>{data.error}</AlertDescription>
          </Alert>
        )}

        {/* Stats Cards */}
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
          <Card>
            <CardContent className="p-6">
              <div className="flex justify-between items-start">
                <div>
                  <p className="text-xs uppercase tracking-widest text-zinc-500">
                    SESSIONS
                  </p>
                  <p className="text-4xl font-bold font-mono mt-3 tabular-nums">
                    {data.sessionCounter}
                  </p>
                </div>
                <Network className="h-9 w-9 text-zinc-600" />
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="p-6">
              <div className="flex justify-between items-start">
                <div>
                  <p className="text-xs uppercase tracking-widest text-zinc-500">
                    RECEIVED
                  </p>
                  <p className="text-4xl font-bold font-mono mt-3 tabular-nums text-emerald-400">
                    {formatBytes(data.rxBytes)}
                  </p>
                </div>
                <div className="text-emerald-500 text-3xl">↓</div>
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="p-6">
              <div className="flex justify-between items-start">
                <div>
                  <p className="text-xs uppercase tracking-widest text-zinc-500">
                    SENT
                  </p>
                  <p className="text-4xl font-bold font-mono mt-3 tabular-nums text-blue-400">
                    {formatBytes(data.txBytes)}
                  </p>
                </div>
                <div className="text-blue-500 text-3xl">↑</div>
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardContent className="p-6">
              <div className="flex justify-between items-start">
                <div>
                  <p className="text-xs uppercase tracking-widest text-zinc-500">
                    UPTIME
                  </p>
                  <p className="text-4xl font-bold font-mono mt-3 tabular-nums">
                    {formatUptime(data.uptimeSeconds)}
                  </p>
                </div>
                <Clock className="h-9 w-9 text-zinc-600" />
              </div>
            </CardContent>
          </Card>
        </div>

        {/* Tabs */}
        <div className="flex border-b border-zinc-800">
          {(["Overview", "Session", "Logs"] as Tab[]).map((t) => (
            <button
              key={t}
              onClick={() => setTab(t)}
              className={cn(
                "px-8 py-4 text-sm font-mono uppercase tracking-widest transition-all border-b-2",
                tab === t
                  ? "text-white border-emerald-500"
                  : "text-zinc-500 hover:text-zinc-400 border-transparent",
              )}
            >
              {t}
            </button>
          ))}
        </div>

        {/* Tab Content */}
        {tab === "Overview" && (
          <div className="space-y-8">
            <Card>
              <CardHeader className="flex flex-row items-center justify-between border-b border-zinc-800 pb-4">
                <div className="flex items-center gap-3">
                  <TrendingUp className="h-5 w-5 text-emerald-500" />
                  <CardTitle className="text-lg">Traffic History</CardTitle>
                </div>
                <div className="text-xs text-zinc-500 font-mono">
                  Last 60 updates • Real-time
                </div>
              </CardHeader>
              <CardContent className="p-6 pt-2">
                <TrafficGraph
                  trafficHistory={trafficHistory}
                  currentRx={data.rxBytes}
                  currentTx={data.txBytes}
                />
              </CardContent>
            </Card>

            {/* Configuration */}
            <Card>
              <CardHeader>
                <CardTitle className="text-lg">Configuration</CardTitle>
              </CardHeader>
              <CardContent className="grid grid-cols-1 md:grid-cols-3 gap-8 text-sm">
                <div>
                  <p className="text-zinc-500 text-xs uppercase tracking-widest mb-1">
                    IP TYPE
                  </p>
                  <p className="font-mono text-lg">{data.config.ipType}</p>
                </div>
                <div>
                  <p className="text-zinc-500 text-xs uppercase tracking-widest mb-1">
                    ADDRESS
                  </p>
                  <p className="font-mono text-lg break-all">
                    {data.config.ip}:{data.config.port}
                  </p>
                </div>
                <div>
                  <p className="text-zinc-500 text-xs uppercase tracking-widest mb-1">
                    TIMEOUT
                  </p>
                  <p className="font-mono text-lg">{data.config.timeout}s</p>
                </div>
              </CardContent>
            </Card>
          </div>
        )}

        {tab === "Session" && (
          <div className="h-96 flex items-center justify-center text-zinc-500"></div>
        )}
        {tab === "Logs" && (
          <div className="h-96 flex items-center justify-center text-zinc-500">
            Logs coming soon
          </div>
        )}
      </div>
    </>
  );
}
