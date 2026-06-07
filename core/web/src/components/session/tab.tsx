import { Info, Network, PlugIcon, Shell, StarsIcon } from "lucide-react";
import { useSearchParams } from "react-router";
import { cn } from "../../lib/utils";

const navItems = [
  { label: "Interactive", icon: Shell },
  { label: "Info", icon: Info },
  { label: "Plugins", icon: PlugIcon },
  { label: "Network", icon: Network },
  { label: "AI", icon: StarsIcon },
] as const;

type TabLabel = (typeof navItems)[number]["label"];
export function Tab({ activeTab }: { activeTab: TabLabel }) {
  const [, setSearchParams] = useSearchParams();

  return (
    <div className="flex border-b border-zinc-800">
      {navItems.map(({ label, icon: Icon }) => (
        <button
          key={label}
          onClick={() => setSearchParams({ tab: label })}
          className={cn(
            "flex flex-row items-center justify-center gap-x-2 p-4 cursor-pointer w-full text-sm font-mono uppercase tracking-widest transition-all border-b-2",
            activeTab === label
              ? "text-white border-emerald-500"
              : "text-zinc-500 hover:text-zinc-400 border-transparent",
          )}
        >
          <Icon className="w-4 h-4" />
          {label}
        </button>
      ))}
    </div>
  );
}
