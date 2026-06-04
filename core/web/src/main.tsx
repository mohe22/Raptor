import ReactDOM from "react-dom/client";
import { BrowserRouter, Routes, Route } from "react-router";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import App from "./App";
import "./index.css";
import { LoginPage } from "./pages/login-page";
import { AuthLayout } from "./components/layout/auth-layout";
import { ProtectedRoute } from "./components/layout/protected-layout";
import { AuthProvider } from "./providers/auth-provider";
import { Toaster } from "./components/ui/sonner";
import { ServersPage } from "./pages/servers-page";
import { CreateServerPage } from "./pages/create-server-page";

const root = document.getElementById("root");
const queryClient = new QueryClient();

ReactDOM.createRoot(root).render(
  <BrowserRouter>
    <QueryClientProvider client={queryClient}>
      <AuthProvider>
        <Routes>
          <Route element={<AuthLayout />}>
            <Route path="/login" element={<LoginPage />} />
          </Route>
          <Route element={<ProtectedRoute />}>
            <Route path="/" element={<App />} />
            <Route path="/servers" element={<ServersPage />} />
            <Route path="/new-server" element={<CreateServerPage />} />
          </Route>
        </Routes>
        <Toaster duration={5000} />
      </AuthProvider>
    </QueryClientProvider>
  </BrowserRouter>,
);
