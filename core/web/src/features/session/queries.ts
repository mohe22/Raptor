import { useQuery } from "@tanstack/react-query";
import { getSesssionForServer } from "./api";

export const SESSION_QUERY_KEYS = {
  all: (server: string) => ["sessions", server] as const,
} as const;

const REFRESH_INTERVAL = 5 * 60 * 1000;

export const useGetSessionsForServer = (server: string) =>
  useQuery({
    queryKey: SESSION_QUERY_KEYS.all(server),
    queryFn: () => getSesssionForServer(server),
    refetchInterval: REFRESH_INTERVAL,
    enabled: !!server,
  });
