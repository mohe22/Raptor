import { api } from "../../lib/api";
import type {
  getAllServersResponse,
  ServerPoolStatus,
} from "../../types/server";

export const getAllServers = async () =>
  await api<getAllServersResponse>("/server/get-servers", {
    method: "GET",
  });
export const poolStatus = async () =>
  await api<ServerPoolStatus>("/server/pool-status", { method: "GET" });
