import { useQuery } from "@tanstack/react-query";
import { getSessionById, getSesssionForServer } from "./api";

export const SESSION_QUERY_KEYS = {
  all: (server: string) => ["sessions", server] as const,
  detail: (server: string, sessionId: string) =>
    ["sessions", server, sessionId] as const,
} as const;

const REFRESH_INTERVAL = 5 * 60 * 1000;

export const useGetSessionsForServer = (server: string) =>
  useQuery({
    queryKey: SESSION_QUERY_KEYS.all(server),
    queryFn: () => getSesssionForServer(server),
    refetchInterval: REFRESH_INTERVAL,
    enabled: !!server,
  });

export const useGetSessionById = (server: string, sessionId: string) =>
  useQuery({
    queryKey: SESSION_QUERY_KEYS.detail(server, sessionId),
    queryFn: () => getSessionById(server, sessionId),
    refetchInterval: REFRESH_INTERVAL,
    enabled: !!server && !!sessionId,
  });
