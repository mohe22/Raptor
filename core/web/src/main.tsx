import ReactDOM from "react-dom/client";
import { BrowserRouter, Routes, Route } from "react-router";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import App from "./App";
import "./index.css";
import { LoginPage } from "./pages/login-page";
import { AuthLayout } from "./components/layout/auth-layout";
import { ProtectedRoute } from "./components/layout/protected-layout";
import { AuthProvider } from "./providers/auth-provider";

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
          </Route>
        </Routes>
      </AuthProvider>
    </QueryClientProvider>
  </BrowserRouter>,
);
