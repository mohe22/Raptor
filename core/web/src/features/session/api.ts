import { api } from "../../lib/api";
import type { Session } from "../../types/session";

export const getSesssionForServer = async (server: string) =>
  await api<Session[]>(`/sessions/get-sessions/${server}`, {
    method: "GET",
  });
