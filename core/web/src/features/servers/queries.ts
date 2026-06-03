import { useQuery } from "@tanstack/react-query";
import { getAllServers, poolStatus } from "./api";

export const SERVER_QUERY_KEYS = {
  all: ["servers"] as const,
  byId: (id: string) => ["servers", id] as const,
  poolStatus: ["servers", "pool-status"] as const,
} as const;

const REFRESH_INTERVAL_MS = 2 * 60 * 1000; // 2 min
const REFRESH_INTERVAL_MAX_MS = 5 * 60 * 1000; // 5 min

export const useGetAllServers = () =>
  useQuery({
    queryKey: SERVER_QUERY_KEYS.all,
    queryFn: getAllServers,
    refetchInterval: REFRESH_INTERVAL_MS,
    staleTime: REFRESH_INTERVAL_MS,
  });

export const usePoolStatus = () =>
  useQuery({
    queryKey: SERVER_QUERY_KEYS.poolStatus,
    queryFn: poolStatus,
    refetchInterval: REFRESH_INTERVAL_MAX_MS,
    staleTime: REFRESH_INTERVAL_MAX_MS,
  });
