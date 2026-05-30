import { createContext, useContext } from "react";

export type AuthContextValue = {
  userId: string | null;
  isLoading: boolean;
  logout: () => void;
};

export const AuthCtx = createContext<AuthContextValue>({
  userId: null,
  isLoading: true,
  logout: () => {},
});

export const useAuth = () => useContext(AuthCtx);
