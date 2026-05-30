import { Navigate, Outlet } from "react-router";
import { useAuth } from "../../providers/auth-context";

export function AuthLayout() {
  const { userId, isLoading } = useAuth();
  if (isLoading) return null;

  if (userId) return <Navigate to="/" replace />;

  return <Outlet />;
}
