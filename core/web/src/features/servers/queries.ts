import { useQuery } from "@tanstack/react-query";
import { getAllServers } from "./api";

export const SERVER_QUERY_KEYS = {
  all: ["servers"] as const,
  byId: (id: string) => ["servers", id] as const,
} as const;
const REFRESH_INTERVAL = 5 * 60 * 1000;
export const useGetAllServers = () =>
  useQuery({
    queryKey: SERVER_QUERY_KEYS.all,
    queryFn: getAllServers,
    refetchInterval: REFRESH_INTERVAL,
  });
