import { api } from "../../lib/api";
import type { LoginResponse } from "../../types/auth";

export const getMe = async (): Promise<{ userId: string } | null> => {
  try {
    return await api("/auth/me", { method: "GET" });
  } catch {
    return null; // unauthenticated
  }
};

export const loginUser = (username: string, password: string) =>
  api<LoginResponse>("/auth/login", {
    method: "POST",
    body: JSON.stringify({ username, password }),
  });
