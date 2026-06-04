import { Link } from "react-router";
import { useGetAllServers } from "../features/servers/queries";
import {
  Empty,
  EmptyContent,
  EmptyDescription,
  EmptyHeader,
  EmptyMedia,
  EmptyTitle,
} from "../components/ui/empty";
import { Plus } from "lucide-react";
import { Button } from "../components/ui/button";
import { Header } from "../components/shared/header";
import {
  ServerCard,
  ServerCardSkeleton,
} from "../components/shared/server-card";

function EmptyServer() {
  return (
    <Link to="/new-server">
      <Empty className="border border-dashed cursor-pointer transition-all duration-200 hover:shadow-[0_0_20px_rgba(255,255,255,0.08)] hover:border-white/20">
        <EmptyHeader>
          <EmptyMedia variant="icon">
            <Plus className="h-4 w-4" />
          </EmptyMedia>

          <EmptyTitle>Another server?!</EmptyTitle>

          <EmptyDescription>
            Create a server to start handling listeners, agents, and traffic
            logs.
          </EmptyDescription>
        </EmptyHeader>

        <EmptyContent>
          <Button
            variant="outline"
            size="sm"
            className="gap-1.5 cursor-pointer"
          >
            <Plus className="h-3.5 w-3.5" />
            Create server
          </Button>
        </EmptyContent>
      </Empty>
    </Link>
  );
}

export function ServersPage() {
  const { data: servers, isLoading, error } = useGetAllServers();
  return (
    <>
      <Header title="Listeners" subtitle="Active infrastructure endpoints" />
      <div className="flex-1 overflow-auto p-2">
        <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
          {isLoading &&
            Array.from({ length: 4 }).map((_, i) => (
              <ServerCardSkeleton key={i} />
            ))}

          {error && (
            <p className="text-xs text-red-400 col-span-full">
              Failed to load servers.
            </p>
          )}
          {!isLoading &&
            !error &&
            servers?.map((server) => (
              <ServerCard key={server.config.instanceName} server={server} />
            ))}
          <EmptyServer />
        </div>
      </div>
    </>
  );
}
