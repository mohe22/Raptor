import { api } from "../../lib/api";

export const getMe = async (): Promise<{ userId: string } | null> => {
  try {
    return await api("/auth/me", { method: "GET" });
  } catch {
    return null;
  }
};
