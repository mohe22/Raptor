import { Search, Users } from "lucide-react";
import { Input } from "../ui/input";
import type React from "react";
// import { cn } from "../../lib/utils";
// import { Input } from "@/components/ui/input";
// import { useSocket } from "@/hooks/use-socket";
// import { cn } from "@/lib/utils";

interface HeaderProps {
  title: string;
  subtitle?: string;
  icon?: string | React.ReactElement;
}

export function Header({ title, subtitle, icon }: HeaderProps) {
  // const { status } = useSocket();
  return (
    <header className="flex h-14 items-center justify-between border-b border-border bg-card/50 px-6">
      <div className="flex items-center gap-3">
        <div>
          <div className="flex items-center gap-2">
            {icon}
            <h1 className="text-sm font-semibold text-foreground">{title}</h1>
          </div>
          {subtitle && (
            <p className="text-[11px] font-mono text-muted-foreground">
              {subtitle}
            </p>
          )}
        </div>
      </div>

      <div className="flex items-center gap-3">
        <div className="relative hidden md:block">
          <Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-muted-foreground" />
          <Input
            placeholder="Search agents, commands..."
            className="w-56 bg-secondary/50 pl-8 h-8 text-xs border-0 focus-visible:ring-1"
          />
        </div>

        <div className="flex items-center gap-2 border-l border-border pl-3">
          {/*<span
            className={cn("h-2 w-2 rounded-full status-online", {
              "status-offline": status !== "connected",
            })}
          />*/}
          {/*<span className="text-[11px] font-mono text-muted-foreground">*/}
          {/*{status === "connected" ? "Connected" : "Disconnected"}*/}
          {/*</span>*/}
        </div>
      </div>
    </header>
  );
}
