import { LoginForm } from "../components/forms/login-form";

export function LoginPage() {
  return (
    <div className="flex min-h-screen items-center justify-center">
      <div className="w-full max-w-sm space-y-6 px-4">
        <h1 className="text-lg font-medium">Sign in</h1>
        <LoginForm />
      </div>
    </div>
  );
}
