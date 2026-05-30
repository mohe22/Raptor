import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import {
  Field,
  FieldDescription,
  FieldError,
  FieldGroup,
  FieldLabel,
  FieldSet,
} from "../ui/field";
import { Input } from "../ui/input";
import { Button } from "../ui/button";
import { Spinner } from "../ui/spinner";
import { useLogin } from "../../features/auth/queries";

const loginSchema = z.object({
  username: z
    .string()
    .min(1, "Username is required")
    .max(32, "Username must be 32 characters or less"),
  password: z.string().min(1, "Password is required"),
});

export type LoginFormValues = z.infer<typeof loginSchema>;

export function LoginForm() {
  const { mutate, isPending, error } = useLogin();

  const {
    register,
    handleSubmit,
    formState: { errors },
  } = useForm<LoginFormValues>({
    resolver: zodResolver(loginSchema),
    defaultValues: { username: "", password: "" },
  });

  function handleFormSubmit(values: LoginFormValues) {
    mutate({ username: values.username, password: values.password });
  }

  return (
    <form onSubmit={handleSubmit(handleFormSubmit)} className="space-y-6">
      <FieldSet>
        <FieldGroup>
          <Field data-invalid={!!errors.username}>
            <FieldLabel htmlFor="username">Username</FieldLabel>
            <Input
              id="username"
              type="text"
              placeholder="johndoe"
              aria-invalid={!!errors.username}
              autoComplete="username"
              {...register("username")}
            />
            <FieldError errors={[errors.username]} />
          </Field>

          <Field data-invalid={!!errors.password}>
            <FieldLabel htmlFor="password">Password</FieldLabel>
            <Input
              id="password"
              type="password"
              placeholder="••••••••"
              aria-invalid={!!errors.password}
              autoComplete="current-password"
              {...register("password")}
            />
            <FieldDescription>
              Credentials are issued by your server administrator.
            </FieldDescription>
            <FieldError errors={[errors.password]} />
          </Field>
        </FieldGroup>
      </FieldSet>

      {error && (
        <div className="border border-destructive/30 bg-destructive/10 px-3 py-2 text-sm text-destructive">
          {error.message}
        </div>
      )}

      <div className="flex justify-end">
        <Button type="submit" disabled={isPending}>
          {isPending ? <Spinner /> : "Sign in"}
        </Button>
      </div>
    </form>
  );
}
