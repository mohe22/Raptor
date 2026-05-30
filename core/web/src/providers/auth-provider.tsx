import { useMe } from "../features/auth/queries";
import { AuthCtx } from "./auth-context";

function deleteTokenCookie() {
  document.cookie = "token=; path=/; max-age=0";
}
export function AuthProvider({ children }: { children: React.ReactNode }) {
  const { data, isLoading, status } = useMe();

  console.log(status, isLoading);

  function logout() {
    deleteTokenCookie();
    window.location.href = "/login";
  }

  return (
    <AuthCtx.Provider
      value={{ userId: data?.userId ?? null, isLoading, logout }}
    >
      <main className="w-screen h-screen bg-background antialiased font-sans">
        {children}
      </main>
    </AuthCtx.Provider>
  );
}
