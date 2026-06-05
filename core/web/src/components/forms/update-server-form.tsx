import { useMemo } from "react";
import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { z } from "zod";
import { useUpdateServer } from "../../features/servers/queries";
import {
  Field,
  FieldError,
  FieldGroup,
  FieldLabel,
  FieldSet,
} from "../ui/field";
import { Input } from "../ui/input";
import { Button } from "../ui/button";
import { Spinner } from "../ui/spinner";
import type { Config } from "../../types/server";
import { useSearchParams } from "react-router";
const updateServerSchema = z.object({
  name: z
    .string()
    .min(1, "Server name is required")
    .max(50, "Maximum 50 characters"),

  port: z
    .string()
    .min(1, "Port is required")
    .refine(
      (value) => {
        const port = Number(value);
        return port >= 1 && port <= 65535;
      },
      { message: "Port must be between 1 - 65535" },
    ),

  ip: z.string().min(1, "IP address is required"),
});

export type UpdateServerFormValues = z.infer<typeof updateServerSchema>;

function detectIpType(ip: string) {
  const ipv4Regex =
    /^(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}$/;
  const ipv6Regex = /^(([0-9a-fA-F]{1,4}:){7}([0-9a-fA-F]{1,4}|:)|::1|::)$/;

  if (ipv4Regex.test(ip)) return "IPv4";
  if (ipv6Regex.test(ip)) return "IPv6";
  return "Invalid";
}

type UpdateServerFormProps = {
  config: Config;
};

export function UpdateServerForm({ config }: UpdateServerFormProps) {
  const {
    register,
    handleSubmit,
    watch,
    formState: { errors },
  } = useForm<UpdateServerFormValues>({
    resolver: zodResolver(updateServerSchema),
    defaultValues: {
      name: config.instanceName,
      port: String(config.port),
      ip: config.ip,
    },
  });

  const { mutate, isPending, error } = useUpdateServer();

  const ipValue = watch("ip");
  const ipType = useMemo(() => detectIpType(ipValue), [ipValue]);

  const [, setSearchParams] = useSearchParams();
  function handleFormSubmit(values: UpdateServerFormValues) {
    mutate(
      {
        name: values.name,
        ip: values.ip,
        port: Number(values.port),
      },
      {
        onSuccess: () => {
          setSearchParams({
            server: values.name,
          });
        },
      },
    );
  }

  return (
    <form onSubmit={handleSubmit(handleFormSubmit)} className="space-y-6">
      <FieldSet>
        <FieldGroup className="space-y-5">
          <Field data-invalid={!!errors.name}>
            <FieldLabel
              htmlFor="name"
              className="mb-2 text-xs font-medium uppercase tracking-wider text-zinc-400"
            >
              Server Name
            </FieldLabel>

            <Input
              disabled={true}
              id="name"
              type="text"
              placeholder="Primary Server"
              aria-invalid={!!errors.name}
              className="h-12 rounded-xl border-zinc-800 bg-zinc-950/70 px-4 text-sm shadow-sm transition-all focus:border-cyan-500 focus:ring-2 focus:ring-cyan-500/20"
              {...register("name")}
            />

            <FieldError errors={[errors.name]} />
          </Field>

          <div className="grid gap-5 md:grid-cols-[1fr_180px]">
            <Field data-invalid={!!errors.ip}>
              <FieldLabel
                htmlFor="ip"
                className="mb-2 text-xs font-medium uppercase tracking-wider text-zinc-400"
              >
                IP Address
              </FieldLabel>

              <div className="relative">
                <Input
                  id="ip"
                  type="text"
                  placeholder="0.0.0.0"
                  aria-invalid={!!errors.ip}
                  className="h-12 rounded-xl border-zinc-800 bg-zinc-950/70 px-4 pr-24 text-sm shadow-sm transition-all focus:border-cyan-500 focus:ring-2 focus:ring-cyan-500/20"
                  {...register("ip")}
                />

                <div
                  className={`absolute right-3 top-1/2 -translate-y-1/2 rounded-md px-2 py-1 text-xs font-medium ${
                    ipType === "IPv4" || ipType === "IPv6"
                      ? "bg-emerald-500/15 text-emerald-400"
                      : "bg-red-500/15 text-red-400"
                  }`}
                >
                  {ipType}
                </div>
              </div>

              <FieldError errors={[errors.ip]} />
            </Field>

            <Field data-invalid={!!errors.port}>
              <FieldLabel
                htmlFor="port"
                className="mb-2 text-xs font-medium uppercase tracking-wider text-zinc-400"
              >
                Port
              </FieldLabel>

              <Input
                id="port"
                type="number"
                placeholder="4444"
                aria-invalid={!!errors.port}
                className="h-12 rounded-xl border-zinc-800 bg-zinc-950/70 px-4 text-sm shadow-sm transition-all focus:border-cyan-500 focus:ring-2 focus:ring-cyan-500/20"
                {...register("port")}
              />

              <FieldError errors={[errors.port]} />
            </Field>
          </div>
        </FieldGroup>
      </FieldSet>

      {error && (
        <div
          style={{ display: "flex", alignItems: "flex-start", gap: "10px" }}
          className="p-3 rounded-xl border border-red-500/30 bg-red-500/10"
        >
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="16"
            height="16"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            className="text-red-400 mt-0.5 shrink-0"
          >
            <circle cx="12" cy="12" r="10" />
            <line x1="12" y1="8" x2="12" y2="12" />
            <line x1="12" y1="16" x2="12.01" y2="16" />
          </svg>
          <div>
            <p className="text-sm font-medium text-red-400">
              Failed to update server
            </p>
            <p className="text-sm text-red-400/80 mt-0.5">{error.message}</p>
          </div>
        </div>
      )}

      <div className="flex justify-end pt-2">
        <Button
          type="submit"
          disabled={isPending}
          className="h-11 rounded-xl px-6 font-medium text-black transition-all"
        >
          {isPending ? <Spinner /> : "Update Server"}
        </Button>
      </div>
    </form>
  );
}
