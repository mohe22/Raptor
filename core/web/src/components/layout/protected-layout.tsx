import { Navigate, Outlet } from "react-router";
import { useAuth } from "../../providers/auth-context";
import { Spinner } from "../ui/spinner";
export function ProtectedRoute() {
  const { userId, isLoading, error } = useAuth();
  console.log(error);
  if (isLoading) {
    return (
      <div className="flex justify-center items-center h-screen">
        <Spinner />
      </div>
    );
  }

  if (!userId) return <Navigate to="/login" replace />;

  return (
    // <SocketProvider>
    <div className="flex">
      {/*<Sidebar />*/}
      <main className="flex flex-1 flex-col overflow-hidden">
        <Outlet />
      </main>
    </div>
    // </SocketProvider>
  );
}
