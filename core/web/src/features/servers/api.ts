import { api } from "../../lib/api";
import type { getAllServersResponse } from "../../types/server";

export const getAllServers = async () =>
  await api<getAllServersResponse>("/server/servers", {
    method: "GET",
  });
