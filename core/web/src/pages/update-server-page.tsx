import { ServerIcon } from "lucide-react";
import { Link, useSearchParams } from "react-router";
import { useGetAllServers } from "../features/servers/queries";
import { Spinner } from "../components/ui/spinner";
import { Header } from "../components/shared/header";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "../components/ui/card";
import { UpdateServerForm } from "../components/forms/update-server-form";
import type { ServerInfo } from "../types/server";

import { AlertTriangleIcon } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "../components/ui/alert";

export function UpdateServerWarning() {
  return (
    <Alert className="my-2 border-amber-900 bg-amber-950 text-amber-50">
      <AlertTriangleIcon className="h-4 w-4" />
      <AlertTitle>Updating this server will cause a restart</AlertTitle>
      <AlertDescription className="mt-1 space-y-1">
        <p>Before proceeding, be aware that:</p>
        <ul className="list-disc pl-4 text-sm space-y-0.5">
          <li>
            All active sessions will be <strong>immediately dropped</strong>
          </li>
          <li>
            Connected clients will <strong>lose their connection</strong>
          </li>
          <li>
            The server will be <strong>briefly unavailable</strong> during
            restart
          </li>
          <li>
            Traffic will resume once the server is back in{" "}
            <strong>running</strong> state
          </li>
        </ul>
      </AlertDescription>
    </Alert>
  );
}

export function UpdateServerPage() {
  const [searchParams] = useSearchParams();
  const name = searchParams.get("server");
  const { data, isLoading } = useGetAllServers();

  if (isLoading)
    return (
      <div className="flex flex-1 items-center justify-center p-4">
        <Spinner />
      </div>
    );

  const server: ServerInfo = data?.find((s) => s.config.instanceName === name);
  if (!server)
    return (
      <div className="flex flex-1 flex-col items-center justify-center gap-3 p-4 text-center">
        <div className="flex h-12 w-12 items-center justify-center  bg-zinc-900 border border-zinc-800">
          <ServerIcon className="h-5 w-5 text-zinc-500" />
        </div>
        <p className="text-sm font-medium text-foreground">Server not found</p>
        <p className="text-xs text-muted-foreground font-mono">
          {name
            ? `"${name}" does not exist or was removed.`
            : "No server specified."}
        </p>
        <Link
          to="/servers"
          className="text-xs text-primary hover:underline mt-1"
        >
          Back to servers
        </Link>
      </div>
    );

  return (
    <>
      <Header
        title="Update Server"
        subtitle="Modify connection details for this endpoint."
      />
      <div className="p-2">
        <UpdateServerWarning />
        <Card className="border-zinc-800 bg-zinc-950/60 shadow-2xl shadow-black/20 backdrop-blur">
          <CardHeader className="pb-2">
            <CardTitle className="text-xl">Server Configuration</CardTitle>
          </CardHeader>
          <CardContent>
            <UpdateServerForm config={server.config} />
          </CardContent>
        </Card>
      </div>
    </>
  );
}
